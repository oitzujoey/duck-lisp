cmake_minimum_required(VERSION 3.10)

add_library(DuckLisp
  gc.c)

target_include_directories(DuckLisp
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	)

if(MEMCHECK)
  add_definitions(-DMEMCHECK)
endif()

add_library(DuckLib STATIC
            DuckLib/core.c
            DuckLib/array.c
            )

if(MSVC)
  target_compile_options(DuckLisp PUBLIC /W4 /WX)
  target_compile_options(DuckLib PUBLIC /W4 /WX)
else()
  target_compile_options(DuckLisp PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
  target_compile_options(DuckLib PUBLIC -Wall -Wextra -Wpedantic -Werror -Wdouble-promotion)
endif()

target_include_directories(DuckLib PUBLIC
                           ${CMAKE_CURRENT_BINARY_DIR}
                           )

if(USE_DUCKLIB_MALLOC)
  add_definitions(-DUSE_DUCKLIB_MALLOC)
endif()

if(USE_STDLIB)
  add_definitions(-DUSE_STDLIB)
endif()

target_compile_definitions(DuckLib PUBLIC "EXPORTING_DUCKLIB")

target_link_libraries(DuckLisp PUBLIC DuckLib)

target_compile_definitions(DuckLisp PUBLIC "EXPORTING_DUCKLISP")
