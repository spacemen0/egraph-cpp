add_library(egraph_core_lib STATIC 
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
    common/egraph_config.cpp
    evaluate/evaluator.cpp
    cost_functions/op_costs.cpp
)
target_include_directories(egraph_core_lib PUBLIC
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

# Find system BLAS and LAPACKE, or fetch OpenBLAS on Windows, or use Accelerate on Apple
if(APPLE)
    message(STATUS "Apple system detected. Using Accelerate Framework with NEW LAPACK.")
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    if(NOT ACCELERATE_FRAMEWORK)
        message(FATAL_ERROR "Accelerate framework not found.")
    endif()
    
    target_compile_definitions(egraph_core_lib PUBLIC ACCELERATE_NEW_LAPACK)
    target_link_libraries(egraph_core_lib PUBLIC magic_enum::magic_enum mimalloc ${ACCELERATE_FRAMEWORK})
else()
    find_package(BLAS QUIET)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LAPACKE QUIET lapacke)
    endif()

    if(NOT LAPACKE_FOUND)
        find_path(LAPACKE_INCLUDE_DIRS NAMES lapacke.h HINTS /opt/homebrew/opt/lapack/include /opt/homebrew/include /usr/local/include /usr/include)
        find_library(LAPACKE_LIBRARIES NAMES lapacke lapack openblas HINTS /opt/homebrew/opt/lapack/lib /opt/homebrew/lib /usr/local/lib /usr/lib)
        if(LAPACKE_INCLUDE_DIRS AND LAPACKE_LIBRARIES)
            set(LAPACKE_FOUND TRUE)
        endif()
    endif()

    if(LAPACKE_FOUND)
        message(STATUS "Found system BLAS and LAPACKE")
        target_include_directories(egraph_core_lib PUBLIC ${LAPACKE_INCLUDE_DIRS})
        target_link_directories(egraph_core_lib PUBLIC ${LAPACKE_LIBRARY_DIRS})
        target_link_libraries(egraph_core_lib PUBLIC magic_enum::magic_enum mimalloc ${BLAS_LIBRARIES} ${LAPACKE_LIBRARIES})
    elseif(WIN32)
        message(STATUS "System BLAS/LAPACKE not found. Fetching OpenBLAS prebuilt package for Windows...")
        include(FetchContent)
        FetchContent_Declare(
            openblas_win
            URL https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.28/OpenBLAS-0.3.28-x64.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(openblas_win)

        target_include_directories(egraph_core_lib PUBLIC ${openblas_win_SOURCE_DIR}/include)
        if(MSVC)
            target_link_libraries(egraph_core_lib PUBLIC magic_enum::magic_enum mimalloc ${openblas_win_SOURCE_DIR}/lib/libopenblas.lib)
        else()
            target_link_libraries(egraph_core_lib PUBLIC magic_enum::magic_enum mimalloc ${openblas_win_SOURCE_DIR}/lib/libopenblas.dll.a)
        endif()

        # Define target to copy DLL to target output directory
        add_custom_target(copy_openblas_dll ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${openblas_win_SOURCE_DIR}/bin/libopenblas.dll"
                "${CMAKE_BINARY_DIR}/bin"
        )
    else()
        message(FATAL_ERROR "BLAS and LAPACKE are required on Linux. Please install them or configure PkgConfig.")
    endif()
endif()

add_executable(egraph_cli cli.cpp)
target_link_libraries(egraph_cli PRIVATE egraph_core_lib)
