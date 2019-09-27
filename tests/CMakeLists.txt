add_subdirectory(postman)

add_executable(test_expr_eval test_expr_eval.cc)
target_link_libraries(test_expr_eval gtest kratos gtest_main kratos-runtime)
target_include_directories(test_expr_eval PRIVATE ../extern/kratos/extern/googletest)
gtest_discover_tests(test_expr_eval)