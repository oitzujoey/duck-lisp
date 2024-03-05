
#ifndef DUCKLIB_MEMORY_H
#define DUCKLIB_MEMORY_H

#include "core.h"
#ifndef USE_DUCKLIB_MALLOC
#include <stdlib.h>
#endif /* USE_DUCKLIB_MALLOC */

typedef struct {
	void *block;
	dl_ptrdiff_t previousBlock;
	dl_ptrdiff_t nextBlock;
	dl_size_t block_size;
	dl_bool_t allocated;
	dl_bool_t unlinked;
} dl_memoryBlock_t;

// As described here:
// https://www.geeksforgeeks.org/partition-allocation-methods-in-memory-management/
typedef enum {
	dl_memoryFit_first,
	dl_memoryFit_next,
	dl_memoryFit_best,
	dl_memoryFit_worst
} dl_memoryFit_t;

typedef struct dl_memoryAllocation_s {
	void *memory;
	dl_size_t size;
	
	dl_memoryFit_t fit;
	dl_ptrdiff_t mostRecentBlock;
	
	// Firstblock will always equal zero.
	dl_ptrdiff_t firstBlock;
	// // Lastblock may change a lot.
	// dl_ptrdiff_t lastBlock;
	
	dl_memoryBlock_t *blockList;
	dl_size_t blockList_length;
	dl_ptrdiff_t blockList_indexOfBlockList;
	dl_size_t max_used;
	dl_size_t used;
} dl_memoryAllocation_t;

dl_error_t DECLSPEC dl_memory_init(dl_memoryAllocation_t *memoryAllocation, void *memory, dl_size_t size, dl_memoryFit_t fit);
void DECLSPEC dl_memory_quit(dl_memoryAllocation_t *memoryAllocation);
/* void DECLSPEC dl_memory_printMemoryAllocation(dl_memoryAllocation_t memoryAllocation); */
/* dl_error_t DECLSPEC dl_memory_checkHealth(dl_memoryAllocation_t memoryAllocation); */

#ifdef USE_DUCKLIB_MALLOC
dl_error_t DECLSPEC dl_malloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size);
dl_error_t DECLSPEC dl_free(dl_memoryAllocation_t *memoryAllocation, void **memory);
dl_error_t DECLSPEC dl_realloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size);
void dl_memory_usage(dl_size_t *bytes, const dl_memoryAllocation_t memoryAllocation);
#else
dl_error_t DECLSPEC dl_malloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size);
dl_error_t DECLSPEC dl_free(dl_memoryAllocation_t *memoryAllocation, void **memory);
dl_error_t DECLSPEC dl_realloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size);
void dl_memory_usage(dl_size_t *bytes, const dl_memoryAllocation_t memoryAllocation);
#endif

#define DL_MALLOC(memoryAllocation, memory, size, type) dl_malloc(memoryAllocation, (void **) memory, (size) * sizeof(type))
#define DL_REALLOC(memoryAllocation, memory, size, type) dl_realloc(memoryAllocation, (void **) memory, (size) * sizeof(type))
#define DL_FREE(memoryAllocation, memory) dl_free(memoryAllocation, (void **) memory)

#endif // DUCKLIB_MEMORY_H
