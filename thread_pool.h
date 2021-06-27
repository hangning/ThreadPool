#pragma once
#include <thread>
#include <mutex>
#include <vector>
#include <future>
#include <queue>
#include <condition_variable>
#include <functional>
#include <type_traits>
#include <iostream>
#include "log.h"
#define THREAD_NUM_MIN 4

typedef std::chrono::time_point<std::chrono::steady_clock,
    std::chrono::milliseconds> Clock;

class DelayTask {
  public:
  DelayTask(std::function<void()> task, std::chrono::milliseconds delay) {
    task_ = std::move(task);
    fire_time_ = std::chrono::steady_clock::now() + delay;
    delay_ = delay;
  }

  bool operator > (const DelayTask& other) const {
    return this->fire_time_ > other.fire_time_;
  }

  bool operator < (const DelayTask& other) const {
    return this->fire_time_ < other.fire_time_;
  }
  std::function<void()> task_;
  std::chrono::steady_clock::time_point fire_time_;
  std::chrono::milliseconds delay_;
};

class ThreadPool {
public:
  ThreadPool(int num = THREAD_NUM_MIN);
  ~ThreadPool();

  ThreadPool(ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;


  template<typename Function, typename... Args>
  auto PostTask(Function&& func, Args&&... args)
      -> std::future<typename std::result_of<Function(Args...)>::type>;

  template<typename Function, typename... Args>
  auto PostDelayTask(std::chrono::milliseconds delay, Function&& func, Args&&... args)
      -> std::future<typename std::result_of<Function(Args...)>::type>;

  static unsigned s_hardware_core_num;
  std::mutex log_mutex_;
private:
  void Init(int num);
  void AddThreadImpl();
  void AddThreadIfNeeded();

  std::atomic_bool stop_;
  std::atomic_int idel_thread_;
  std::vector<std::thread> threads_;
  std::thread schedule_thread_;
  std::mutex mutex_;
  std::condition_variable cond_;

  std::priority_queue<DelayTask, std::vector<DelayTask>,
     std::greater<DelayTask>> delay_tasks_;

  std::condition_variable timer_cond_;
  std::queue<std::function<void()>> tasks_;
  std::queue<std::function<void()>> high_proirty_tasks_;
}; //ThreadPool

template<typename Function, typename... Args>
auto ThreadPool::PostDelayTask(std::chrono::milliseconds delay, Function&& func, Args&&...args)
-> std::future<typename std::result_of<Function(Args...)>::type> {
  using return_type = typename std::result_of<Function(Args...)>::type;
  auto task = std::make_shared<std::packaged_task<return_type()>>(
  std::bind(std::forward<Function>(func),std::forward<Args>(args)...));
  std::future<return_type> res = task->get_future();

  mutex_.lock();
  if (delay != std::chrono::milliseconds::zero()) { //if is delay task
    auto earlier_time = 
        delay_tasks_.size() ? delay_tasks_.top().fire_time_ :
        std::chrono::steady_clock::now() + std::chrono::hours(1); //a large time
    delay_tasks_.emplace([task](){(*task)();}, delay);

    // if the new task is eailer, notify scheduler thread
    if (earlier_time > std::chrono::steady_clock::now() + delay) {
      timer_cond_.notify_one();
    }
  } else {
    tasks_.push([task](){ (*task)();});
    cond_.notify_one();
    AddThreadIfNeeded();
  }

  mutex_.unlock();
  return res;

}

template<typename Function, typename... Args>
auto ThreadPool::PostTask(Function&& func, Args&&...args)
-> std::future<typename std::result_of<Function(Args...)>::type> {
  return PostDelayTask(std::chrono::milliseconds::zero(), 
      std::forward<Function>(func), std::forward<Args>(args)...);
}