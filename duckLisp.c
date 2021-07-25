
#include "duckLisp.h"
#include "DuckLib/memory.h"

typedef struct {
	
} duck_astNode_t;

typedef struct {
	duck_astNode_t *nodes;
	dl_size_t nodes_length;
} duck_ast_t;

dl_error_t duckLisp_init(duckLisp_t *duckLisp, void *memory, dl_size_t size) {
	dl_error_t error = dl_error_ok;
	
	error = dl_memory_init(&duckLisp->memoryAllocation, memory, size, dl_memoryFit_best);
	if (error) {
		goto l_cleanup;
	}
	
	error = dl_error_ok;
	l_cleanup:
	
	return error;
}

void duckLisp_quit(duckLisp_t *duckLisp) {
	/* No error */ dl_memory_quit(&duckLisp->memoryAllocation);
}

void duckLisp_loadString(const char *source) {
	
}
