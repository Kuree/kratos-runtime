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

std::string get_handle_name(const std::string &top, const std::string& handle_name) {
    // change the handle name
    if (handle_name.size() < top.size() ||
        handle_name.substr(top.size()) != top) {
        std::string format;
        if (top.back() == '.')
            format = "{0}{1}";
        else
            format = "{0}.{1}";
        return fmt::format(format, top, handle_name);
    }
    return handle_name;
}