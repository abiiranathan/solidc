# Add the test executable for arena module
set(ARENA_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/arena_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arena.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
)

add_executable(arena_test ${ARENA_SRCS})
add_test(arena_test arena_test)
target_link_libraries(arena_test pthread m)


# Add the test executable for cstr module
set(CSTR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/cstr_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cstr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arena.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
)

add_executable(cstr_test ${CSTR_SRCS})
add_test(cstr_test cstr_test)
target_link_libraries(cstr_test pthread m)

# Add the test executable for file module
set(FILE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/file_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filepath.c
)

add_executable(file_test ${FILE_SRCS})
add_test(file_test file_test)
target_link_libraries(file_test pthread m)


# Add the test executable for filepath module
set(FILEPATH_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/filepath_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filepath.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/file.c
)

add_executable(filepath_test ${FILEPATH_SRCS})
add_test(filepath_test filepath_test)
target_link_libraries(filepath_test pthread m)

# Add the test executable for lock module
set(LOCK_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/lock_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
)

add_executable(lock_test ${LOCK_SRCS})
add_test(lock_test lock_test)
target_link_libraries(lock_test pthread m)


# Add the test executable for thread module
set(THREAD_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
)

add_executable(thread_test ${THREAD_SRCS})
add_test(thread_test thread_test)
target_link_libraries(thread_test pthread m)

# Add the test executable for process module
set(PROCESS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/process_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/process.c
)

add_executable(process_test ${PROCESS_SRCS})
add_test(process_test process_test)
target_link_libraries(process_test pthread m)


# Add the test executable for threadpool module
set(THREADPOOL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/threadpool_test.c
)

add_executable(threadpool_test ${THREADPOOL_SRCS})
add_test(threadpool_test threadpool_test)
target_link_libraries(threadpool_test pthread m)

# Add the test executable for map module
set(MAP_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/map_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/threadpool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/thread.c
)

add_executable(map_test ${MAP_SRCS})
add_test(map_test map_test)
target_link_libraries(map_test pthread m)



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
target_link_libraries(socket_test pthread m)

# If you have access to stdin, turn this on
set(TEST_STDIN OFF)

# stdstreams_test module
set(STDSTREAMS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/stdstreams_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/stdstreams.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cstr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arena.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filepath.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/file.c
)



add_executable(stdstreams_test ${STDSTREAMS_SRCS})
if(TEST_STDIN)
    target_compile_definitions(stdstreams_test PUBLIC TEST_STDIN=1)
endif()

target_link_libraries(stdstreams_test pthread m)
add_test(stdstreams_test stdstreams_test)
 
# strton_test module
set(STRTON_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/strton_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/strton.c
)

add_executable(strton_test ${STRTON_SRCS})
add_test(strton_test strton_test)

# hash test module
set(HASH_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/hash_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/hash.c
)

add_executable(hash_test ${HASH_SRCS})
add_test(hash_test hash_test)


set(FLAG_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/flag_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/flag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/strton.c
)

add_executable(flag_test ${FLAG_SRCS})
add_test(flag_test flag_test)

# csvparser_test module
set(CSVPARSER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/csvparser_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/csvparser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/arena.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/filepath.c
)

add_executable(csvparser_test ${CSVPARSER_SRCS})
add_test(csvparser_test csvparser_test)
target_link_libraries(csvparser_test pthread m)


# Array test module
set(ARRAY_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/array_test.c
)

add_executable(array_test ${ARRAY_SRCS})
add_test(array_test array_test)

# channel test module
set(CHANNEL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/channel_test.c
)

add_executable(channel_test ${CHANNEL_SRCS})
add_test(channel_test channel_test)
