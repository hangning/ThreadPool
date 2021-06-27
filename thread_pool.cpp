#include "thread_pool.h"
#include <utility>
#include <iostream>
#include <type_traits>

#define CLAMP(num, min, max) \
   if (num < min) num = min; \
   if (num > max) num = max;

unsigned ThreadPool::s_hardware_core_num = std::thread::hardware_concurrency();

ThreadPool::ThreadPool(int num) {
  stop_.store(false);
  idel_thread_.store(0);

  CLAMP(num, THREAD_NUM_MIN, s_hardware_core_num);
  Init(num);
}

ThreadPool::~ThreadPool() {
  stop_.store(true);
  cond_.notify_all();
  if (schedule_thread_.joinable())
    schedule_thread_.join();

  for(auto& thread : threads_) {
    if (thread.joinable())
     thread.join();
  }
}

void ThreadPool::Init(int num) {
  std::lock_guard<std::mutex> lock(mutex_);
  stop_ = false;

  schedule_thread_ = std::move(std::thread(
    [this] () {
      while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (delay_tasks_.size()) {  //This Thread need finished all task, then can destroy
          DelayTask top = delay_tasks_.top();
          if (top.fire_time_ <= std::chrono::steady_clock::now()) {
            delay_tasks_.pop();
            high_proirty_tasks_.push(std::move(top.task_));
            AddThreadIfNeeded(); 
            cond_.notify_one();
            continue;
          }

          timer_cond_.wait_until(lock, top.fire_time_);
        } else if (stop_) {
          break;
        } else {
          timer_cond_.wait(lock);  
        }
      }
    }
  ));

  while(num--) {
    AddThreadImpl();
  }
}

void ThreadPool::AddThreadIfNeeded() {
  if (threads_.size() < s_hardware_core_num && 
        (tasks_.size() / threads_.size() >2 || 
          high_proirty_tasks_.size() > idel_thread_)){  
    AddThreadImpl();
  }
}


void ThreadPool::AddThreadImpl() {
  threads_.emplace_back([this](){
      while(true) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (high_proirty_tasks_.size()) {
          auto task = std::move(high_proirty_tasks_.front());
          high_proirty_tasks_.pop();
          lock.unlock();
          task();
        } else
        
        
         if  (tasks_.size()) {
          auto task = std::move(tasks_.front());
          tasks_.pop();
          lock.unlock();
          task();
        } else if(stop_) {
          lock.unlock();
          break;
        } else {  // tasks_.empty()
          idel_thread_.fetch_add(1);

          log::getInstance()->print("idel_thread_: ", idel_thread_);

          cond_.wait(lock, [&]() {return stop_ || high_proirty_tasks_.size() || tasks_.size();});
          idel_thread_.fetch_sub(1);
        }
      }
  });
  log::getInstance()->
    print("ThreadPool AddThread now Thread_Num is :", threads_.size());
}



