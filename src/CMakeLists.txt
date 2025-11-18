add_library(core_lib STATIC 
    union_find.cpp 
    union_find.h
)
target_include_directories(core_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(my_app main.cpp)

target_link_libraries(my_app PRIVATE core_lib)