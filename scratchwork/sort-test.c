
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include "../DuckLib/core.h"

#define ITERATIONS 1000
#define MASK       0xFFFFFF

#define SOURCE_ARRAY_LENGTH 100
#define DESTINATION_ARRAY_LENGTH (SOURCE_ARRAY_LENGTH + SOURCE_ARRAY_LENGTH)




// Helper function for qsort.
int qsort_less(const void *l, const void *r) {
	return *(int *) l - *(int *) r;
}

// Helper function for custom sorts.
int less(const void *l, const void *r, const void *context) {
	return *(int *) l - *(int *) r;
}

/*
  HANABI IS POWERFUL ENOUGH
  def less int (const pointer void l  const pointer void r  pointer void context) {
    - (object-at cast (pointer int) l) (object-at cast (pointer int) r)
  }
 */

void swap(void *a, void *b, const size_t size) {
	// Make `size` static if your version of C is causing you problems.
	unsigned char buf[size];
	/**/ dl_memcopy_noOverlap(buf, a, sizeof(buf));
	/**/ dl_memcopy_noOverlap(a, b, sizeof(buf));
	/**/ dl_memcopy_noOverlap(b, buf, sizeof(buf));
}



void max_heapify(void *array, const size_t length, size_t size, const dl_ptrdiff_t i, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	dl_ptrdiff_t left = 2 * i;
	dl_ptrdiff_t right = left + 1;
	dl_ptrdiff_t largest = i;

	if ((left  < length) && comparison((char *) array + left  * size, (char *) array + largest * size, context) > 0)
		largest = left;

	if ((right < length) && comparison((char *) array + right * size, (char *) array + largest * size, context) > 0)
		largest = right;

	if (largest != i) {
		/**/ swap((char *) array + i * size, (char *) array + largest * size, size);
		/**/ max_heapify(array, length, size, largest, comparison, context);
	}
}

void heapify(void *array, size_t length, size_t size, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	for (dl_ptrdiff_t i = (length - 1)/2; i >= 0; --i) {
		/**/ max_heapify(array, length, size, i, comparison, context);
	}
}

void heapsort(void *array, size_t length, size_t size, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	unsigned char buf[size];
	
	/**/ heapify(array, length, size, comparison, context);

	for (dl_ptrdiff_t end = length - 1; end > 0; --end) {
		/* /\**\/ dl_memcopy_noOverlap(buf, array, size); */
		/* /\**\/ void_heapify(array, end, size, comparison, context); */
		/* /\**\/ dl_memcopy_noOverlap((char *) array + end * size, buf, size); */
		/**/ swap(array, (char *) array + end * size, size);
		/**/ heapify(array, end, size, comparison, context);
	}
}


// heapsortmerge
void void_max_heapify(void *array, const size_t length, size_t size, const dl_ptrdiff_t i, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	dl_ptrdiff_t left = 2 * i;
	dl_ptrdiff_t right = left + 1;
	dl_ptrdiff_t largest = i;

	if ((left  < length) && comparison((char *) array + left  * size, (char *) array + largest * size, context) > 0)
		largest = left;

	if ((right < length) && comparison((char *) array + right * size, (char *) array + largest * size, context) > 0)
		largest = right;

	if (largest != i) {
		/**/ dl_memcopy_noOverlap((char *) array + i * size, (char *) array + largest * size, size);
		/**/ max_heapify(array, length, size, largest, comparison, context);
	}
}

void void_heapify(void *array, size_t length, size_t size, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	for (dl_ptrdiff_t i = (length - 1)/2; i >= 0; --i) {
		/**/ void_max_heapify(array, length, size, i, comparison, context);
	}
}


// quicksortlomuto
dl_ptrdiff_t partition_lomuto(void *array, const dl_size_t length, const dl_size_t size, const dl_ptrdiff_t low, const dl_ptrdiff_t high, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	char pivot[size];
	dl_ptrdiff_t index;

	/**/ dl_memcopy_noOverlap(pivot, (char *) array + high * size, size);
	
	index = low - 1;
	
	for (dl_ptrdiff_t i = low; i < high; i++) {
		if (comparison((char *) array + i * size, pivot, context) <= 0) {
			index++;
			swap((char *) array + index * size, (char *) array + i * size, size);
		}
	}
	index++;
	swap((char *) array + index * size, (char *) array + high * size, size);
	return index;
}

void quicksort_lomuto(void *array, const dl_size_t length, const dl_size_t size, const dl_ptrdiff_t low, const dl_ptrdiff_t high, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	dl_ptrdiff_t pivot;
	
	if ((low >= high) || (low < 0)) return;

	pivot = partition_lomuto(array, length, size, low, high, comparison, context);

	quicksort_lomuto(array, length, size, low, pivot - 1, comparison, context);
	quicksort_lomuto(array, length, size, pivot + 1, high, comparison, context);
}


// quicksorthoare
dl_ptrdiff_t partition_hoare(void *array, const dl_size_t length, const dl_size_t size, const dl_ptrdiff_t low, const dl_ptrdiff_t high, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	char pivot[size];
	dl_ptrdiff_t left_index;
	dl_ptrdiff_t right_index;

	/**/ dl_memcopy_noOverlap(pivot, (char *) array + (high + low)/2 * size, size);
	
	left_index = low - 1;
	right_index = high + 1;
	
	while (dl_true) {
		do left_index++; while (comparison((char *) array + left_index * size, pivot, context) < 0);
		do --right_index; while (comparison((char *) array + right_index * size, pivot, context) > 0);
		
		if (left_index >= right_index) return right_index;
		
		swap((char *) array + left_index * size, (char *) array + right_index * size, size);
	}
}

void quicksort_hoare(void *array, const dl_size_t length, const dl_size_t size, const dl_ptrdiff_t low, const dl_ptrdiff_t high, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	dl_ptrdiff_t pivot;
	
	if ((low >= 0) && (high >= 0) && (low < high)) {
		
		pivot = partition_hoare(array, length, size, low, high, comparison, context);
		
		quicksort_hoare(array, length, size, low, pivot, comparison, context);
		quicksort_hoare(array, length, size, pivot + 1, high, comparison, context);
	}
}





int main(int argc, char *argv[]) {
	int e = 0;

	unsigned int seed = time(NULL);

	clock_t program_start;
	clock_t program_end;
	long double heapsort_accumulated_time = 0;
	long double quicksortlomuto_accumulated_time = 0;
	long double quicksorthoare_accumulated_time = 0;
	clock_t hashsort_start;
	clock_t hashsort_end;

	/* * * * * **
	 * Heapsort *
	 * * * * * **/

	program_start = clock();
	/**/ srand(seed);

	for (ptrdiff_t i = 0; i < ITERATIONS; i++){
		clock_t clock_start;
		clock_t clock_end;
		int a[SOURCE_ARRAY_LENGTH];
		int b[SOURCE_ARRAY_LENGTH];
		int c[DESTINATION_ARRAY_LENGTH] = {0};  // Initialize to zero because we may need to view it partially filled.
		
		// Fill arrays
		for (ptrdiff_t j = 0; j < SOURCE_ARRAY_LENGTH; j++) {
			a[j] = rand() & MASK;
			b[j] = rand() & MASK;
		}

		// Sort a.
		/**/ qsort(a, SOURCE_ARRAY_LENGTH, sizeof(int), qsort_less);

		if (ITERATIONS == 1) {
			for (ptrdiff_t j = 0; j < SOURCE_ARRAY_LENGTH; j++) {
				printf("%x ", a[j]);
			}
			printf("\n\n");
			
			for (ptrdiff_t j = 0; j < SOURCE_ARRAY_LENGTH; j++) {
				printf("%x ", b[j]);
			}
			printf("\n\n");
		}

		clock_start = clock();
		
		/**/ dl_memcopy_noOverlap(c, a, SOURCE_ARRAY_LENGTH * sizeof(int));
		/**/ dl_memcopy_noOverlap(c + SOURCE_ARRAY_LENGTH, b, SOURCE_ARRAY_LENGTH * sizeof(int));

		if (ITERATIONS == 1) {
			for (ptrdiff_t j = 0; j < DESTINATION_ARRAY_LENGTH; j++) {
				printf("%x ", c[j]);
			}
			printf("\n\n");
		}

		heapsort(c, DESTINATION_ARRAY_LENGTH, sizeof(int), less, NULL);

		if (ITERATIONS == 1) {
			printf("\n\n");
			for (ptrdiff_t j = 0; j < DESTINATION_ARRAY_LENGTH; j++) {
				printf("%x ", c[j]);
			}
			printf("\n\n");
		}

		clock_end = clock();

		heapsort_accumulated_time += clock_end - clock_start;
	}

	/* Print results. */

	program_end = clock();

	printf("Heapsort time: %Lf sec.\n", heapsort_accumulated_time / CLOCKS_PER_SEC);
	printf("Run time: %Lf sec. (%.1Lf%% overhead)\n", (long double) (program_end - program_start) / CLOCKS_PER_SEC,  100.0 - 100.0 * heapsort_accumulated_time / (long double) (program_end - program_start));

	/* * * * * * *
	 * Quicksort *
	 * * * * * * */
	
	program_start = clock();
	/**/ srand(seed);

	for (ptrdiff_t i = 0; i < ITERATIONS; i++){
		clock_t clock_start;
		clock_t clock_end;
		int a[SOURCE_ARRAY_LENGTH];
		int b[SOURCE_ARRAY_LENGTH];
		int c[DESTINATION_ARRAY_LENGTH] = {0};  // Initialize to zero because we may need to view it partially filled.
		
		// Fill arrays
		for (ptrdiff_t j = 0; j < SOURCE_ARRAY_LENGTH; j++) {
			a[j] = rand() & MASK;
			b[j] = rand() & MASK;
		}

		// Sort a.
		/**/ qsort(a, SOURCE_ARRAY_LENGTH, sizeof(int), qsort_less);

		clock_start = clock();
		
		/**/ dl_memcopy_noOverlap(c, a, SOURCE_ARRAY_LENGTH * sizeof(int));
		/**/ dl_memcopy_noOverlap(c + SOURCE_ARRAY_LENGTH, b, SOURCE_ARRAY_LENGTH * sizeof(int));

		quicksort_lomuto(c, DESTINATION_ARRAY_LENGTH, sizeof(int), 0, DESTINATION_ARRAY_LENGTH - 1, less, NULL);

		if (ITERATIONS == 1) {
			printf("\n\n");
			for (ptrdiff_t j = 0; j < DESTINATION_ARRAY_LENGTH; j++) {
				printf("%x ", c[j]);
			}
			printf("\n\n");
		}

		clock_end = clock();

		quicksortlomuto_accumulated_time += clock_end - clock_start;
	}

	/* Print results. */

	program_end = clock();

	printf("\nQuicksort (Lomuto) time: %Lf sec.\n", quicksortlomuto_accumulated_time / CLOCKS_PER_SEC);
	printf("Run time: %Lf sec. (%.1Lf%% overhead)\n", (long double) (program_end - program_start) / CLOCKS_PER_SEC,  100.0 - 100.0 * quicksortlomuto_accumulated_time / (long double) (program_end - program_start));

	program_start = clock();
	/**/ srand(seed);

	for (ptrdiff_t i = 0; i < ITERATIONS; i++){
		clock_t clock_start;
		clock_t clock_end;
		int a[SOURCE_ARRAY_LENGTH];
		int b[SOURCE_ARRAY_LENGTH];
		int c[DESTINATION_ARRAY_LENGTH] = {0};  // Initialize to zero because we may need to view it partially filled.
		
		// Fill arrays
		for (ptrdiff_t j = 0; j < SOURCE_ARRAY_LENGTH; j++) {
			a[j] = rand() & MASK;
			b[j] = rand() & MASK;
		}

		// Sort a.
		/**/ qsort(a, SOURCE_ARRAY_LENGTH, sizeof(int), qsort_less);

		clock_start = clock();
		
		/**/ dl_memcopy_noOverlap(c, a, SOURCE_ARRAY_LENGTH * sizeof(int));
		/**/ dl_memcopy_noOverlap(c + SOURCE_ARRAY_LENGTH, b, SOURCE_ARRAY_LENGTH * sizeof(int));

		quicksort_hoare(c, DESTINATION_ARRAY_LENGTH, sizeof(int), 0, DESTINATION_ARRAY_LENGTH - 1, less, NULL);

		if (ITERATIONS == 1) {
			printf("\n\n");
			for (ptrdiff_t j = 0; j < DESTINATION_ARRAY_LENGTH; j++) {
				printf("%x ", c[j]);
			}
			printf("\n\n");
		}

		clock_end = clock();

		quicksorthoare_accumulated_time += clock_end - clock_start;
	}

	/* Print results. */

	program_end = clock();

	printf("\nQuicksort (Hoare) time: %Lf sec.\n", quicksorthoare_accumulated_time / CLOCKS_PER_SEC);
	printf("Run time: %Lf sec. (%.1Lf%% overhead)\n", (long double) (program_end - program_start) / CLOCKS_PER_SEC,  100.0 - 100.0 * quicksorthoare_accumulated_time / (long double) (program_end - program_start));

 cleanup:
	
	return e;
}
