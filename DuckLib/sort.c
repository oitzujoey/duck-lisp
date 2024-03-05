
#include "sort.h"

void swap(void *a, void *b, const dl_size_t size) {
	// Make `size` static if your version of C is causing you problems.
	unsigned char buf[size];
	/**/ dl_memcopy_noOverlap(buf, a, sizeof(buf));
	/**/ dl_memcopy_noOverlap(a, b, sizeof(buf));
	/**/ dl_memcopy_noOverlap(b, buf, sizeof(buf));
}



void max_heapify(void *array, const dl_size_t length, dl_size_t size, const dl_ptrdiff_t i, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	dl_ptrdiff_t left = 2 * i;
	dl_ptrdiff_t right = left + 1;
	dl_ptrdiff_t largest = i;

	if (((dl_size_t) left  < length) && comparison((char *) array + left  * size, (char *) array + largest * size, context) > 0)
		largest = left;

	if (((dl_size_t) right < length) && comparison((char *) array + right * size, (char *) array + largest * size, context) > 0)
		largest = right;

	if (largest != i) {
		/**/ swap((char *) array + i * size, (char *) array + largest * size, size);
		/**/ max_heapify(array, length, size, largest, comparison, context);
	}
}

void heapify(void *array, dl_size_t length, dl_size_t size, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
	for (dl_ptrdiff_t i = (length - 1)/2; i >= 0; --i) {
		/**/ max_heapify(array, length, size, i, comparison, context);
	}
}

void heapsort(void *array, dl_size_t length, dl_size_t size, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {

	/**/ heapify(array, length, size, comparison, context);

	for (dl_ptrdiff_t end = length - 1; end > 0; --end) {
		/* /\**\/ dl_memcopy_noOverlap(buf, array, size); */
		/* /\**\/ void_heapify(array, end, size, comparison, context); */
		/* /\**\/ dl_memcopy_noOverlap((char *) array + end * size, buf, size); */
		/**/ swap(array, (char *) array + end * size, size);
		/**/ heapify(array, end, size, comparison, context);
	}
}



// quicksortlomuto
dl_ptrdiff_t partition_lomuto(void *array, const dl_size_t size, const dl_ptrdiff_t low, const dl_ptrdiff_t high, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
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

	pivot = partition_lomuto(array, size, low, high, comparison, context);

	quicksort_lomuto(array, length, size, low, pivot - 1, comparison, context);
	quicksort_lomuto(array, length, size, pivot + 1, high, comparison, context);
}



// quicksorthoare
dl_ptrdiff_t partition_hoare(void *array, const dl_size_t size, const dl_ptrdiff_t low, const dl_ptrdiff_t high, int (*comparison)(const void *l, const void *r, const void *context), const void *context) {
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
		
		pivot = partition_hoare(array, size, low, high, comparison, context);
		
		quicksort_hoare(array, length, size, low, pivot, comparison, context);
		quicksort_hoare(array, length, size, pivot + 1, high, comparison, context);
	}
}
