
#include "duckLisp.h"
#include "DuckLib/memory.h"

typedef struct {
	
} duck_astNode_t;

typedef struct {
	duck_astNode_t *nodes;
	dl_size_t nodes_length;
} duck_ast_t;

typedef struct {
	dl_memory_t memory;
} duck_t;

static void *duck_malloc(dl_size_t size) {
	
}

duck_t duck_init(void *memory, dl_size_t size) {
	
	duck_t duck;
	
	duck.memory = memory;
	duck.memory_length = size;
	
	return duck;
}

void duck_quit(void) {

}

void duck_loadString(const char *source) {
	
}
