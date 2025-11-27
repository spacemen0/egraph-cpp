add_library(core_lib STATIC 
    union_find.cpp
    e_node.cpp
    e_graph.cpp
    e_class.cpp 
    expression.cpp
    pattern.cpp
    rewrite.cpp
)
target_include_directories(core_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(main main.cpp)

target_link_libraries(main PRIVATE core_lib)