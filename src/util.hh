#ifndef KRATOS_RUNTIME_UTIL_HH
#define KRATOS_RUNTIME_UTIL_HH

#include <string>
#include <vector>
#include <atomic>

std::vector<std::string> get_tokens(const std::string &line, const std::string &delimiter);

class SpinLock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT ;
public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) { ; }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
    }
};

#endif  // KRATOS_RUNTIME_UTIL_HH
