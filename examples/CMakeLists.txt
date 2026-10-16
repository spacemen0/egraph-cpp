add_executable(examples
    main.cpp
    ols.cpp
    gls.cpp
    image_restoration.cpp
    ols_dgels.cpp
    stochastic_newton.cpp
    kalman_gain.cpp
    kalman_filter.cpp
)
target_link_libraries(examples PRIVATE egraph_core_lib)
if(TARGET copy_openblas_dll)
    add_dependencies(examples copy_openblas_dll)
endif()
if(TARGET copy_asan_runtime)
    add_dependencies(examples copy_asan_runtime)
endif()