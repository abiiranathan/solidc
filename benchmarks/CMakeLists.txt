
add_executable(threadpool_bench  ${CMAKE_CURRENT_SOURCE_DIR}/threadpool_bench.c)
target_link_libraries(threadpool_bench PRIVATE solidc)

add_executable(threadpool_micro  ${CMAKE_CURRENT_SOURCE_DIR}/threadpool_micro.c)
target_link_libraries(threadpool_micro PRIVATE solidc pthread)

add_executable(file_micro  ${CMAKE_CURRENT_SOURCE_DIR}/file_micro.c)
target_link_libraries(file_micro PRIVATE solidc pthread)

add_executable(bench_arena ${CMAKE_CURRENT_SOURCE_DIR}/bench_arena.c)
target_link_libraries(bench_arena PRIVATE solidc)

add_executable(bench_cache ${CMAKE_CURRENT_SOURCE_DIR}/bench_cache.c)
target_link_libraries(bench_cache PRIVATE solidc)
add_executable(map_micro  ${CMAKE_CURRENT_SOURCE_DIR}/map_micro.c)
target_link_libraries(map_micro PRIVATE solidc pthread)

add_executable(csv_micro  ${CMAKE_CURRENT_SOURCE_DIR}/csv_micro.c)
target_link_libraries(csv_micro PRIVATE solidc pthread)

add_executable(hash_micro  ${CMAKE_CURRENT_SOURCE_DIR}/hash_micro.c)
target_link_libraries(hash_micro PRIVATE solidc pthread)

add_executable(swiss_micro  ${CMAKE_CURRENT_SOURCE_DIR}/swiss_micro.c)
target_link_libraries(swiss_micro PRIVATE solidc pthread)

add_executable(dynarray_micro  ${CMAKE_CURRENT_SOURCE_DIR}/dynarray_micro.c)
target_link_libraries(dynarray_micro PRIVATE solidc pthread)

add_executable(sort_micro  ${CMAKE_CURRENT_SOURCE_DIR}/sort_micro.c)
target_link_libraries(sort_micro PRIVATE solidc pthread)

add_executable(filepath_walk_bench  ${CMAKE_CURRENT_SOURCE_DIR}/filepath_walk_bench.c)
target_link_libraries(filepath_walk_bench PRIVATE solidc pthread)
