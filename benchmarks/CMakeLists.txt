# Only a standalone, explicitly built benchmark executable; not a test.
add_executable(benchmark_binary_tree_parallel benchmark_binary_tree_parallel.cpp)
target_link_libraries(benchmark_binary_tree_parallel PRIVATE
    ariadne-tree tree ariadne-benchmarking)
add_dependencies(benchmarks benchmark_binary_tree_parallel)
