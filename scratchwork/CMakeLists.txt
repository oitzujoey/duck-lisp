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
option(USE_DUCKLIB_MALLOC "Use DuckLib's memory allocator" OFF)
option(USE_STDLIB "Replace DuckLib functions with standard library equivalents" ON)
option(NO_OPTIMIZE_JUMPS "Disable minimization of jump and branch instruction size" OFF)
option(NO_OPTIMIZE_PUSHPOPS "Disable deletion of redundant push-pop instruction sequences" OFF)
option(USE_DATALOGGING "Add an extra field in \"duckLisp_t\" called \"duckLisp_datalog_t\" to track performance" OFF)
option(USE_PARENTHESIS_INFERENCE "Enable optional parenthesis inference" OFF)


add_subdirectory(.. build-lisp)

add_executable(memory-dev memory-dev.c)
add_executable(duckLisp-dev duckLisp-dev.c)
add_executable(trie-dev trie-dev.c)
add_executable(sort-test sort-test.c)
add_executable(duckLisp-test duckLisp-test.c)
add_executable(example-callbacks example-callbacks.c)
add_executable(example-script-call example-script-call.c)

if(MSVC)
  target_compile_options(memory-dev PUBLIC /W4 /WX)
  target_compile_options(duckLisp-dev PUBLIC /W4 /WX)
  target_compile_options(trie-dev PUBLIC /W4 /WX)
  target_compile_options(sort-test PUBLIC /W4 /WX)
  target_compile_options(example-callbacks PUBLIC /W4 /WX)
  target_compile_options(example-script-call PUBLIC /W4 /WX)
else()
  target_compile_options(memory-dev PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
  target_compile_options(duckLisp-dev PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
  target_compile_options(trie-dev PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
  target_compile_options(sort-test PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
  target_compile_options(duckLisp-test PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
  target_compile_options(example-callbacks PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
  target_compile_options(example-script-call PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
endif()

if(USE_DUCKLIB_MALLOC)
  add_definitions(-DUSE_DUCKLIB_MALLOC)
endif()

if(USE_STDLIB)
  add_definitions(-DUSE_STDLIB)
endif()

if(NO_OPTIMIZE_JUMPS)
  add_definitions(-DNO_OPTIMIZE_JUMPS)
endif()

if(NO_OPTIMIZE_PUSHPOPS)
  add_definitions(-DNO_OPTIMIZE_PUSHPOPS)
endif()

if(USE_DATALOGGING)
  add_definitions(-DUSE_DATALOGGING)
endif()

if(USE_PARENTHESIS_INFERENCE)
  add_definitions(-DUSE_PARENTHESIS_INFERENCE)
endif()

target_link_libraries(memory-dev PUBLIC DuckLib)
target_link_libraries(duckLisp-dev PUBLIC DuckLisp)
target_link_libraries(trie-dev PUBLIC DuckLib)
target_link_libraries(sort-test PUBLIC DuckLib)
target_link_libraries(duckLisp-test PUBLIC DuckLisp)
target_link_libraries(example-callbacks PUBLIC DuckLisp)
target_link_libraries(example-script-call PUBLIC DuckLisp)
