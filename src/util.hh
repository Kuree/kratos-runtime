#ifndef KRATOS_RUNTIME_UTIL_HH
#define KRATOS_RUNTIME_UTIL_HH

#include <atomic>
#include <string>
#include <vector>

std::vector<std::string> get_tokens(const std::string &line, const std::string &delimiter);

class SpinLock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) {
            ;
        }
    }
    void unlock() { locked.clear(std::memory_order_release); }
};

std::string get_handle_name(const std::string &top, const std::string &handle);

bool is_expr_symbol(const std::string &expr, const std::string::size_type &pos, const size_t &len);

#endif  // KRATOS_RUNTIME_UTIL_HH
