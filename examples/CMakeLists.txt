add_executable(OLS OLS.cpp)
target_link_libraries(OLS PRIVATE core_lib)
add_executable(GLS GLS.cpp)
target_link_libraries(GLS PRIVATE core_lib)