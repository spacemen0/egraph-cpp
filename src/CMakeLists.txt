add_library(core_lib STATIC 
    core/union_find.cpp
    core/e_node.cpp
    core/e_graph.cpp
    core/e_class.cpp 
    core/expression.cpp
    rewrite/pattern.cpp
    rewrite/matcher.cpp
    rewrite/rewriter.cpp
    extract/cost_storage.cpp
    extract/extractor.cpp
    prune/pruner.cpp
    analysis/property_table.cpp
    analysis/analysis.cpp
    io/e_graph_visualization.cpp
    common/parser.cpp
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
)

add_executable(egraph_cli main.cpp)
target_link_libraries(egraph_cli PRIVATE core_lib)
