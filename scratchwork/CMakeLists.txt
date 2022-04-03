cmake_minimum_required(VERSION 3.10)

project(ducklisp-dev VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIRECTORY}")

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

add_subdirectory(.. build-lisp)

add_executable(memory-dev memory-dev.c)
add_executable(duckLisp-dev duckLisp-dev.c)
add_executable(trie-dev trie-dev.c)
add_executable(sort-test sort-test.c)

if(MSVC)
  target_compile_options(memory-dev PRIVATE /W4 /WX)
  target_compile_options(duckLisp-dev PRIVATE /W4 /WX)
  target_compile_options(trie-dev PRIVATE /W4 /WX)
  target_compile_options(sort-test PRIVATE /W4 /WX)
else()
  target_compile_options(memory-dev PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(duckLisp-dev PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(trie-dev PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(sort-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(memory-dev PUBLIC DuckLib)
target_link_libraries(duckLisp-dev PUBLIC DuckLisp)
target_link_libraries(trie-dev PUBLIC DuckLib)
target_link_libraries(sort-test PUBLIC DuckLib)
