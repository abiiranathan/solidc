
add_executable(threadpool_bench 
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/threadpool_bench.c
)
