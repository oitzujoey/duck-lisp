cmake_minimum_required(VERSION 3.10)

add_library(DuckLisp
  duckLisp.c
  parser.c
  parenthesis-inferrer.c
  emitters.c
  generators.c
  assembler.c
  duckVM.c)

target_include_directories(DuckLisp
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	)

if(MEMCHECK)
  add_definitions(-DMEMCHECK)
endif()

add_library(DuckLib STATIC
            DuckLib/core.c
            DuckLib/memory.c
            DuckLib/array.c
            DuckLib/string.c
            DuckLib/trie.c
            DuckLib/sort.c
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

# add_subdirectory(DuckLib)

set_target_properties(DuckLib PROPERTIES
                      POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
                      )

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

target_compile_definitions(DuckLib PUBLIC "EXPORTING_DUCKLIB")

target_link_libraries(DuckLisp PUBLIC DuckLib)

target_compile_definitions(DuckLisp PUBLIC "EXPORTING_DUCKLISP")
