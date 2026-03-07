find_package(OpenMP QUIET)

if(NOT OpenMP_CXX_FOUND)
    # macOS Apple clang: libomp via Homebrew
    find_library(LIBOMP_LIB omp
        HINTS /opt/homebrew/opt/libomp/lib /usr/local/opt/libomp/lib)
    find_path(LIBOMP_INC omp.h
        HINTS /opt/homebrew/opt/libomp/include /usr/local/opt/libomp/include)
    if(LIBOMP_LIB AND LIBOMP_INC)
        add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
        set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LIBOMP_INC}"
            INTERFACE_LINK_LIBRARIES "${LIBOMP_LIB}"
            INTERFACE_COMPILE_OPTIONS "-Xpreprocessor;-fopenmp")
    else()
        message(FATAL_ERROR "OpenMP not found. Run: brew install libomp")
    endif()
endif()

add_executable(phase2
    src/main.cpp
    src/ParallelDataStore.cpp
    ../phase1/src/CSVParser.cpp
    benchmark/Benchmark.cpp
)

target_include_directories(phase2 PRIVATE
    include
    ../phase1/include
)

target_link_libraries(phase2 PRIVATE common OpenMP::OpenMP_CXX)
target_compile_options(phase2 PRIVATE -O2 -Wall -Wextra)
