/*
MIT License

Copyright (c) 2021, 2022, 2023 Joseph Herguth

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "gc.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include <stdlib.h>


dl_error_t duckVM_error_pushRuntime(duckVM_t *duckVM, const dl_uint8_t *message, const dl_size_t message_length) {
	dl_error_t e = dl_error_ok;

	if (duckVM->errors.elements_length > 0) {
		e = dl_array_pushElements(&duckVM->errors, DL_STR("\n"));
		if (e) goto cleanup;
	}
	e = dl_array_pushElements(&duckVM->errors, message, message_length);
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_gclist_init(duckVM_gclist_t *gclist,
                              duckVM_t *duckVM,
                              const dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	gclist->duckVM = duckVM;

	gclist->objects = malloc(maxObjects * sizeof(duckVM_object_t));
	if (gclist->objects == NULL) {
		e = dl_error_outOfMemory;
		goto cleanup;
	}
	gclist->objects_length = maxObjects;

	gclist->freeObjects = malloc(maxObjects * sizeof(duckVM_object_t *));
	if (gclist->freeObjects == NULL) {
		e = dl_error_outOfMemory;
		goto cleanup;
	}
	gclist->freeObjects_length = maxObjects;


	gclist->objectInUse = malloc(maxObjects * sizeof(dl_bool_t));
	if (gclist->objectInUse == NULL) {
		e = dl_error_outOfMemory;
		goto cleanup;
	}


	(void) dl_memclear(gclist->objects, gclist->objects_length * sizeof(duckVM_object_t));


	for (dl_ptrdiff_t i = 0; (dl_size_t) i < maxObjects; i++) {
		gclist->freeObjects[i] = &gclist->objects[i];
	}

 cleanup:

	return e;
}

static dl_error_t duckVM_gclist_quit(duckVM_gclist_t *gclist) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	(void) free(gclist->freeObjects);
	gclist->freeObjects_length = 0;


	(void) free(gclist->objects);
	e = eError ? eError : e;
	gclist->objects_length = 0;


	(void) free(gclist->objectInUse);
	e = eError ? eError : e;

	return e;
}

static dl_error_t duckVM_gclist_markObject(duckVM_gclist_t *gclist, duckVM_object_t *object, dl_bool_t stack) {
	dl_error_t e = dl_error_ok;

	/* Array of pointers that need to be traced. */
	dl_array_t dispatchStack;
	(void) dl_array_init(&dispatchStack, sizeof(duckVM_object_t *), dl_array_strategy_double);

	while (dl_true) {
		// Bug: It is possible for object - gclist->objects to be negative during an OOM condition.
		if (object && (stack
		               || !gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)])) {
			if (!stack) {
				gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)] = dl_true;
			}
			if (object->type == duckVM_object_type_list) {
				e = dl_array_pushElement(&dispatchStack, &object->value.list);
				if (e) goto cleanup;
			}
			else if (object->type == duckVM_object_type_cons) {
				e = dl_array_pushElement(&dispatchStack, &object->value.cons.car);
				if (e) goto cleanup;
				e = dl_array_pushElement(&dispatchStack, &object->value.cons.cdr);
				if (e) goto cleanup;
			}
			else if (object->type == duckVM_object_type_user) {
				if (object->value.user.marker) {
					/* User-provided marking function */
					e = object->value.user.marker(gclist, &dispatchStack, object);
					if (e) goto cleanup;
				}
			}
			/* else ignore, since the stack is the root of GC. Would cause a cycle (infinite loop) if we handled it. */
		}

		e = dl_array_popElement(&dispatchStack, &object);
		if (e == dl_error_bufferUnderflow) {
			e = dl_error_ok;
			break;
		}
		if (e) goto cleanup;

		stack = dl_false;
	}

 cleanup:
	(void) dl_array_quit(&dispatchStack);
	return e;
}

static dl_error_t duckVM_gclist_garbageCollect(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	/* Clear the in use flags. */
	duckVM_gclist_t *gclistPointer = &duckVM->gclist;
	duckVM_gclist_t gclistCopy = duckVM->gclist;

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < gclistCopy.objects_length; i++) {
		gclistCopy.objectInUse[i] = dl_false;
	}

	/* Mark the cells in use. */

	/* Stack */
	DL_DOTIMES(i, duckVM->stack.elements_length) {
		e = duckVM_gclist_markObject(gclistPointer,
		                             &DL_ARRAY_GETADDRESS(duckVM->stack, duckVM_object_t, i),
		                             dl_true);
		if (e) goto cleanup;
	}

	/* Globals */
	DL_DOTIMES(i, duckVM->globals.elements_length) {
		duckVM_object_t *object = DL_ARRAY_GETADDRESS(duckVM->globals, duckVM_object_t *, i);
		if (object != dl_null) {
			e = duckVM_gclist_markObject(gclistPointer, object, dl_false);
			if (e) goto cleanup;
		}
	}

	/* Free cells if not marked. */
	gclistPointer->freeObjects_length = 0;  /* This feels horribly inefficient. (That's 'cause it is.) */
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < gclistCopy.objects_length; i++) {
		if (!gclistCopy.objectInUse[i]) {
			duckVM_object_t *objectPointer = &gclistCopy.objects[i];
			gclistCopy.freeObjects[gclistPointer->freeObjects_length++] = objectPointer;
			duckVM_object_t object = *objectPointer;
			duckVM_object_type_t type = object.type;
			if ((type == duckVM_object_type_symbol)
			         /* Prevent multiple frees. */
			         && (object.value.symbol.name != dl_null)) {
				(void) free(objectPointer->value.symbol.name);
			}
			else if ((type == duckVM_object_type_user)
			         && (object.value.user.destructor != dl_null)) {
				e = object.value.user.destructor(gclistPointer, objectPointer);
				if (e) goto cleanup;
				objectPointer->value.user.destructor = dl_null;
			}
		}
	}

 cleanup:
	return e;
}

dl_error_t duckVM_gclist_pushObject(duckVM_t *duckVM, duckVM_object_t **objectOut, duckVM_object_t objectIn) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_gclist_t *gclist = &duckVM->gclist;

	// Try once
	if (gclist->freeObjects_length == 0) {
		// STOP THE WORLD
		e = duckVM_gclist_garbageCollect(duckVM);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_gclist_pushObject: Garbage collection failed."));
			if (!e) e = eError;
			goto cleanup;
		}

		// Try twice
		if (gclist->freeObjects_length == 0) {
			e = dl_error_outOfMemory;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_gclist_pushObject: Garbage collection failed. Out of memory."));
			if (!e) e = eError;
			goto cleanup;
		}
	}

	duckVM_object_t *heapObject = gclist->freeObjects[--gclist->freeObjects_length];
	*heapObject = objectIn;
	if (objectIn.type == duckVM_object_type_symbol) {
		if (objectIn.value.symbol.name_length > 0) {
			heapObject->value.symbol.name = malloc(objectIn.value.symbol.name_length * sizeof(dl_uint8_t));
			if (heapObject->value.symbol.name == NULL) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_gclist_pushObject: String allocation failed."));
				if (!e) e = eError;
				goto cleanup;
			}
			(void) dl_memcopy_noOverlap(heapObject->value.symbol.name,
			                            objectIn.value.symbol.name,
			                            objectIn.value.symbol.name_length);
		}
		else {
			heapObject->value.symbol.name = dl_null;
			heapObject->value.symbol.name_length = 0;
		}
		if (e) goto cleanup;
	}
	*objectOut = heapObject;

 cleanup:

	return e;
}


dl_error_t duckVM_init(duckVM_t *duckVM, dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	duckVM->nextUserType = duckVM_object_type_last;
	(void) dl_array_init(&duckVM->errors, sizeof(dl_uint8_t), dl_array_strategy_double);
	(void) dl_array_init(&duckVM->stack, sizeof(duckVM_object_t), dl_array_strategy_double);
	(void) dl_array_init(&duckVM->globals,
	                     sizeof(duckVM_object_t *),
	                     dl_array_strategy_double);
	(void) dl_array_init(&duckVM->globals_map,
	                     sizeof(dl_ptrdiff_t),
	                     dl_array_strategy_double);
	e = duckVM_gclist_init(&duckVM->gclist, duckVM, maxObjects);
	if (e) goto cleanup;
	duckVM->userData = dl_null;

 cleanup:
	return e;
}

void duckVM_quit(duckVM_t *duckVM) {
	dl_error_t e;
	(void) dl_array_quit(&duckVM->stack);
	(void) dl_array_quit(&duckVM->globals);
	(void) dl_array_quit(&duckVM->globals_map);
	e = duckVM_gclist_garbageCollect(duckVM);
	(void) duckVM_gclist_quit(&duckVM->gclist);
	(void) dl_array_quit(&duckVM->errors);
	duckVM->userData = dl_null;
	(void) e;
}


/************************************************/
/* Pushing and popping values on/off the stack. */
/************************************************/

dl_error_t stack_push(duckVM_t *duckVM, duckVM_object_t *object) {
	dl_error_t e = dl_error_ok;
	e = dl_array_pushElement(&duckVM->stack, object);
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("stack_push: Failed."));
		if (!e) e = eError;
	}
	return e;
}

dl_error_t stack_pop(duckVM_t *duckVM, duckVM_object_t *object) {
	dl_error_t e = dl_error_ok;
	e = dl_array_popElement(&duckVM->stack, object);
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("stack_pop: Failed."));
		if (!e) e = eError;
	}
	return e;
}

dl_error_t stack_get(duckVM_t *duckVM, duckVM_object_t *element, dl_ptrdiff_t index) {
	if (index < 0) {
		index = duckVM->stack.elements_length + index;
	}
	return dl_array_get(&duckVM->stack, element, index);
}

dl_error_t stack_set(duckVM_t *duckVM, duckVM_object_t *element, dl_ptrdiff_t index) {
	if (index < 0) {
		index = duckVM->stack.elements_length + index;
	}
	return dl_array_set(&duckVM->stack, element, index);
}


/***************************************/
/* Setting and fetching global values. */
/***************************************/

dl_error_t duckVM_global_get(const duckVM_t *duckVM, duckVM_object_t **global, const dl_ptrdiff_t key) {
	DL_DOTIMES(index, duckVM->globals_map.elements_length) {
		dl_ptrdiff_t currentKey = DL_ARRAY_GETADDRESS(duckVM->globals_map, dl_ptrdiff_t, index);
		if (key == currentKey) {
			*global = DL_ARRAY_GETADDRESS(duckVM->globals, duckVM_object_t *, index);
			return dl_error_ok;
		}
	}
	return dl_error_invalidValue;
}

dl_error_t duckVM_global_set(duckVM_t *duckVM, duckVM_object_t *value, dl_ptrdiff_t key) {
	dl_error_t e = dl_error_ok;

	dl_array_t *globals = &duckVM->globals;
	dl_array_t *globals_map = &duckVM->globals_map;
	DL_DOTIMES(index, globals_map->elements_length) {
		dl_ptrdiff_t currentKey = DL_ARRAY_GETADDRESS(*globals_map, dl_ptrdiff_t, index);
		if (key == currentKey) {
			DL_ARRAY_GETADDRESS(*globals, duckVM_object_t *, index) = value;
			goto cleanup;
		}
	}
	e = dl_array_pushElement(globals, &value);
	if (e) goto cleanup;
	e = dl_array_pushElement(globals_map, &key);
	if (e) goto cleanup;

 cleanup: return e;
}
