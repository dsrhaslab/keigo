#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <atomic>
#include <cstdio>
#include <thread>
		
static inline void AsmVolatilePause() {
#if defined(__i386__) || defined(__x86_64__)
  asm volatile("pause");
#elif defined(__aarch64__)
  asm volatile("wfe");
#elif defined(__powerpc64__)
  asm volatile("or 27,27,27");
#endif
  // it's okay for other platforms to be no-ops
}

//
// MySpinMutex has very low overhead for low-contention cases.  Method names
// are chosen so you can use std::unique_lock or std::lock_guard with it.
//
class MySpinMutex {
 public:
  MySpinMutex() : locked_(false) {}

  bool try_lock() {
    auto currently_locked = locked_.load(std::memory_order_relaxed);
    return !currently_locked &&
           locked_.compare_exchange_weak(currently_locked, true,
                                         std::memory_order_acquire,
                                         std::memory_order_relaxed);
  }

  void lock() {
    for (size_t tries = 0;; ++tries) {
      if (try_lock()) {
        // success
        break;
      }
      AsmVolatilePause();
      if (tries > 100) {
        std::this_thread::yield();
      }
    }
  }

  void unlock() { locked_.store(false, std::memory_order_release); }

 private:
  std::atomic<bool> locked_;
};

#endif  // SPINLOCK_H