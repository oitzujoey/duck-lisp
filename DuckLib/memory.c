
#include "memory.h"
#include "core.h"
#ifdef MEMCHECK
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#endif /* MEMCHECK */

/* const dl_uint8_t redZoneSize = 8; */


dl_error_t dl_memory_init(dl_memoryAllocation_t *memoryAllocation, void *memory, dl_size_t size, dl_memoryFit_t fit) {
	dl_error_t error;

	// Might as well since we are only doing it once.
	if ((memoryAllocation == dl_null) || (memory == dl_null)) {
		error = dl_error_nullPointer;
		goto cleanup;
	}

	// Initialize.
	memoryAllocation->memory = memory;
	memoryAllocation->size = size;

#ifdef MEMCHECK
    VALGRIND_MAKE_MEM_NOACCESS(memoryAllocation->memory, memoryAllocation->size);
#endif /* MEMCHECK */

	memoryAllocation->blockList = memory;
	/*
	0   Block list
	1   Unallocated block
	*/
	memoryAllocation->blockList_length = 2;
	memoryAllocation->blockList_indexOfBlockList = 0;

	if (memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t) > memoryAllocation->size) {
		error = dl_error_outOfMemory;
		goto cleanup;
	}

#ifdef MEMCHECK
    VALGRIND_MAKE_MEM_DEFINED(memoryAllocation->blockList, memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t));
#endif /* MEMCHECK */


	// Our allocations table is our first allocation.
	memoryAllocation->blockList[0].block = memory;
	memoryAllocation->blockList[0].block_size = memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t);
	memoryAllocation->blockList[0].allocated = dl_true;

	memoryAllocation->blockList[0].previousBlock = -1;
	// Next block is the massive chunk of unallocated memory.
	memoryAllocation->blockList[0].nextBlock = 1;
	memoryAllocation->blockList[0].unlinked = dl_false;


	// Stick the block after the block list.
	memoryAllocation->blockList[1].block = (char*)memory + memoryAllocation->blockList[0].block_size;
	// Claim the rest of the block.
	memoryAllocation->blockList[1].block_size = size - memoryAllocation->blockList[0].block_size;
	if (memoryAllocation->blockList[1].block_size == 0) {
		error = dl_error_outOfMemory;
		goto cleanup;
	}
	memoryAllocation->blockList[1].allocated = dl_false;

	// Previous block is the block list.
	memoryAllocation->blockList[1].previousBlock = 0;
	memoryAllocation->blockList[1].nextBlock = -1;
	memoryAllocation->blockList[1].unlinked = dl_false;


	// First block is the block list.
	memoryAllocation->firstBlock = 0;
	// // Last block is the block of unallocated memory.
	// memoryAllocation->lastBlock = 1;


	memoryAllocation->mostRecentBlock = 0;
	memoryAllocation->fit = fit;


	memoryAllocation->used = memoryAllocation->blockList[0].block_size;
	memoryAllocation->max_used = memoryAllocation->used;

	error = dl_error_ok;
 cleanup:

	if (error) {
		memoryAllocation = dl_null;
	}

	return error;
}

// Actually optional if you don't care about dangling pointers.
void dl_memory_quit(dl_memoryAllocation_t *memoryAllocation) {
	memoryAllocation->memory = dl_null;
	memoryAllocation->size = 0;
	memoryAllocation->blockList = dl_null;
	memoryAllocation->blockList_length = 0;
	memoryAllocation->blockList_indexOfBlockList = -1;
	memoryAllocation->firstBlock = -1;
	// memoryAllocation->lastBlock = -1;
	memoryAllocation->mostRecentBlock = -1;
	// memoryAllocation->fit = 
}

/* void dl_memory_printMemoryAllocation(dl_memoryAllocation_t memoryAllocation) { */

/* // Console colors */
/* #define COLOR_NORMAL    "\x1B[0m" */
/* #define COLOR_BLACK     "\x1B[30m" */
/* #define COLOR_RED       "\x1B[31m" */
/* #define COLOR_GREEN     "\x1B[32m" */
/* #define COLOR_YELLOW    "\x1B[33m" */
/* #define COLOR_BLUE      "\x1B[34m" */
/* #define COLOR_MAGENTA   "\x1B[35m" */
/* #define COLOR_CYAN      "\x1B[36m" */
/* #define COLOR_WHITE     "\x1B[37m" */

/* // Console background colors */
/* #define B_COLOR_BLACK     "\x1B[40m" */
/* #define B_COLOR_RED       "\x1B[41m" */
/* #define B_COLOR_GREEN     "\x1B[42m" */
/* #define B_COLOR_YELLOW    "\x1B[43m" */
/* #define B_COLOR_BLUE      "\x1B[44m" */
/* #define B_COLOR_MAGENTA   "\x1B[45m" */
/* #define B_COLOR_CYAN      "\x1B[46m" */
/* #define B_COLOR_WHITE     "\x1B[47m" */

/* 	fflush(stdout); fflush(stderr); */
	
/* 	printf("(dl_memoryAllocation_t) {\n"); */
/* 	printf("\t.memory = %llX,\n", (unsigned long long) memoryAllocation.memory); */
/* 	printf("\t.size = %llu,\n", memoryAllocation.size); */
/* 	printf("\t.fit = %s,\n", */
/* 		((memoryAllocation.fit == dl_memoryFit_first) ? */
/* 			"dl_memoryFit_first" */
/* 		: ((memoryAllocation.fit == dl_memoryFit_next) ? */
/* 			"dl_memoryFit_next" */
/* 		: ((memoryAllocation.fit == dl_memoryFit_best) ? */
/* 			"dl_memoryFit_best" */
/* 		: ((memoryAllocation.fit == dl_memoryFit_worst) ? */
/* 			"dl_memoryFit_worst" */
/* 		: */
/* 			"Illegal value")))) */
/* 	); */
/* 	printf("\t.mostRecentBlock = %lli,\n", memoryAllocation.mostRecentBlock); */
/* 	printf("\t.firstBlock = %lli,\n", memoryAllocation.firstBlock); */
/* 	// printf("\t.lastBlock = %lli,\n", memoryAllocation.lastBlock); */
/* 	printf("\t.blockList_length = %llu,\n", memoryAllocation.blockList_length); */
/* 	printf("\t.blockList_indexOfBlockList = %lli,\n", memoryAllocation.blockList_indexOfBlockList); */
/* 	printf("\t.blockList_blockList = { /\* %llX *\/\n", (unsigned long long) memoryAllocation.blockList); */
/* 	for (dl_size_t i = 0; i < memoryAllocation.blockList_length; i++) { */
/* 		fflush(stdout); fflush(stderr); */
/* 		if (memoryAllocation.blockList[i].allocated) { */
/* 			if (memoryAllocation.blockList[i].unlinked) { */
/* 				printf(COLOR_RED); */
/* 			} */
/* 			else { */
/* 				printf(COLOR_GREEN); */
/* 			} */
/* 		} */
/* 		else { */
/* 			if (memoryAllocation.blockList[i].unlinked) { */
/* 				printf(COLOR_MAGENTA); */
/* 			} */
/* 			else { */
/* 				printf(COLOR_YELLOW); */
/* 			} */
/* 		} */
/* 		printf("\t\t(dl_memoryBlock_t) { /\* %llu *\/\n", i); */
/* 		printf("\t\t\t.block = %llX, /\* offset = %llu *\/\n", (unsigned long long) memoryAllocation.blockList[i].block, (unsigned long long) ((char *) memoryAllocation.blockList[i].block - (char *) memoryAllocation.memory)); */
/* 		printf("\t\t\t.block_size = %llu,\n", memoryAllocation.blockList[i].block_size); */
/* 		printf("\t\t\t.allocated = %s,\n", memoryAllocation.blockList[i].allocated ? "true" : "false"); */
/* 		printf("\t\t\t.unlinked = %s,\n", memoryAllocation.blockList[i].unlinked ? "true" : "false"); */
/* 		printf("\t\t\t.previousBlock = %lli,\n", memoryAllocation.blockList[i].previousBlock); */
/* 		printf("\t\t\t.nextBlock = %lli,\n", memoryAllocation.blockList[i].nextBlock); */
/* 		printf("\t\t},\n"); */
/* 		printf(COLOR_NORMAL); */
/* 		fflush(stdout); fflush(stderr); */
/* 	} */
/* 	printf("\t},\n"); */
/* 	printf("}\n"); */
/* } */

/* dl_error_t dl_memory_checkHealth(dl_memoryAllocation_t memoryAllocation) { */
/* 	dl_error_t error = dl_error_ok; */
	
/* 	dl_memoryBlock_t blockListEntry; */
	
/* 	/\* Main structure checks *\/ */
	
/* 	// fit */
/* 	if ((memoryAllocation.fit < 0) || (memoryAllocation.fit > dl_memoryFit_worst)) { */
/* 		printf("memoryAllocation.fit is out of range: %i\n", memoryAllocation.fit); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// mostRecentBlock */
/* 	if ((memoryAllocation.mostRecentBlock < -1) || (memoryAllocation.mostRecentBlock >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 		printf("memoryAllocation.mostRecentBlock is out of range: %lli\n", memoryAllocation.mostRecentBlock); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// firstBlock */
/* 	if ((memoryAllocation.firstBlock < -1) || (memoryAllocation.firstBlock >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 		printf("memoryAllocation.firstBlock is out of range: %lli\n", memoryAllocation.firstBlock); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// // lastBlock */
/* 	// if ((memoryAllocation.lastBlock < -1) || (memoryAllocation.lastBlock >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 	// 	printf("memoryAllocation.lastBlock is out of range: %lli\n", memoryAllocation.lastBlock); */
/* 	// 	error = dl_error_invalidValue; */
/* 	// 	goto l_cleanup; */
/* 	// } */
	
/* 	// blockList */
/* 	if (((unsigned char *) memoryAllocation.blockList < (unsigned char *) memoryAllocation.memory) */
/* 	 || ((unsigned char *) memoryAllocation.blockList >= (unsigned char *) memoryAllocation.memory + memoryAllocation.size)) { */
/* 		printf("memoryAllocation.blockList is out of range: %llu\n", (unsigned long long) memoryAllocation.blockList); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// blockList_length */
/* 	if (((unsigned char *) memoryAllocation.blockList + memoryAllocation.blockList_length * sizeof(dl_memoryBlock_t)) */
/* 	   > (unsigned char *) memoryAllocation.memory + memoryAllocation.size) { */
/* 		printf("memoryAllocation.blockList_length is out of range: %llu\n", memoryAllocation.blockList_length); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// blockList_indexOfBlockList */
/* 	if ((memoryAllocation.blockList_indexOfBlockList < -1) || (memoryAllocation.blockList_indexOfBlockList >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 		printf("memoryAllocation.blockList_indexOfBlockList is out of range: %lli\n", memoryAllocation.blockList_indexOfBlockList); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	/\* Block list checks *\/ */
/* 	blockListEntry = memoryAllocation.blockList[memoryAllocation.blockList_indexOfBlockList]; */
	
/* 	// First check the block list's self reference. */
	
/* 	// previousBlock */
/* 	if ((blockListEntry.previousBlock < -1) || (blockListEntry.previousBlock >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 		printf("memoryAllocation.blockList[memoryAllocation.blockList_indexOfBlockList].previousBlock is out of range: %lli\n", blockListEntry.previousBlock); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// nextBlock */
/* 	if ((blockListEntry.nextBlock < -1) || (blockListEntry.nextBlock >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 		printf("memoryAllocation.blockList[memoryAllocation.blockList_indexOfBlockList].nextBlock is out of range: %lli\n", blockListEntry.nextBlock); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// block_size */
/* 	if (blockListEntry.block_size != memoryAllocation.blockList_length * sizeof(dl_memoryBlock_t)) { */
/* 		printf("memoryAllocation.blockList[memoryAllocation.blockList_indexOfBlockList].block_size is not equal to memoryAllocation.blockList_length: %llu %llu\n", */
/* 			   blockListEntry.block_size, */
/* 			   memoryAllocation.blockList_length * sizeof(dl_memoryBlock_t)); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// block */
/* 	if ((unsigned char *) blockListEntry.block != (unsigned char *) memoryAllocation.blockList) { */
/* 		printf("memoryAllocation.blockList[memoryAllocation.blockList_indexOfBlockList].block is not equal to memoryAllocation.blockList: %llX %llX\n", */
/* 			   (unsigned long long) blockListEntry.block, (unsigned long long) memoryAllocation.blockList); */
/* 		error = dl_error_invalidValue; */
/* 		goto l_cleanup; */
/* 	} */
	
/* 	// Second, check the rest of block list. */
	
/* 	for (dl_size_t i = 0; i < memoryAllocation.blockList_length; i++) { */
/* 		blockListEntry = memoryAllocation.blockList[i]; */
		
/* 		if (blockListEntry.unlinked) { */
/* 			continue; */
/* 		} */
		
/* 		// previousBlock */
/* 		if ((blockListEntry.previousBlock < -1) || (blockListEntry.previousBlock >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 			printf("memoryAllocation.blockList[%llu].previousBlock is out of range: %lli\n", i, blockListEntry.previousBlock); */
/* 			error = dl_error_invalidValue; */
/* 			goto l_cleanup; */
/* 		} */
		
/* 		// nextBlock */
/* 		if ((blockListEntry.nextBlock < -1) || (blockListEntry.nextBlock >= (dl_ptrdiff_t) memoryAllocation.blockList_length)) { */
/* 			printf("memoryAllocation.blockList[%llu].nextBlock is out of range: %lli\n", i, blockListEntry.nextBlock); */
/* 			error = dl_error_invalidValue; */
/* 			goto l_cleanup; */
/* 		} */
		
/* 		// block */
/* 		if (((unsigned char *) blockListEntry.block < (unsigned char *) memoryAllocation.memory) */
/* 		 || ((unsigned char *) blockListEntry.block > (unsigned char *) memoryAllocation.memory + memoryAllocation.size)) { */
/* 			printf("memoryAllocation.blockList[%llu].block is outsize: %llu\n", */
/* 				   i, (unsigned long long) blockListEntry.block); */
/* 			error = dl_error_invalidValue; */
/* 			goto l_cleanup; */
/* 		} */
		
/* 		// block_size */
/* 		if (blockListEntry.nextBlock != -1) { */
/* 			if ((unsigned char *) blockListEntry.block + blockListEntry.block_size */
/* 			  > (unsigned char *) memoryAllocation.blockList[blockListEntry.nextBlock].block) { */
/* 				printf("memoryAllocation.blockList[%llu].block_size overlaps with another block: block_size %llu, nextBlock %llu\n", */
/* 				    i, blockListEntry.block_size, blockListEntry.nextBlock); */
/* 				error = dl_error_invalidValue; */
/* 				goto l_cleanup; */
/* 			} */
/* 			else if ((unsigned char *) blockListEntry.block + blockListEntry.block_size */
/* 			  < (unsigned char *) memoryAllocation.blockList[blockListEntry.nextBlock].block) { */
/* 				printf("memoryAllocation.blockList[%llu].block_size leaves dead space between another block: block_size %llu, nextBlock %llu\n", */
/* 				    i, blockListEntry.block_size, blockListEntry.nextBlock); */
/* 				error = dl_error_invalidValue; */
/* 				goto l_cleanup; */
/* 			} */
/* 		} */
/* 		else { */
/* 			if ((unsigned char *) blockListEntry.block + blockListEntry.block_size */
/* 			  > (unsigned char *) memoryAllocation.memory + memoryAllocation.size) { */
/* 				printf("memoryAllocation.blockList[%llu].block_size exceeds the bounds of allocated memory: %llu\n", */
/* 				    i, blockListEntry.block_size); */
/* 				error = dl_error_invalidValue; */
/* 				goto l_cleanup; */
/* 			} */
/* 		} */
/* 	} */
	
/* 	error = dl_error_ok; */
/* 	l_cleanup: */
	
/* 	return error; */
/* } */

dl_error_t dl_memory_pointerToBlock(dl_memoryAllocation_t memoryAllocation, dl_ptrdiff_t *block, void **memory) {
	dl_error_t error = dl_error_ok;
	
	*block = -1;
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < memoryAllocation.blockList_length; i++) {
		if (!memoryAllocation.blockList[i].unlinked && (memoryAllocation.blockList[i].block == *memory)) {
			*block = i;
		}
	}
	if (*block == -1) {
		error = dl_error_danglingPointer;
		goto l_cleanup;
	}
	
	error = dl_error_ok;
	l_cleanup:
	
	return error;
}

// Ask specifically for fit, because who knows, maybe there's exceptions.
dl_error_t dl_memory_findBlock(dl_memoryAllocation_t *memoryAllocation, dl_ptrdiff_t *block, dl_size_t size, dl_memoryFit_t fit) {
	dl_error_t error = dl_error_ok;
	
	dl_ptrdiff_t optimumBlock = -1;
	dl_ptrdiff_t currentBlock = -1;
	dl_bool_t found = dl_false;
	
	if (fit == dl_memoryFit_next) {
		currentBlock = memoryAllocation->blockList[memoryAllocation->mostRecentBlock].nextBlock;
	}
	else {
		currentBlock = memoryAllocation->firstBlock;
	}
	
	// Find the smallest of the empty blocks.
	while (!found && (currentBlock != -1)) {
		if (!memoryAllocation->blockList[currentBlock].allocated && (memoryAllocation->blockList[currentBlock].block_size >= size)) {
			if (optimumBlock == -1) {
				optimumBlock = currentBlock;
				found = dl_true;
			}
			else switch (fit) {
			case dl_memoryFit_first:
				optimumBlock = currentBlock;
				break;
			
			// case dl_memoryFit_next:
			// 	optimumBlock = currentBlock;
			// 	break;
			
			case dl_memoryFit_best:
				if (memoryAllocation->blockList[currentBlock].block_size < memoryAllocation->blockList[optimumBlock].block_size) {
					optimumBlock = currentBlock;
				}
				break;
			
			case dl_memoryFit_worst:
				if (memoryAllocation->blockList[currentBlock].block_size > memoryAllocation->blockList[optimumBlock].block_size) {
					optimumBlock = currentBlock;
				}
				break;
			
			default:
				error = dl_error_shouldntHappen;
				goto l_cleanup;
			}
		}
		currentBlock = memoryAllocation->blockList[currentBlock].nextBlock;
	}
	
	if (optimumBlock == -1) {
		error = dl_error_outOfMemory;
		goto l_cleanup;
	}
	
	error = dl_error_ok;
	l_cleanup:
	
	if (error) {
		*block = -1;
	}
	else {
		*block = optimumBlock;
	}
	
	return error;
}

void dl_memory_mergeBlockAfter(dl_memoryAllocation_t *memoryAllocation, dl_bool_t *merged, dl_ptrdiff_t block) {
	
	dl_ptrdiff_t nextBlock = memoryAllocation->blockList[block].nextBlock;
	
	if ((nextBlock != -1) && !memoryAllocation->blockList[nextBlock].allocated) {
		// Increase size of *block.
		memoryAllocation->blockList[block].block_size += memoryAllocation->blockList[nextBlock].block_size;
		// Unlink nextBlock.
		memoryAllocation->blockList[block].nextBlock = memoryAllocation->blockList[nextBlock].nextBlock;
		if (memoryAllocation->blockList[block].nextBlock != -1) {
		// 	memoryAllocation->lastBlock = block;
		// }
		// else {
			memoryAllocation->blockList[memoryAllocation->blockList[block].nextBlock].previousBlock = block;
		}
		// Free nextBlock's entry.
		memoryAllocation->blockList[nextBlock].unlinked = dl_true;
		
		if (merged) {
			*merged = dl_true;
		}
	}
	else {
		if (merged) {
			*merged = dl_false;
		}
	}
}

void dl_memory_mergeBlockBefore(dl_memoryAllocation_t *memoryAllocation, dl_bool_t *merged, dl_ptrdiff_t block) {
	
	dl_ptrdiff_t previousBlock = memoryAllocation->blockList[block].previousBlock;
	
	if ((previousBlock != -1) && !memoryAllocation->blockList[previousBlock].allocated) {
		// Increase size of previousBlock.
		memoryAllocation->blockList[block].block_size += memoryAllocation->blockList[previousBlock].block_size;
		memoryAllocation->blockList[block].block = (char*)memoryAllocation->blockList[block].block - memoryAllocation->blockList[previousBlock].block_size;
		
		memoryAllocation->blockList[block].previousBlock = memoryAllocation->blockList[previousBlock].previousBlock;
		if (memoryAllocation->blockList[block].previousBlock == -1) {
			memoryAllocation->firstBlock = block;
		}
		else {
			memoryAllocation->blockList[memoryAllocation->blockList[block].previousBlock].nextBlock = block;
		}
		// Free previousBlock's entry.
		memoryAllocation->blockList[previousBlock].unlinked = dl_true;
		
		if (merged) {
			*merged = dl_true;
		}
	}
	else {
		if (merged) {
			*merged = dl_false;
		}
	}
}

// We simply mark deleted block descriptors as unlinked because dl_memory_pushBlockEntry uses this function and we want to touch the table as little as possible.
void dl_memory_mergeBlocks(dl_memoryAllocation_t *memoryAllocation, dl_bool_t *merged, dl_ptrdiff_t block) {
	dl_memory_mergeBlockAfter(memoryAllocation, merged, block);
	dl_memory_mergeBlockBefore(memoryAllocation, merged, block);
}

// // Remove unlinked block descriptors.
// dl_error_t dl_memory_cleanBlockList(dl_memoryAllocation_t *memoryAllocation) {
// 	dl_error_t error = dl_error_ok;
	
// 	dl_ptrdiff_t previousBlock = -1;
// 	dl_ptrdiff_t nextBlock = -1;
	
// 	// This is about the one time we can do this with the block list.
// 	for (dl_size_t i = 0; i < memoryAllocation->blockList_length; i++) {
// 		if (memoryAllocation->blockList[i].unlinked) {
// 			previousBlock = memoryAllocation->blockList[i].previousBlock;
// 			nextBlock = memoryAllocation->blockList[i].nextBlock;
			
// 			if (previousBlock != -1) {
// 				memoryAllocation->blockList[previousBlock].
// 			}
// 		}
// 	}
	
// 	error = dl_error_ok;
// 	l_cleanup:
	
// 	return error;
// }

dl_error_t dl_memory_reserveTableEntries(dl_memoryAllocation_t *memoryAllocation, dl_size_t entries_number) {
	dl_error_t error = dl_error_ok;
	
	dl_size_t entries_left = entries_number;
	dl_size_t unlinkedBlock_number = 0;
	dl_ptrdiff_t newBlock = -1;
	dl_bool_t merged = dl_false;
	void *tempMemory = dl_null;
	dl_ptrdiff_t extraBlock = -1;
	dl_memoryBlock_t *blockListEntry = dl_null;
	
	// Use unlinked descriptors if possible.
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < memoryAllocation->blockList_length; i++) {
		if (memoryAllocation->blockList[i].unlinked) {
			unlinkedBlock_number++;
		}
	}
	
	entries_left = (entries_left > unlinkedBlock_number) ? entries_left - unlinkedBlock_number : 0;
	
	// If we still need descriptors, then realloc the table.
	if (entries_left) {
		
		memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].allocated = dl_false;
		
		if (memoryAllocation->blockList[memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].nextBlock].nextBlock == -1) {
			memoryAllocation->used = (char *) memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].block - (char *) memoryAllocation->memory;
		}
		
		
		/* No error */ dl_memory_mergeBlockAfter(memoryAllocation, &merged, memoryAllocation->blockList_indexOfBlockList);
		
		if (memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].block_size
		    < (memoryAllocation->blockList_length + entries_left) * sizeof(dl_memoryBlock_t)) {
			
			// Need to move.
			
			entries_left++;
			if (merged) {
				--entries_left;
			}
			
			/* No error */ dl_memory_mergeBlockBefore(memoryAllocation, &merged, memoryAllocation->blockList_indexOfBlockList);
			
			if (merged) {
				entries_left = (entries_left > 1) ? entries_left - 1 : 0;
			}
		
			// Find a larger memory block for the table.
			error = dl_memory_findBlock(memoryAllocation, &newBlock, (memoryAllocation->blockList_length + entries_left) * sizeof(dl_memoryBlock_t),
			                            memoryAllocation->fit);
			if (error) goto cleanup;
			
			// Copy block list to new location.
			tempMemory = memoryAllocation->blockList[newBlock].block;

#ifdef MEMCHECK
			VALGRIND_MAKE_MEM_UNDEFINED(tempMemory, memoryAllocation->blockList[newBlock].block_size);
#endif /* MEMCHECK */
			(void) dl_memcopy(tempMemory,
			                  memoryAllocation->blockList,
			                  memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t));
#ifdef MEMCHECK
			VALGRIND_MAKE_MEM_NOACCESS(memoryAllocation->blockList, memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t));
#endif /* MEMCHECK */
			
			// Transfer control to the new block.
			memoryAllocation->blockList = tempMemory;
			memoryAllocation->blockList_indexOfBlockList = newBlock;
		}
		
		memoryAllocation->blockList_length += entries_left;
#ifdef MEMCHECK
		VALGRIND_MAKE_MEM_DEFINED(memoryAllocation->blockList, memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t));
#endif /* MEMCHECK */
		
		// Initialize new blocks to unlinked.
		for (dl_size_t i = 0; i < entries_left; i++) {
			memoryAllocation->blockList[i + (memoryAllocation->blockList_length - entries_left)].unlinked = dl_true;
		}
		
		if (memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].block_size
		    > memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t)) {
			
			blockListEntry = &memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList];
			
			// Split block.
			
			// Find unlinked descriptor.
			for (dl_ptrdiff_t i = memoryAllocation->blockList_length - 1; i >= 0; --i) {
				if (memoryAllocation->blockList[i].unlinked) {
					extraBlock = i;
					break;
				}
			}
			if (extraBlock == -1) {
				error = dl_error_cantHappen;
				goto cleanup;
			}
			
			memoryAllocation->blockList[extraBlock].block = (unsigned char *) blockListEntry->block
			                                              + memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t);
			// Decrease size of blocks.
			memoryAllocation->blockList[extraBlock].block_size = blockListEntry->block_size - memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t);
			blockListEntry->block_size = memoryAllocation->blockList_length * sizeof(dl_memoryBlock_t);
			
			// Link extra block after block list block.
			memoryAllocation->blockList[extraBlock].nextBlock = blockListEntry->nextBlock;
			if (blockListEntry->nextBlock != -1) {
			// 	memoryAllocation->lastBlock = extraBlock;
			// }
			// else {
				memoryAllocation->blockList[blockListEntry->nextBlock].previousBlock = extraBlock;
			}
			memoryAllocation->blockList[extraBlock].previousBlock = memoryAllocation->blockList_indexOfBlockList;
			blockListEntry->nextBlock = extraBlock;
			memoryAllocation->blockList[extraBlock].unlinked = dl_false;
			
			memoryAllocation->blockList[extraBlock].allocated = dl_false;
			
			entries_left = (entries_left > 1) ? entries_left - 1 : 0;
		}
		
		memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].allocated = dl_true;
		
		memoryAllocation->used = dl_max(memoryAllocation->used, (dl_size_t) (memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].block_size + (char *) memoryAllocation->blockList[memoryAllocation->blockList_indexOfBlockList].block - (char *) memoryAllocation->memory));
		memoryAllocation->max_used = dl_max(memoryAllocation->max_used, memoryAllocation->used);
	}
	
	error = dl_error_ok;
 cleanup: return error;
}

// New block will be at the address (*block)->nextBlock after the function returns.
dl_error_t dl_memory_splitBlock(dl_memoryAllocation_t *memoryAllocation, dl_ptrdiff_t block, dl_size_t index) {
	dl_error_t error = dl_error_ok;
	
	dl_ptrdiff_t unlinkedBlock = -1;
	
	/*
	What we are trying to do here is find a safe place in memory to put the
	block list. After that, we copy the block list to its new home. We of course
	leave some room for a block descriptor at the end of the list, but we don't
	touch it yet, mainly because I want to keep this complicated section as
	simple as possible. I may have failed in that endeavor.
	*/
	
	/*
	Find an unlinked descriptor for the new block. This should have been
	allocated beforehand, hence the error.
	*/
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < memoryAllocation->blockList_length; i++) {
		if (memoryAllocation->blockList[i].unlinked) {
			unlinkedBlock = i;
			break;
		}
	}
	if (unlinkedBlock == -1) {
		error = dl_error_shouldntHappen;
		goto l_cleanup;
	}

	memoryAllocation->blockList[unlinkedBlock].allocated = dl_false;

	memoryAllocation->blockList[unlinkedBlock].block = (unsigned char *) memoryAllocation->blockList[block].block + index;
	memoryAllocation->blockList[unlinkedBlock].block_size = memoryAllocation->blockList[block].block_size - index;
	memoryAllocation->blockList[block].block_size = index;

	memoryAllocation->blockList[unlinkedBlock].nextBlock = memoryAllocation->blockList[block].nextBlock;
	if (memoryAllocation->blockList[block].nextBlock != -1) {
		memoryAllocation->blockList[memoryAllocation->blockList[block].nextBlock].previousBlock = unlinkedBlock;
	}
	memoryAllocation->blockList[unlinkedBlock].previousBlock = block;
	memoryAllocation->blockList[block].nextBlock = unlinkedBlock;
	memoryAllocation->blockList[unlinkedBlock].unlinked = dl_false;

	error = dl_error_ok;
	l_cleanup:

	return error;
}

#ifdef USE_DUCKLIB_MALLOC
dl_error_t dl_malloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size) {
	dl_error_t error = dl_error_ok;

	dl_ptrdiff_t block = -1;

	if (size == 0) {
		error = dl_error_invalidValue;
		goto cleanup;
	}

	size += DL_ALIGNMENT - (size & (DL_ALIGNMENT - 1));

	error = dl_memory_reserveTableEntries(memoryAllocation, 1);
	if (error) goto cleanup;

	// Find a fitting block.
	error = dl_memory_findBlock(memoryAllocation, &block, size, memoryAllocation->fit);
	if (error) goto cleanup;

	// Split.
	if (memoryAllocation->blockList[block].block_size != size) {
		error = dl_memory_splitBlock(memoryAllocation, block, size);
		if (error) goto cleanup;
	}

	// Mark allocated.
	memoryAllocation->blockList[block].allocated = dl_true;

	// Pass the memory.
	*memory = memoryAllocation->blockList[block].block;
	memoryAllocation->used = dl_max(memoryAllocation->used, (dl_size_t) (memoryAllocation->blockList[block].block_size + (char *) memoryAllocation->blockList[block].block - (char *) memoryAllocation->memory));
	memoryAllocation->max_used = dl_max(memoryAllocation->max_used, memoryAllocation->used);

#ifdef MEMCHECK
	VALGRIND_MAKE_MEM_UNDEFINED(*memory, size);
#endif /* MEMCHECK */

	error = dl_error_ok;

 cleanup:

	if (error) memory = dl_null;

	return error;
}


// Sets the given pointer to zero after freeing the memory.
dl_error_t dl_free(dl_memoryAllocation_t *memoryAllocation, void **memory) {
	dl_error_t error = dl_error_ok;

	dl_ptrdiff_t block = -1;

	if (*memory == dl_null) {
		error = dl_error_nullPointer;
		goto l_cleanup;
	}

	error = dl_memory_pointerToBlock(*memoryAllocation, &block, memory);
	if (error || (memoryAllocation->blockList[block].allocated == dl_false)) {
		error = dl_error_danglingPointer;
		goto l_cleanup;
	}

#ifdef MEMCHECK
	VALGRIND_MAKE_MEM_NOACCESS(*memory, memoryAllocation->blockList[block].block_size);
#endif /* MEMCHECK */
		
	memoryAllocation->blockList[block].allocated = dl_false;

	if (memoryAllocation->blockList[memoryAllocation->blockList[block].nextBlock].nextBlock == -1) {
		memoryAllocation->used = (char *) memoryAllocation->blockList[block].block - (char *) memoryAllocation->memory;
	}
	
	/* No error */ dl_memory_mergeBlocks(memoryAllocation, dl_null, block);

	error = dl_error_ok;
	l_cleanup:

	*memory = dl_null;
	
	return error;
}

dl_error_t dl_realloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size) {
	dl_error_t error = dl_error_ok;
	
	dl_ptrdiff_t currentBlock = -1;
	dl_ptrdiff_t newBlock = -1;

	dl_bool_t blockFits = dl_false;
#ifdef MEMCHECK
	dl_size_t oldSize;
#endif
	
	if (*memory == dl_null) {
		error = dl_malloc(memoryAllocation, memory, size);
		goto l_cleanup;
	}

	if (size == 0) {
		error = dl_error_invalidValue;
		goto l_cleanup;
	}
	
	size += DL_ALIGNMENT - (size & (DL_ALIGNMENT - 1));
	
	error = dl_memory_pointerToBlock(*memoryAllocation, &currentBlock, memory);
	if (error || (memoryAllocation->blockList[currentBlock].allocated == dl_false)) {
		error = dl_error_danglingPointer;
		goto l_cleanup;
	}

#ifdef MEMCHECK
	oldSize = memoryAllocation->blockList[currentBlock].block_size;
#endif
	
	// Mark deleted just in case we find that we can expand our block.
	memoryAllocation->blockList[currentBlock].allocated = dl_false;
	
	if (memoryAllocation->blockList[memoryAllocation->blockList[currentBlock].nextBlock].nextBlock == -1) {
		memoryAllocation->used = (char *) memoryAllocation->blockList[currentBlock].block - (char *) memoryAllocation->memory;
	}
	
	/* No error */ dl_memory_mergeBlockAfter(memoryAllocation, dl_null, currentBlock);
	
	blockFits = (memoryAllocation->blockList[currentBlock].block_size >= size);
	
	if (!blockFits) {
		/* No error */ dl_memory_mergeBlockBefore(memoryAllocation, dl_null, currentBlock);
	}
	
	memoryAllocation->blockList[currentBlock].allocated = dl_true;
	error = dl_memory_reserveTableEntries(memoryAllocation, 1);
	if (error) {
		goto l_cleanup;
	}
	memoryAllocation->blockList[currentBlock].allocated = dl_false;
	
	
	newBlock = currentBlock;
	if (!blockFits) {
		error = dl_memory_findBlock(memoryAllocation, &newBlock, size, memoryAllocation->fit);
		if (error) {
			goto l_cleanup;
		}
	}
	
	if (memoryAllocation->blockList[newBlock].block_size != size) {
		error = dl_memory_splitBlock(memoryAllocation, newBlock, size);
		if (error) {
			goto l_cleanup;
		}
	}

	if (!blockFits) {
		// Copy.
#ifdef MEMCHECK
		VALGRIND_MAKE_MEM_UNDEFINED(memoryAllocation->blockList[newBlock].block, size);
		VALGRIND_MAKE_MEM_DEFINED(*memory, memoryAllocation->blockList[currentBlock].block_size);
#endif /* MEMCHECK */
		error = dl_memcopy(memoryAllocation->blockList[newBlock].block,
		                   *memory,
		                   dl_min(size, memoryAllocation->blockList[currentBlock].block_size));
		if (error) {
			goto l_cleanup;
		}
#ifdef MEMCHECK
		VALGRIND_MAKE_MEM_NOACCESS(*memory, oldSize);
		VALGRIND_MAKE_MEM_UNDEFINED(memoryAllocation->blockList[newBlock].block + oldSize, size - oldSize);
		VALGRIND_MAKE_MEM_DEFINED(memoryAllocation->blockList[newBlock].block, oldSize);
#endif /* MEMCHECK */
	}
#ifdef MEMCHECK
	else {
		if (size > oldSize) {
			VALGRIND_MAKE_MEM_NOACCESS(*memory, oldSize);
			VALGRIND_MAKE_MEM_UNDEFINED(memoryAllocation->blockList[newBlock].block + oldSize, size - oldSize);
			VALGRIND_MAKE_MEM_DEFINED(memoryAllocation->blockList[newBlock].block, oldSize);
		}
		else {
			VALGRIND_MAKE_MEM_NOACCESS(*memory, oldSize);
			VALGRIND_MAKE_MEM_DEFINED(memoryAllocation->blockList[newBlock].block, size);
		}
	}
#endif /* MEMCHECK */
	
	// Mark allocated.
	memoryAllocation->blockList[newBlock].allocated = dl_true;
    
	// Pass the memory.
	*memory = memoryAllocation->blockList[newBlock].block;
	memoryAllocation->used = dl_max(memoryAllocation->used, (dl_size_t) (memoryAllocation->blockList[newBlock].block_size + (char *) memoryAllocation->blockList[newBlock].block - (char *) memoryAllocation->memory));
	memoryAllocation->max_used = dl_max(memoryAllocation->max_used, memoryAllocation->used);

	error = dl_error_ok;
	l_cleanup:
	
	if (error) {
		memory = dl_null;
	}
	
	return error;
}

void dl_memory_usage(dl_size_t *bytes, const dl_memoryAllocation_t memoryAllocation) {
	const dl_memoryBlock_t *blockList = memoryAllocation.blockList;
	const dl_size_t blockList_length = memoryAllocation.blockList_length;
	dl_size_t sum = 0;
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < blockList_length; i++) {
		if (blockList[i].allocated) {
			sum += blockList[i].block_size;
		}
	}
	*bytes = sum;
}

#else /* USE_DUCKLIB_MALLOC */

dl_error_t dl_malloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size) {
	(void) memoryAllocation;
	*(dl_uint8_t **) memory = malloc(size);
	if (*(dl_uint8_t **) memory == NULL) return dl_error_outOfMemory;
	return dl_error_ok;
}

// Sets the given pointer to zero after freeing the memory.
dl_error_t dl_free(dl_memoryAllocation_t *memoryAllocation, void **memory) {
	(void) memoryAllocation;
	free(*(dl_uint8_t **) memory);
	*(dl_uint8_t **) memory = NULL;
	return dl_error_ok;
}

dl_error_t dl_realloc(dl_memoryAllocation_t *memoryAllocation, void **memory, dl_size_t size) {
	(void) memoryAllocation;
	*(dl_uint8_t **) memory = realloc(*(dl_uint8_t **) memory, size);
	if (*(dl_uint8_t **) memory == NULL) {
		return dl_error_outOfMemory;
	}
	return dl_error_ok;
}

void dl_memory_usage(dl_size_t *bytes, const dl_memoryAllocation_t memoryAllocation) {
	(void) memoryAllocation;
	*bytes = 0;
}

#endif /* USE_DUCKLIB_MALLOC */
