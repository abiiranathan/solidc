# Add the test executable for arena module
set(ARENA_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/arena_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arena.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
)

add_executable(arena_test ${ARENA_SRCS})
add_test(arena_test arena_test)


# Add the test executable for cstr module
set(CSTR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/cstr_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cstr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arena.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
)

add_executable(cstr_test ${CSTR_SRCS})
add_test(cstr_test cstr_test)


# Add the test executable for file module
set(FILE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/file_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filepath.c
)

add_executable(file_test ${FILE_SRCS})
add_test(file_test file_test)


# Add the test executable for filepath module
set(FILEPATH_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/filepath_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filepath.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/file.c
)

add_executable(filepath_test ${FILEPATH_SRCS})
add_test(filepath_test filepath_test)


# Add the test executable for lock module
set(LOCK_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/lock_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
)

add_executable(lock_test ${LOCK_SRCS})
add_test(lock_test lock_test)


# Add the test executable for thread module
set(THREAD_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
)

add_executable(thread_test ${THREAD_SRCS})
add_test(thread_test thread_test)


# Add the test executable for map module
set(MAP_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/map_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
)

add_executable(map_test ${MAP_SRCS})
add_test(map_test map_test)


# Add the test executable for process module
set(PROCESS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/process_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/process.c
)

add_executable(process_test ${PROCESS_SRCS})
add_test(process_test process_test)


# Add the test executable for threadpool module
set(THREADPOOL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/threadpool_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
)

add_executable(threadpool_test ${THREADPOOL_SRCS})
add_test(threadpool_test threadpool_test)


# Add the test executable for vec module
set(VEC_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/vec_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/vec.c
)

add_executable(vec_test ${VEC_SRCS})
add_test(vec_test vec_test)


# Add the test executable for optional module
set(OPTIONAL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/optional_test.c
)

add_executable(optional_test ${OPTIONAL_SRCS})
add_test(optional_test optional_test)


# Add the test executable for socket module
set(SOCKET_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/socket_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/socket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
)

add_executable(socket_test ${SOCKET_SRCS})
add_test(socket_test socket_test)
