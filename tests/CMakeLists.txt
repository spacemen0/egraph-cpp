add_executable(unit_tests 
    union_find_test.cpp
)

target_link_libraries(unit_tests 
    PRIVATE 
    GTest::gtest_main 
    core_lib
)

gtest_discover_tests(unit_tests)