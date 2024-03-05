
#ifndef DUCKLIB_SORT_H
#define DUCKLIB_SORT_H

#include "core.h"

void swap(void *a, void *b, const dl_size_t size);

void heapsort(void *array, dl_size_t length, dl_size_t size,
              int (*comparison)(const void *l, const void *r,
                                const void *context),
              const void *context);

void quicksort_lomuto(void *array, const dl_size_t length, const dl_size_t size,
                      const dl_ptrdiff_t low, const dl_ptrdiff_t high,
                      int (*comparison)(const void *l, const void *r,
                                        const void *context),
                      const void *context);

void quicksort_hoare(void *array, const dl_size_t length, const dl_size_t size,
                     const dl_ptrdiff_t low, const dl_ptrdiff_t high,
                     int (*comparison)(const void *l, const void *r,
                                       const void *context),
                     const void *context);


#endif /* DUCKLIB_SORT_H */
