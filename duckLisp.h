
#ifndef DUCKLISP_H
#define DUCKLISP_H

#include "DuckLib/core.h"
#include "DuckLib/memory.h"

typedef struct {
	dl_memoryAllocation_t memoryAllocation;
} duckLisp_t;

dl_error_t duckLisp_init(duckLisp_t *duckLisp, void *memory, dl_size_t size);
void duckLisp_quit(duckLisp_t *duckLisp);

#endif // DUCKLISP_H
