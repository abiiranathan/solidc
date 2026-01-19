# Define test modules
set(TEST_MODULES
    arena
    cstr
    file
    filepath
    lock
    thread
    process
    threadpool
    map
    socket
    stdstreams
    str_to_num
    hash
    csvparser
    unicode
    slist
    list
    vec
    matrix
    cache
    dynarray
    dotenv
    xtime
    hashset
    trie
    flags
    linear_alg
    prettytable
)


# Create test executables
foreach(module ${TEST_MODULES})
    add_executable(${module}_test ${module}_test.c)
    target_link_libraries(${module}_test PRIVATE solidc)
    
    # Check for WIN32, MINGW, OR if the compiler path contains "mingw"
    if(WIN32 OR MINGW OR CMAKE_C_COMPILER MATCHES "mingw")
        # Windows-specific libraries
        target_link_libraries(${module}_test PRIVATE ws2_32 advapi32 kernel32)
    elseif(APPLE)
        target_link_libraries(${module}_test PRIVATE pthread m)
    elseif(UNIX)
        target_link_libraries(${module}_test PRIVATE pthread m)
    endif()
    
    add_test(NAME ${module}_test COMMAND ${module}_test)
endforeach()
# Enable stdin testing if required
if(TEST_STDIN)
    target_compile_definitions(stdstreams_test PUBLIC TEST_STDIN=1)
endif()
