#ifndef DUCKLIB_ARRAY_H
#define DUCKLIB_ARRAY_H

#include "core.h"
#include "memory.h"

typedef enum {
	dl_array_strategy_fit,
	dl_array_strategy_double
} dl_array_strategy_t;

typedef struct {
	void *elements;
	dl_size_t element_size;
	dl_size_t elements_length;
	dl_size_t elements_memorySize;
	dl_array_strategy_t strategy;
} dl_array_t;

void DECLSPEC dl_array_init(dl_array_t *array, dl_size_t element_size, dl_array_strategy_t strategy);
void DECLSPEC dl_array_quit(dl_array_t *array);
dl_error_t DECLSPEC dl_array_pushElement(dl_array_t *array, void *element);
dl_error_t DECLSPEC dl_array_pushElements(dl_array_t *array, const void *elements, dl_size_t elements_length);
dl_error_t DECLSPEC dl_array_popElement(dl_array_t *array, void *element);
dl_error_t DECLSPEC dl_array_popElements(dl_array_t *array, void *elements, dl_size_t count);
dl_error_t DECLSPEC dl_array_getTop(dl_array_t *array, void *element);
dl_error_t DECLSPEC dl_array_setTop(dl_array_t *array, void *element);
dl_error_t DECLSPEC dl_array_get(dl_array_t *array, void *element, dl_ptrdiff_t index);
dl_error_t DECLSPEC dl_array_set(dl_array_t *array, const void *element, dl_ptrdiff_t index);
void DECLSPEC dl_array_clear(dl_array_t *array);

#define DL_ARRAY_GETTOPADDRESS(array, type) ((type*) (array).elements)[(array).elements_length - 1]
#define DL_ARRAY_GETADDRESS(array, type, index) ((type*) (array).elements)[index]

#define DL_ARRAY_FOREACH(element, array, onerror, body) \
for (dl_ptrdiff_t dl_array_i = 0; (dl_size_t) dl_array_i < array.elements_length; dl_array_i++) { \
	dl_error_t dl_array_e = dl_array_get(&array, &element, dl_array_i); \
	if (dl_array_e) \
		onerror \
	body \
}

#endif // DUCKLIB_ARRAY_H
