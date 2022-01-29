cmake_minimum_required(VERSION 3.10)

add_library(DuckLisp duckLisp.c duckVM.c)

target_include_directories(DuckLisp
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	)

add_library(DuckLib STATIC
            DuckLib/core.c
            DuckLib/memory.c
            DuckLib/array.c
            DuckLib/string.c
            DuckLib/trie.c
            DuckLib/sort.c
            )

target_include_directories(DuckLib PUBLIC
                           ${CMAKE_CURRENT_BINARY_DIR}
                           )

# add_subdirectory(DuckLib)

set_target_properties(DuckLib PROPERTIES
                      POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
                      )
                      
target_compile_definitions(DuckLib PUBLIC "EXPORTING_DUCKLIB")

target_link_libraries(DuckLisp PUBLIC DuckLib)

target_compile_definitions(DuckLisp PUBLIC "EXPORTING_DUCKLISP")
