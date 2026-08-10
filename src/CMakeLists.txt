add_library(core_lib STATIC 
    core/union_find.cpp
    core/e_node.cpp
    core/e_graph.cpp
    core/e_class.cpp 
    core/expression.cpp
    rewrite/pattern.cpp
    rewrite/matcher.cpp
    rewrite/rewriter.cpp
    extract/extractor.cpp
    prune/pruner.cpp
    analysis/property_table.cpp
    analysis/analysis.cpp
    io/e_graph_visualization.cpp
    common/parser.cpp
    evaluate/evaluator.cpp
    cost_functions/op_costs.cpp
)
target_include_directories(core_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/rewrite
    ${CMAKE_CURRENT_SOURCE_DIR}/extract
    ${CMAKE_CURRENT_SOURCE_DIR}/prune
    ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/rule_sets
    ${CMAKE_CURRENT_SOURCE_DIR}/io
    ${CMAKE_CURRENT_SOURCE_DIR}/evaluate
    ${CMAKE_CURRENT_SOURCE_DIR}/cost_functions
)

find_package(BLAS REQUIRED)
target_link_libraries(core_lib PUBLIC magic_enum::magic_enum ${BLAS_LIBRARIES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(LAPACKE REQUIRED lapacke)

target_include_directories(core_lib PUBLIC ${LAPACKE_INCLUDE_DIRS})
target_link_directories(core_lib PUBLIC ${LAPACKE_LIBRARY_DIRS})
target_link_libraries(core_lib PUBLIC ${LAPACKE_LIBRARIES})

add_executable(egraph_cli cli.cpp)
target_link_libraries(egraph_cli PRIVATE core_lib)
add_executable(tester tester.cpp)
target_link_libraries(tester PRIVATE core_lib)
