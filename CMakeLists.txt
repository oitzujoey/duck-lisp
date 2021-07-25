cmake_minimum_required(VERSION 3.21)

add_library(DuckLisp SHARED duckLisp.c)

target_include_directories(
	DuckLisp
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_compile_definitions(DuckLisp PRIVATE "USE_DUCKLISP")

add_library(
	DuckLib
	SHARED
	DuckLib/core.c
	DuckLib/memory.c
)

target_include_directories(
	DuckLib
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
)

# set_target_properties(DuckLib PROPERTIES
# 	POSITION_INDEPENDENT_CODE ON
# )

target_link_libraries(DuckLisp PRIVATE DuckLib)

set(installable_libs DuckLisp)
list(APPEND installable_libs DuckLib)
install(TARGETS ${installable_libs} DESTINATION lib)
install(FILES core.h memory.h DESTINATION include)
