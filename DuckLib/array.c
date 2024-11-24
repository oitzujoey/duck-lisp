
#include "array.h"
#include <stdlib.h>

void dl_array_init(dl_array_t *array, dl_size_t element_size, dl_array_strategy_t strategy) {
	dl_array_t newArray;
	newArray.element_size = element_size;
	newArray.elements_length = 0;
	newArray.elements_memorySize = 0;
	newArray.elements = dl_null;
	newArray.strategy = strategy;
	*array = newArray;
}

void dl_array_quit(dl_array_t *array) {
	if (array->elements != dl_null) {
		(void) free(array->elements);
	}
	array->elements_length = 0;
	array->element_size = 0;
	array->elements_memorySize = 0;
	array->strategy = 0;
}

dl_error_t dl_array_pushElement(dl_array_t *array, void *element) {
	dl_error_t e = dl_error_ok;

	dl_array_t localArray = *array;
	dl_size_t newLengthSize = (localArray.elements_length + 1) * localArray.element_size;

	/* Add space for a new element. */
	if (localArray.strategy == dl_array_strategy_fit) {
		localArray.elements = realloc(localArray.elements, newLengthSize);
		if (localArray.elements == NULL) {
			e = dl_error_outOfMemory;
			goto cleanup;
		}
		localArray.elements_memorySize = newLengthSize;
	}
	else {
		if (newLengthSize > localArray.elements_memorySize) {
			localArray.elements = realloc(localArray.elements, 2 * newLengthSize);
			if (localArray.elements == NULL) {
				e = dl_error_outOfMemory;
				goto cleanup;
			}
			localArray.elements_memorySize = 2 * newLengthSize;
		}
	}

	if (element == dl_null) {
		/* Create an empty element. */
		/**/ dl_memclear(((unsigned char *) localArray.elements
		                  + (dl_ptrdiff_t) (localArray.element_size * localArray.elements_length)),
		                 localArray.element_size);
	} else {
		/* Copy given element into array. */
		(void) dl_memcopy(((unsigned char *) localArray.elements
		                   + (dl_ptrdiff_t) (localArray.element_size * localArray.elements_length)),
		                  element,
		                  localArray.element_size);
	}

	localArray.elements_length++;

 cleanup:
	*array = localArray;
	return e;
}

dl_error_t dl_array_pushElements(dl_array_t *array, const void *elements, dl_size_t elements_length) {
	dl_error_t e = dl_error_ok;
	
	dl_bool_t wasNull = (elements == dl_null);
	
	if (elements_length == 0) goto cleanup;
	
	// Add space for new elements.
	switch (array->strategy) {
	case dl_array_strategy_fit:
		array->elements = realloc(array->elements, (array->elements_length + elements_length) * array->element_size);
		if (array->elements == NULL) {
			e = dl_error_outOfMemory;
			goto cleanup;
		}
		array->elements_memorySize = (array->elements_length + elements_length) * array->element_size;
		break;
	case dl_array_strategy_double:
		while ((array->elements_length + elements_length) * array->element_size > array->elements_memorySize) {
			array->elements = realloc(array->elements, 2 * (array->elements_length + elements_length) * array->element_size);
			if (array->elements == NULL) {
				e = dl_error_outOfMemory;
				goto cleanup;
			}
			array->elements_memorySize = 2 * (array->elements_length + elements_length) * array->element_size;
		}
		break;
	default:
		e = dl_error_shouldntHappen;
		goto cleanup;
	}
	
	if (wasNull) {
		/**/ dl_memclear(&((unsigned char *) array->elements)[array->element_size * array->elements_length],
		                 elements_length * array->element_size);
	}
	else {
		// Copy given element into array.
		(void) dl_memcopy(&((unsigned char *) array->elements)[array->element_size * array->elements_length],
		                  elements,
		                  elements_length * array->element_size);
	}
	
	array->elements_length += elements_length;
	
 cleanup: return e;
}

// dl_error_t dl_array_popElement(dl_array_t *array, void **element, dl_ptrdiff_t index) {
// 	dl_error_t e = dl_error_ok;
	
// 	*element = (unsigned char *) array->elements + index;
	
// 	l_cleanup:
	
// 	return e;
// }

dl_error_t dl_array_popElement(dl_array_t *array, void *element) {
	if (array->elements_length > 0) {
		if (element != dl_null) {
			(void) dl_memcopy(element,
			                  (char*)array->elements + (array->elements_length - 1) * array->element_size,
			                  array->element_size);
		}
		--array->elements_length;
	}
	else {
		return dl_error_bufferUnderflow;
	}
	return dl_error_ok;
}

dl_error_t dl_array_popElements(dl_array_t *array, void *elements, dl_size_t count) {
	dl_error_t e = dl_error_ok;

	if (count > 0) {
		if (array->elements_length >= count) {
			if (elements != dl_null) {
				(void) dl_memcopy(elements, (char*)array->elements + (array->elements_length - count) * array->element_size, count * array->element_size);
			}
			array->elements_length -= count;
		}
		else {
			e = dl_error_bufferUnderflow;
			goto cleanup;
		}
	}
	
 cleanup: return e;
}

/*
Fetches element on the top of the stack.
Returns bufferUnderflow if the stack is empty.
*/
dl_error_t dl_array_getTop(dl_array_t *array, void *element) {
	dl_error_t e = dl_error_ok;
	
	if (array->elements_length > 0) {
		(void) dl_memcopy(element, (char*)array->elements + (array->elements_length - 1) * array->element_size, array->element_size);
	}
	else {
		e = dl_error_bufferUnderflow;
		goto cleanup;
	}
	
 cleanup: return e;
}

/*
Fetches element on the top of the stack.
Returns bufferUnderflow if the stack is empty.
*/
dl_error_t dl_array_setTop(dl_array_t *array, void *element) {
	dl_error_t e = dl_error_ok;
	
	if (array->elements_length > 0) {
		(void) dl_memcopy((char*)array->elements + (array->elements_length - 1) * array->element_size,
		                  element,
		                  array->element_size);
	}
	else {
		e = dl_error_bufferUnderflow;
		goto cleanup;
	}
	
 cleanup: return e;
}

dl_error_t dl_array_get(dl_array_t *array, void *element, dl_ptrdiff_t index) {
	if ((index >= 0) && ((dl_size_t) index < array->elements_length)) {
		(void) dl_memcopy(element, (char*)array->elements + index * array->element_size, array->element_size);
	}
	else {
		return dl_error_invalidValue;
	}
	return dl_error_ok;
}

dl_error_t dl_array_set(dl_array_t *array, const void *element, dl_ptrdiff_t index) {
	if ((index >= 0) && ((dl_size_t) index < array->elements_length)) {
		(void) dl_memcopy( (char*)array->elements + index * array->element_size, element, array->element_size);
	}
	else {
		return dl_error_invalidValue;
	}
	return dl_error_ok;
}

/*
Keeps arrayDestination's allocator the same. After all, it's easy enough to copy the allocator yourself if needed.
*/
dl_error_t dl_array_copy(dl_array_t *arrayDestination, dl_array_t arraySource) {
	dl_error_t e = dl_error_ok;
	
	(void) free(arrayDestination->elements);
	arrayDestination->elements = malloc(arraySource.elements_memorySize);
	if (arrayDestination->elements == NULL) {
		e = dl_error_outOfMemory;
		goto cleanup;
	}
	
	arrayDestination->elements_length = arraySource.elements_length;
	// arrayDestination->memoryAllocation = dl_null;
	arrayDestination->element_size = arraySource.element_size;
	arrayDestination->elements_memorySize = arraySource.elements_memorySize;
	arrayDestination->strategy = arraySource.strategy;
	
	(void) dl_memcopy(arrayDestination->elements,
	                  arraySource.elements,
	                  arraySource.elements_length * arraySource.element_size);
	
	*arrayDestination = arraySource;
	
 cleanup: return e;
}

// Just `array_push` but for arrays.
dl_error_t dl_array_append(dl_array_t *arrayDestination, dl_array_t *arraySource) {
	dl_error_t e = dl_error_ok;

	// Add space for new elements.
	switch (arrayDestination->strategy) {
	case dl_array_strategy_fit:
		arrayDestination->elements = realloc(arrayDestination->elements,
		                                     ((arrayDestination->elements_length + arraySource->elements_length)
		                                      * arrayDestination->element_size));
		if (arrayDestination->elements == NULL) {
			e = dl_error_outOfMemory;
			goto cleanup;
		}
		arrayDestination->elements_memorySize = (arrayDestination->elements_length + arraySource->elements_length) * arrayDestination->element_size;
		break;
	case dl_array_strategy_double:
		while ((arrayDestination->elements_length + arraySource->elements_length) * arrayDestination->element_size > arrayDestination->elements_memorySize) {
			arrayDestination->elements = realloc(arrayDestination->elements,
			                                     (2
			                                      * (arrayDestination->elements_length + arraySource->elements_length)
			                                      * arrayDestination->element_size));
			if (arrayDestination->elements == NULL) {
				e = dl_error_outOfMemory;
				goto cleanup;
			}
			arrayDestination->elements_memorySize = 2 * (arrayDestination->elements_length + arraySource->elements_length) * arrayDestination->element_size;
		}
		break;
	default:
		e = dl_error_shouldntHappen;
		goto cleanup;
	}
	
	// Copy given element into array.
	(void) dl_memcopy(&((unsigned char *) arrayDestination->elements)[arrayDestination->element_size
	                                                                  * arrayDestination->elements_length],
	                  arraySource->elements,
	                  arraySource->elements_length * arrayDestination->element_size);
	
	arrayDestination->elements_length += arraySource->elements_length;

 cleanup: return e;
}

void dl_array_clear(dl_array_t *array) {
	if (array->elements_length == 0) return;	
	array->elements_length = 0;
	array->elements_memorySize = 0;
	(void) free(array->elements);
}
