
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include "../DuckLib/core.h"
#include "../DuckLib/memory.h"

int main(int argc, char *argv[]) {
	dl_error_t error = dl_error_ok;

	const size_t size = 1024 * 1024;
	void *memory = NULL;
	dl_memoryAllocation_t memoryAllocation;

#define memories_length ((dl_size_t) 10)
	unsigned char *memories[memories_length] = {0};
	unsigned char *malloc_memories[memories_length] = {0};
	size_t memories_lengths[memories_length] = {0};
	
	unsigned int randomNumbers[3] = {0};
	
	memory = malloc(size);
	if (memory == NULL) {
		error = dl_error_outOfMemory;
		printf("Could not allocate memory using malloc. (%s)\n", dl_errorString[error]);
		goto l_cleanup;
	}
	
	error = dl_memory_init(&memoryAllocation, memory, size, dl_memoryFit_best);
	if (error) {
		printf("Could not initialize memory allocator. (%s)\n", dl_errorString[error]);
		goto l_cleanup;
	}
	
	printf("0x%X\n", memoryAllocation.memory);

	srand(time(NULL));
	
	for (unsigned long long i = 0; i < 100; i++) {
		
		// randomNumbers[0] = rand() % 2;
		randomNumbers[1] = rand() % memories_length;
		randomNumbers[2] = rand() % 1000;
		
		printf("randomNumbers %u %u %u\n", randomNumbers[0], randomNumbers[1], randomNumbers[2]);
		
		if (malloc_memories[randomNumbers[1]] == NULL) {
			error = dl_malloc(&memoryAllocation, &((void **) memories)[randomNumbers[1]], randomNumbers[2]);
			if (error) {
				printf("dl_malloc: Out of memory. (%s)\n", dl_errorString[error]);
				goto l_cleanup;
			}
			malloc_memories[randomNumbers[1]] = malloc(randomNumbers[2]);
			if (malloc_memories[randomNumbers[1]] == NULL) {
				printf("malloc: Out of memory. (%s)\n", dl_errorString[error]);
				error = dl_error_outOfMemory;
				goto l_cleanup;
			}
			
			// Stick junk in them.
			for (size_t j = 0; j < randomNumbers[2]; j++) {
				malloc_memories[randomNumbers[1]][j] = memories[randomNumbers[1]][j] = rand() % 0x100;
			}
			memories_lengths[randomNumbers[1]] = randomNumbers[2];
		}
		else {
			// Check for equality.
			for (size_t j = 0; j < memories_lengths[randomNumbers[1]]; j++) {
				if ( malloc_memories[randomNumbers[1]][j] != memories[randomNumbers[1]][j]) {
					printf("Failed: Malloc byte %zu:%zu/%zu = %u while dl_malloc byte %zu:%zu/%zu = %u\n",
						randomNumbers[1], j, memories_lengths[randomNumbers[1]], malloc_memories[randomNumbers[1]][j],
						randomNumbers[1], j, memories_lengths[randomNumbers[1]], memories[randomNumbers[1]][j]);
					dl_memory_printMemoryAllocation(memoryAllocation);
					goto l_cleanup;
				}
			}
			
			error = dl_free(&memoryAllocation, &((void **) memories)[randomNumbers[1]]);
			if (error) {
				printf("dl_free: Error freeing memory. (%s)\n", dl_errorString[error]);
				goto l_cleanup;
			}
			free(malloc_memories[randomNumbers[1]]); malloc_memories[randomNumbers[1]] = NULL;
		}
	}

	dl_memory_quit(&memoryAllocation);

	error = dl_error_ok;
	l_cleanup:
	
	free(memory); memory = NULL;

	return error;
}
