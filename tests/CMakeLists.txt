# Define test modules
set(TEST_MODULES
    arena
    cstr
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
)

# Loop over test modules and set up tests consistently
foreach(module ${TEST_MODULES})
    add_executable(${module}_test ${module}_test.c)
    target_link_libraries(${module}_test solidc)
    add_test(${module}_test ${module}_test)
endforeach()

# Enable stdin testing if required
if(TEST_STDIN)
    target_compile_definitions(stdstreams_test PUBLIC TEST_STDIN=1)
endif()
