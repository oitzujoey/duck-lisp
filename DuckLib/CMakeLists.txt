add_library(DuckLib core.c array.c string.c)

target_include_directories(DuckLib
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	)

if(USE_DUCKLIB_MALLOC)
  add_definitions(-DUSE_DUCKLIB_MALLOC)
endif()

if(USE_STDLIB)
  add_definitions(-DUSE_STDLIB)
endif()

target_compile_definitions(DuckLisp PRIVATE "EXPORTING_DUCKLIB")
