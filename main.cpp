#include "thread_pool.h"
#include <iostream>
#include <chrono>
#include <vector>

int main() {
  int i = 10;
  ThreadPool thread_pool(4);
  
  std::vector<std::future<int>> res;


 int normal_task = 400;
 int j =0;
 while ( ++j != normal_task) {
    auto g = [&thread_pool](int i) {

      log::getInstance()->print("normal task: ",i," run"); 
      std::this_thread::sleep_for(std::chrono::milliseconds(rand()%100));
      return 0;
    };
    res.push_back(std::move(thread_pool.PostTask(g,j)));
 }


  while(i--) {
    
    auto time = std::chrono::steady_clock::now();
    auto f = [time_ = time, &thread_pool](int i) {
          auto now = std::chrono::steady_clock::now();

          log::getInstance()->print("post delay: ",i , " real_run_delay :",
            std::chrono::duration_cast<std::chrono::milliseconds>(now-time_).count());
          
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          return 0;
          };
    res.push_back(std::move(thread_pool.PostDelayTask(
       std::chrono::milliseconds(i*100),std::bind(f,i* 100))));
  };

  return 0;
}