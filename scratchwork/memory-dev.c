
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include "../DuckLib/core.h"
#include "../DuckLib/memory.h"

int main(int argc, char *argv[]) {
	dl_error_t error = dl_error_ok;

	const size_t size = 1024 * 1024 * 1024;
	void *memory = NULL;
	dl_memoryAllocation_t memoryAllocation;
	size_t iterations;

#define memories_length ((dl_size_t) 1000)
	unsigned char *memories[memories_length] = {0};
	unsigned char *malloc_memories[memories_length] = {0};
	size_t memories_lengths[memories_length] = {0};
	
	unsigned int randomNumbers[3] = {0};
	
	char lastChar = ' ';
	
	const int lineLength = 120;
	
	if (argc != 2) {
		printf("Requires one argument.\n");
		goto l_cleanup;
	}
	iterations = strtoull(argv[1], NULL, 10);
	printf("iterations = %llu\n", iterations);
	
	memory = malloc(size);
	if (memory == NULL) {
		error = dl_error_outOfMemory;
		printf("Could not allocate memory using malloc. (%s)\n", dl_errorString[error]);
		goto l_cleanup;
	}
	
	srand(time(NULL));
	
	for (unsigned long long k = 0; k < iterations; k++) {
	
		for (size_t i = 0; i < memories_length; i++) {
			memories[i] = NULL;
			malloc_memories[i] = NULL;
			memories_lengths[i] = 0;
		}
	
		error = dl_memory_init(&memoryAllocation, memory, size, dl_memoryFit_best);
		if (error) {
			printf("Could not initialize memory allocator. (%s)\n", dl_errorString[error]);
			goto l_cleanup;
		}
		
		for (unsigned long long i = 0; i < 10000; i++) {
			
			randomNumbers[0] = rand() % 2;
			randomNumbers[1] = rand() % memories_length;
			randomNumbers[2] = (rand() % (1000 - 1)) + 1;
			
			// Realloc
			if (randomNumbers[0]) {
				if (malloc_memories[randomNumbers[1]] != NULL) {
					// Check for equality.
					for (size_t j = 0; j < memories_lengths[randomNumbers[1]]; j++) {
						if ( malloc_memories[randomNumbers[1]][j] != memories[randomNumbers[1]][j]) {
							printf("0x%X\n", memoryAllocation.memory);
							printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
							printf("Failed: Malloc byte %zu:%zu/%zu = %u while dl_malloc byte %zu:%zu/%zu = %u\n",
								randomNumbers[1], j, memories_lengths[randomNumbers[1]], malloc_memories[randomNumbers[1]][j],
								randomNumbers[1], j, memories_lengths[randomNumbers[1]], memories[randomNumbers[1]][j]);
							dl_memory_printMemoryAllocation(memoryAllocation);
							goto l_cleanup;
						}
					}
				}
				
				error = dl_realloc(&memoryAllocation, &((void **) memories)[randomNumbers[1]], randomNumbers[2]);
				if (error) {
					printf("0x%X\n", memoryAllocation.memory);
					printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
					printf("dl_realloc: Out of memory. (%s)\n", dl_errorString[error]);
					dl_memory_printMemoryAllocation(memoryAllocation);
					goto l_cleanup;
				}
				malloc_memories[randomNumbers[1]] = realloc(malloc_memories[randomNumbers[1]], randomNumbers[2]);
				if (malloc_memories[randomNumbers[1]] == NULL) {
					printf("0x%X\n", memoryAllocation.memory);
					printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
					printf("remalloc: Out of memory. (%s)\n", dl_errorString[error]);
					error = dl_error_outOfMemory;
					goto l_cleanup;
				}
				
				// Stick junk in them.
				for (size_t j = 0; j < randomNumbers[2]; j++) {
					malloc_memories[randomNumbers[1]][j] = memories[randomNumbers[1]][j] = rand() % 0x100;
				}
				memories_lengths[randomNumbers[1]] = randomNumbers[2];
			}
			// Malloc
			else if (malloc_memories[randomNumbers[1]] == NULL) {
				error = dl_malloc(&memoryAllocation, &((void **) memories)[randomNumbers[1]], randomNumbers[2]);
				if (error) {
					printf("0x%X\n", memoryAllocation.memory);
					printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
					printf("dl_malloc: Out of memory. (%s)\n", dl_errorString[error]);
					dl_memory_printMemoryAllocation(memoryAllocation);
					goto l_cleanup;
				}
				malloc_memories[randomNumbers[1]] = malloc(randomNumbers[2]);
				if (malloc_memories[randomNumbers[1]] == NULL) {
					printf("0x%X\n", memoryAllocation.memory);
					printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
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
			// Free
			else {
				// Check for equality.
				for (size_t j = 0; j < memories_lengths[randomNumbers[1]]; j++) {
					if ( malloc_memories[randomNumbers[1]][j] != memories[randomNumbers[1]][j]) {
						printf("0x%X\n", memoryAllocation.memory);
						printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
						printf("Failed: Malloc byte %zu:%zu/%zu = %u while dl_malloc byte %zu:%zu/%zu = %u\n",
							randomNumbers[1], j, memories_lengths[randomNumbers[1]], malloc_memories[randomNumbers[1]][j],
							randomNumbers[1], j, memories_lengths[randomNumbers[1]], memories[randomNumbers[1]][j]);
						dl_memory_printMemoryAllocation(memoryAllocation);
						goto l_cleanup;
					}
				}
				
				error = dl_free(&memoryAllocation, &((void **) memories)[randomNumbers[1]]);
				if (error) {
					printf("0x%X\n", memoryAllocation.memory);
					printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
					printf("dl_free: Error freeing memory. (%s)\n", dl_errorString[error]);
					goto l_cleanup;
				}
				free(malloc_memories[randomNumbers[1]]); malloc_memories[randomNumbers[1]] = NULL;
			}
			
			error = dl_memory_checkHealth(memoryAllocation);
			if (error) {
				printf("0x%X\n", memoryAllocation.memory);
				printf("%llu %u %u %u\n", i, randomNumbers[0], randomNumbers[1], randomNumbers[2]);
				printf("dl_memory_checkHealth: Health check failed. (%s)\n", dl_errorString[error]);
				dl_memory_printMemoryAllocation(memoryAllocation);
				goto l_cleanup;
			}
		}
		
		dl_memory_quit(&memoryAllocation);
		
		if ((k%lineLength) == (0)) {
			printf("%llu\t|", k);
		}
		putc('.', stdout);
		lastChar = ' ';
		if ((k%lineLength) == (lineLength-1)) {
			putc('\n', stdout);
			lastChar = '\n';
		}
		fflush(stdout);
	}
	if (lastChar != '\n') {
		putc('\n', stdout);
	}
	printf("Passed.\n");

	dl_memory_quit(&memoryAllocation);

	error = dl_error_ok;
	l_cleanup:
	
	if (memory) {
		free(memory); memory = NULL;
	}

	return error;
}
