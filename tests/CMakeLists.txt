add_executable(unit_tests 
    unit/union_find_test.cpp
    unit/e_node_test.cpp
    unit/e_graph_test.cpp
    unit/pattern_test.cpp
    unit/analysis_test.cpp
    unit/e_class_test.cpp
    unit/expression_test.cpp
    unit/parser_test.cpp
    unit/rewrite_test.cpp
    unit/scale_rewrite_test.cpp
    unit/extractor_test.cpp
    unit/property_test.cpp
    unit/api_test.cpp
    integration/integration_test.cpp
    integration/ols_test.cpp
    integration/gls_test.cpp
    integration/pruner_test.cpp
)

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/support
)

target_link_libraries(unit_tests 
    PRIVATE 
    GTest::gtest_main 
    core_lib
)

target_compile_definitions(unit_tests PRIVATE UNIT_TEST)

gtest_discover_tests(unit_tests)