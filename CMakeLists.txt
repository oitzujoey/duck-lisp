cmake_minimum_required(VERSION 3.10)

add_library(DuckLisp duckLisp.c)

add_subdirectory(DuckLib)

target_link_libraries(DuckLisp PUBLIC DuckLib)

target_include_directories(DuckLisp
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	)
