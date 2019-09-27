#include "gtest/gtest.h"
#include "../src/expr.hh"
#include "vpi_impl.hh"


TEST(expr_eval, eval_no_throw) { // NOLINT
    EXPECT_NO_THROW(add_expr(0, "a + 2", {"a"}));
}

TEST(expr_eval, value) { // NOLINT
    add_expr(0, "a + 2", {"a"});
    EXPECT_TRUE(evaluate(0, {{"a", 1}}));
    EXPECT_FALSE(evaluate(1, {}));
    EXPECT_FALSE(evaluate(0, {{"a", -2}}));
}

TEST(expr_eval, bool_) {    // NOLINT
    add_expr(0, "a > 2", {"a"});
    EXPECT_FALSE(evaluate(0, {{"a", 1}}));
    EXPECT_FALSE(evaluate(0, {{"a", 2}}));
    EXPECT_TRUE(evaluate(0, {{"a", 3}}));
}

TEST(expr_eval, remove) {   // NOLINT
    add_expr(0, "a > 2", {"a"});
    EXPECT_TRUE(evaluate(0, {{"a", 3}}));
    remove_expr(0);
    EXPECT_FALSE(evaluate(0, {{"a", 3}}));
}