#include "util.hh"
#include "fmt/format.h"

std::vector<std::string> get_tokens(const std::string &line, const std::string &delimiter) {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    std::string token;
    // copied from https://stackoverflow.com/a/7621814
    while ((pos = line.find_first_of(delimiter, prev)) != std::string::npos) {
        if (pos > prev) {
            tokens.emplace_back(line.substr(prev, pos - prev));
        }
        prev = pos + 1;
    }
    if (prev < line.length()) tokens.emplace_back(line.substr(prev, std::string::npos));
    // remove empty ones
    std::vector<std::string> result;
    result.reserve(tokens.size());
    for (auto const &t : tokens)
        if (!t.empty()) result.emplace_back(t);
    return result;
}

std::string get_handle_name(const std::string &top, const std::string &handle_name) {
    // change the handle name
    if (!top.empty() && handle_name.find(top) == std::string::npos) {
        std::string format;
        if (top.back() == '.')
            format = "{0}{1}";
        else
            format = "{0}.{1}";
        return fmt::format(format, top, handle_name);
    }
    return handle_name;
}

bool is_expr_symbol(const std::string &expr, const std::string &symbol) {
    // FIXME: this is a hack
    auto pos = expr.find(symbol);
    if (pos == std::string::npos) return false;
    static auto is_w = [](const char c) {
        return ((c >= 0x30 && c <= 0x39) || (c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A) ||
                (c == 0x5F));
    };
    if (pos > 0) {
        char cc = expr[pos];
        if (is_w(cc)) return false;
    }
    if (pos + symbol.size() < expr.size()) {
        char cc = expr[pos + symbol.size()];
        if (is_w(cc)) return false;
    }
    return true;
}

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    auto to_path = to;
    if (to_path.back() == '/' && str[start_pos + from.length()] == '/') {
        // fs doesn't like double //
        to_path = to_path.substr(0, to_path.size() - 1);
    } else if (to_path.back() != '/' && from.back() == '/') {
        to_path = to_path.append("/");
    }
    str.replace(start_pos, from.length(), to_path);
    return true;
}
