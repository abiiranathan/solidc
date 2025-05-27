# Define test modules
set(TEST_MODULES
    arena
    cstr
    cstr_array
    flag
    file
    filepath
    lock
    thread
    process
    threadpool
    map
    socket
    stdstreams
    strton
    hash
    csvparser
    array
    unicode
    slist
    larena
    vec
    matrix
)

# Then modify your test loop:
foreach(module ${TEST_MODULES})
    add_executable(${module}_test ${module}_test.c)
    
    # Link the main library
    target_link_libraries(${module}_test PRIVATE solidc)
    
    # Platform-specific dependencies
    if(IS_WINDOWS_TARGET)
        target_link_libraries(${module}_test PRIVATE ws2_32 advapi32 kernel32)
        target_compile_options(${module}_test PRIVATE /arch:AVX2)
    else()
        target_link_libraries(${module}_test PRIVATE pthread m)
        target_compile_options(${module}_test PRIVATE -mavx -mavx2 -mfma)

    endif()
    
    add_test(NAME ${module}_test COMMAND ${module}_test)
endforeach()

# Enable stdin testing if required
if(TEST_STDIN)
    target_compile_definitions(stdstreams_test PUBLIC TEST_STDIN=1)
endif()
