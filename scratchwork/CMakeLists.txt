
cmake_minimum_required(VERSION 3.10)

project(ducklisp-dev VERSION 1.0)

add_subdirectory(../DuckLib build)

add_executable(memory-dev memory-dev.c)

target_link_libraries(memory-dev PUBLIC DuckLib)

target_include_directories(memory-dev PUBLIC
	"${PROJECT_BINARY_DIR}"
	"${PROJECT_BINARY_DIR}/../DuckLib"
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
