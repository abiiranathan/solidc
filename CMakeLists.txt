cmake_minimum_required(VERSION 3.5)
project(falcon LANGUAGES CXX C VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(GNUInstallDirs)

# Header only library
add_library(falcon INTERFACE)
target_include_directories(falcon INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_executable(str_test str_test.cpp)
target_link_libraries(str_test gtest pcre2-8)

add_executable(trie_test trie_test.cpp)
target_link_libraries(trie_test gtest m pthread)

add_executable(map_test map_test.cpp)
target_link_libraries(map_test gtest pthread)

install(FILES str.h DESTINATION falcon)
install(FILES trie.h DESTINATION falcon)
install(FILES log.h DESTINATION falcon)
install(FILES map.h DESTINATION falcon)

# Testing
enable_testing()
add_test(NAME str_test COMMAND str_test)
add_test(NAME trie_test COMMAND trie_test)

