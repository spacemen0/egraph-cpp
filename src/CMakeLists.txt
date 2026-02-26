add_library(core_lib STATIC 
    union_find.cpp
    e_node.cpp
    e_graph.cpp
    e_class.cpp 
    expression.cpp
    pattern.cpp
    matcher.cpp
    rewriter.cpp
    cost_storage.cpp
    extractor.cpp
    property_table.cpp
    analysis.cpp
)
target_include_directories(core_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
