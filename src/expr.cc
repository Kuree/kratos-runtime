#include "expr.hh"
#include "exprtk/exprtk.hpp"

using SymbolType = double;
using SymbolTable = exprtk::symbol_table<SymbolType>;
using Expression = exprtk::expression<SymbolType>;
using ExprSymbol = std::pair<SymbolType, std::unordered_map<std::string, SymbolType>>;
using Parser = exprtk::parser<SymbolType>;

struct BreakPointExpression {
    SymbolTable table;
    Expression expr;
    std::unordered_map<std::string, SymbolType > var_mapping;
};

std::unordered_map<uint32_t, BreakPointExpression> symbol_table;

void add_expr(uint32_t breakpoint_id, const std::string &expr,
              const std::unordered_set<std::string> &symbols,
              const std::unordered_map<std::string, int64_t> &constants) {
    symbol_table.emplace(breakpoint_id, BreakPointExpression{});
    auto &bp = symbol_table.at(breakpoint_id);
    for (auto const &s : symbols) {
        // default value to 0
        bp.var_mapping.emplace(s, 0);
        auto &x = bp.var_mapping.at(s);
        bp.table.add_variable(s, x);
    }
    for (auto const &[s, v]: constants) {
        bp.table.add_constant(s, v);
    }

    // compile the expression
    auto &expr_t = bp.expr;
    expr_t.register_symbol_table(bp.table);
    Parser parser;
    auto result = parser.compile(expr, expr_t);
    if (!result) {
        throw std::runtime_error("Invalid expression");
    }
}

bool evaluate(uint32_t breakpoint_id, const std::unordered_map<std::string, int64_t> &values) {
    if (symbol_table.find(breakpoint_id) == symbol_table.end())
        return false;
    // set values
    auto &bp = symbol_table.at(breakpoint_id);
    for (auto const &[s, v]: values) {
        bp.var_mapping.at(s) = v;
    }
    SymbolType raw_result = bp.expr.value();
    int v = static_cast<int>(raw_result);
    printf("final value %d\n", v);
    return v;
}

void remove_expr(uint32_t breakpoint_id) {
    if (symbol_table.find(breakpoint_id) != symbol_table.end()) {
        symbol_table.erase(breakpoint_id);
    }
}

bool has_expr_breakpoint(uint32_t breakpoint_id) {
    return symbol_table.find(breakpoint_id) != symbol_table.end();
}