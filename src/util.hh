#ifndef KRATOS_RUNTIME_UTIL_HH
#define KRATOS_RUNTIME_UTIL_HH

#include <atomic>
#include <string>
#include <vector>
#include <sstream>

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

bool is_expr_symbol(const std::string &expr, const std::string &symbol);

bool replace(std::string& str, const std::string& from, const std::string& to);

bool is_digits(const std::string &str);

template <typename Iter>
std::string static join(Iter begin, Iter end, const std::string &sep) {
    std::stringstream stream;
    for (auto it = begin; it != end; it++) {
        if (it != begin) stream << sep;
        stream << *it;
    }
    return stream.str();
}

#endif  // KRATOS_RUNTIME_UTIL_HH
