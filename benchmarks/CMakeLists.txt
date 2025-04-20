
add_executable(threadpool_bench 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/threadpool_bench.c
)

add_executable(threadpool_bench2 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark.c
)

add_executable(bench_arena 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arena.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark.c
)