#include <iostream>
#include <mutex>
#include <type_traits>

class log {
 public:
  static log* getInstance() {
    static log s_instance;
    return &s_instance;
  }

  template <typename T, typename... Args>
  void print(const T& t, const Args&... args) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    print_impl(t, args...);
    std::cout<<std::endl;
  }

  template<typename T>
   void print_impl(const T& arg) {
    std::cout<<arg;
  }
   
  template <typename T, typename... Args>
  void print_impl(const T& t, const Args&... args) {
    print_impl(t);
    print_impl(args...);
  }

  std::recursive_mutex mutex_;
};
