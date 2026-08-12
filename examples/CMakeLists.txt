add_executable(examples
    main.cpp
    ols.cpp
    gls.cpp
    image_restoration.cpp
    ols_dgels.cpp
)
target_link_libraries(examples PRIVATE core_lib)