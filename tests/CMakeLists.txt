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
    array
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
)


# Create test executables
foreach(module ${TEST_MODULES})
    add_executable(${module}_test ${module}_test.c)
    
    # Link the main library
    target_link_libraries(${module}_test PRIVATE solidc)
    
    # Platform-specific dependencies
    if(IS_WINDOWS_TARGET)
        # Windows-specific libraries
        target_link_libraries(${module}_test PRIVATE ws2_32 advapi32 kernel32)
    elseif(APPLE)
        # macOS-specific
        target_link_libraries(${module}_test PRIVATE pthread m)
    elseif(UNIX)
        # Linux/Unix-specific
        target_link_libraries(${module}_test PRIVATE pthread m)
        # Add rt library for older Linux systems (for clock_gettime, etc.)
        if(NOT ANDROID)
            target_link_libraries(${module}_test PRIVATE rt)
        endif()
    endif()
    
    # Add the test
    add_test(NAME ${module}_test COMMAND ${module}_test)
endforeach()

# Enable stdin testing if required
if(TEST_STDIN)
    target_compile_definitions(stdstreams_test PUBLIC TEST_STDIN=1)
endif()
