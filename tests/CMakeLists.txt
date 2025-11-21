add_executable(unit_tests 
    union_find_test.cpp
    e_node_test.cpp
    e_graph_test.cpp
)

target_link_libraries(unit_tests 
    PRIVATE 
    GTest::gtest_main 
    core_lib
)

target_compile_definitions(unit_tests PRIVATE UNIT_TEST)

gtest_discover_tests(unit_tests)