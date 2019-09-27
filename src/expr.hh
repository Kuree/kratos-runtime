#ifndef KRATOS_RUNTIME_EXPR_HH
#define KRATOS_RUNTIME_EXPR_HH

#include <unordered_map>
#include <unordered_set>

bool evaluate(uint32_t breakpoint_id, const std::unordered_map<std::string, int64_t> &values);
void add_expr(uint32_t breakpoint_id, const std::string &expr,
              const std::unordered_set<std::string> &symbols,
              const std::unordered_map<std::string, uint64_t> & = {});
void remove_expr(uint32_t breakpoint_id);

#endif  // KRATOS_RUNTIME_EXPR_HH
