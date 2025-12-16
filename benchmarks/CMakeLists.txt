
add_executable(threadpool_bench  ${CMAKE_CURRENT_SOURCE_DIR}/threadpool_bench.c)
target_link_libraries(threadpool_bench PRIVATE solidc)

add_executable(bench_arena ${CMAKE_CURRENT_SOURCE_DIR}/bench_arena.c)
target_link_libraries(bench_arena PRIVATE solidc)

