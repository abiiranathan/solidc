cmake_minimum_required(VERSION 3.5)
project(solidc 
    VERSION 0.1.0 
    DESCRIPTION "Robust collection of general purpose header-only C libraries and data structures"
    LANGUAGES CXX C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(GNUInstallDirs)
include(CTest)

# Header only library
add_library(solidc INTERFACE)
target_include_directories(solidc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set(HEADERS
    os.h
    lock.h
    list.h
    log.h
    map.h
    ordered_map.h
    slist.h
    socket.h
    str.h
    str_builder.h
    trie.h
    vec.h
    set.h
)

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/solidc)

if(BUILD_TESTING)
    # Test executables
    add_executable(str_test tests/str_test.cpp)
    target_link_libraries(str_test gtest pcre2-8)

    add_executable(trie_test tests/trie_test.cpp)
    target_link_libraries(trie_test gtest m)

    add_executable(map_test tests/map_test.cpp)
    target_link_libraries(map_test gtest)

    add_executable(ordered_map_test tests/ordered_map_test.cpp)
    target_link_libraries(ordered_map_test gtest)

    add_executable(vec_test tests/vec_test.cpp)
    target_link_libraries(vec_test gtest)

    add_executable(list_test tests/list_test.cpp)
    target_link_libraries(list_test gtest)

    add_executable(slist_test tests/slist_test.cpp)
    target_link_libraries(slist_test gtest)

    add_executable(set_test tests/set_test.cpp)
    target_link_libraries(set_test gtest)

    add_executable(os_test tests/os_test.cpp)
    target_link_libraries(os_test gtest)

    add_executable(str_builder tests/str_builder_test.cpp)
    target_link_libraries(str_builder gtest)

    add_executable(unicode_test tests/unicode_test.cpp)
    target_link_libraries(unicode_test gtest)

    include(GoogleTest)
    
    gtest_discover_tests(str_test)
    gtest_discover_tests(trie_test)
    gtest_discover_tests(map_test)
    gtest_discover_tests(ordered_map_test)
    gtest_discover_tests(vec_test)
    gtest_discover_tests(list_test)
    gtest_discover_tests(slist_test)
    gtest_discover_tests(set_test)
    gtest_discover_tests(os_test)
    gtest_discover_tests(str_builder)
    gtest_discover_tests(unicode_test)
endif()
