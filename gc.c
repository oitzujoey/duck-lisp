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

/*******************/
/* Error reporting */
/*******************/

// `message` is written to the end of the `errors` string.
dl_error_t duckVM_error_pushRuntime(duckVM_t *duckVM, const dl_uint8_t *message, const dl_size_t message_length) {
	dl_error_t e = dl_error_ok;
	e = dl_array_pushElements(&duckVM->errors, DL_STR("\n"));
	if (e) goto cleanup;
	e = dl_array_pushElements(&duckVM->errors, message, message_length);
	if (e) goto cleanup;
 cleanup: return e;
}


/**********************/
/* Garbage collection */
/**********************/

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

static void duckVM_gclist_quit(duckVM_gclist_t *gclist) {
	(void) free(gclist->freeObjects);
	gclist->freeObjects_length = 0;
	(void) free(gclist->objects);
	gclist->objects_length = 0;
	(void) free(gclist->objectInUse);
}

// Trace objects, marking them as "in use" when we reach them.
static dl_error_t duckVM_gclist_markObject(duckVM_gclist_t *gclist, duckVM_object_t *object, dl_bool_t stack) {
	dl_error_t e = dl_error_ok;

	/* Array of pointers that need to be traced. This is to simulate the recursive algorithm we used before. */
	dl_array_t dispatchStack;
	(void) dl_array_init(&dispatchStack, sizeof(duckVM_object_t *), dl_array_strategy_double);

	while (dl_true) {
		// Bug: It is possible for object - gclist->objects to be negative during an OOM condition.
		/* The object must not be NULL.
		   The object must be on the stack or not have already been marked as in-use. This is to avoid an infinitely
		   traversing a cycle of conses. */
		if (object && (stack || !gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)])) {
			if (!stack) {
				/* The object should not be on the stack because the garbage collector doesn't manage memory that is on
				   the stack, because the stack is an array of objects, not pointers to objects on the heap. */
				// Mark in-use.
				gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)] = dl_true;
			}

			if (object->type == duckVM_object_type_list) {
				// Lists are a pointer to a chain of conses (or nil/NULL).
				e = dl_array_pushElement(&dispatchStack, &object->value.list);
				if (e) goto cleanup;
			}
			else if (object->type == duckVM_object_type_cons) {
				// Trace the CAR.
				e = dl_array_pushElement(&dispatchStack, &object->value.cons.car);
				if (e) goto cleanup;
				// Trace the CDR.
				e = dl_array_pushElement(&dispatchStack, &object->value.cons.cdr);
				if (e) goto cleanup;
			}
			else if (object->type == duckVM_object_type_user) {
				if (object->value.user.marker) {
					/* Call the user-provided marking function */
					e = object->value.user.marker(gclist, &dispatchStack, object);
					if (e) goto cleanup;
				}
			}
			/* Object types that don't point to any other objects are simply ignored. They have 0 pointers we need to
			   trace. So we get integers, floats, and bools for free. */

			/* else ignore, since the stack is the root of GC. Would cause a cycle (infinite loop) if we handled it. */
		}

		// Fetch the next live object to trace.
		e = dl_array_popElement(&dispatchStack, &object);
		if (e == dl_error_bufferUnderflow) {
			e = dl_error_ok;
			break;
		}
		if (e) goto cleanup;

		// We are no longer tracing the stack. If we were, we are now tracing the stack's children.
		stack = dl_false;
	}

 cleanup:
	(void) dl_array_quit(&dispatchStack);
	return e;
}

static dl_error_t duckVM_gclist_garbageCollect(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	duckVM_gclist_t *gclistPointer = &duckVM->gclist;
	duckVM_gclist_t gclistCopy = duckVM->gclist;

	/* Clear the in-use flags. */
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < gclistCopy.objects_length; i++) {
		gclistCopy.objectInUse[i] = dl_false;
	}

	/* Mark the objects in use by chasing their pointers. */

	/* Stack */
	/* Stack objects aren't on the heap, but we start the trace from the stack. This doesn't and can't mark any of the
	   stack objects as free or in use. */
	DL_DOTIMES(i, duckVM->stack.elements_length) {
		e = duckVM_gclist_markObject(gclistPointer,
		                             &DL_ARRAY_GETADDRESS(duckVM->stack, duckVM_object_t, i),
		                             dl_true);
		if (e) goto cleanup;
	}

	/* Globals */
	/* Unlike the stack, globals are stored on the heap. Each global in the list and its children will be marked as
	   in-use. */
	DL_DOTIMES(i, duckVM->globals.elements_length) {
		duckVM_object_t *object = DL_ARRAY_GETADDRESS(duckVM->globals, duckVM_object_t *, i);
		if (object != dl_null) {
			e = duckVM_gclist_markObject(gclistPointer, object, dl_false);
			if (e) goto cleanup;
		}
	}

	/* Free cells if not marked. */
	// Clear the free-array.
	gclistPointer->freeObjects_length = 0;  /* This feels horribly inefficient. (That's 'cause it is.) */
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < gclistCopy.objects_length; i++) {
		/* For each object in the heap array, check if it is in use. If it isn't, put a pointer to that object in the
		   free-array. */
		if (!gclistCopy.objectInUse[i]) {
			duckVM_object_t *objectPointer = &gclistCopy.objects[i];
			gclistCopy.freeObjects[gclistPointer->freeObjects_length++] = objectPointer;
			duckVM_object_t object = *objectPointer;
			duckVM_object_type_t type = object.type;
			/* This is the time to run destructors. If this object points to any memory allocated with malloc, free it
			   now. If it points to any other heap objects, ignore them, since the garbage collector will eventually
			   free those objects too if it hasn't done so already. So if you have a string object, free the character
			   array that the object points to. */

			/* Run the user-provided destructor if this is the user-defined object type. */
			if ((type == duckVM_object_type_user) && (object.value.user.destructor != dl_null)) {
				e = object.value.user.destructor(gclistPointer, objectPointer);
				if (e) goto cleanup;
				objectPointer->value.user.destructor = dl_null;
			}
		}
	}
 cleanup: return e;
}

/* Allocate an object on the heap.
   Accepts `objectIn`, an object, which is then copied into the heap. `*objectOut` is a
   pointer to the new clone that lives in the heap. */
dl_error_t duckVM_gclist_pushObject(duckVM_t *duckVM, duckVM_object_t **objectOut, duckVM_object_t objectIn) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_gclist_t *gclist = &duckVM->gclist;

	// Check if there's space on the heap.
	if (gclist->freeObjects_length == 0) {
		// No space. Garbage collect, since maybe there's some unreachable objects that we could reuse.
		e = duckVM_gclist_garbageCollect(duckVM);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_gclist_pushObject: Garbage collection failed."));
			if (eError) e = eError;
			goto cleanup;
		}

		// Check for space again. There will be space unless the heap is in fact at max capacity.
		if (gclist->freeObjects_length == 0) {
			e = dl_error_outOfMemory;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_gclist_pushObject: Garbage collection failed. Out of memory."));
			if (eError) e = eError;
			goto cleanup;
		}
	}

	// Allocate an object on the heap.
	duckVM_object_t *heapObject = gclist->freeObjects[--gclist->freeObjects_length];
	// The heap object is now allocated.
	// Copy the object to the heap.
	*heapObject = objectIn;

	/* In the real duckVM garbage collector, we would now make copies of some fields so that we don't reference memory
	   that is not managed by the garbage collector. For example, in the case of a symbol or string we would make a copy
	   of the string. It's a bit more complicated than that. */

	// Pass the pointer to the heap object back to the caller.
	*objectOut = heapObject;

 cleanup: return e;
}


/*************************************/
/* Remaining duckVM data structures. */
/*************************************/

dl_error_t duckVM_init(duckVM_t *duckVM, dl_size_t maxObjects) {
	(void) dl_array_init(&duckVM->errors, sizeof(dl_uint8_t), dl_array_strategy_double);
	(void) dl_array_init(&duckVM->stack, sizeof(duckVM_object_t), dl_array_strategy_double);
	(void) dl_array_init(&duckVM->globals,
	                     sizeof(duckVM_object_t *),
	                     dl_array_strategy_double);
	(void) dl_array_init(&duckVM->globals_map,
	                     sizeof(dl_ptrdiff_t),
	                     dl_array_strategy_double);
	return duckVM_gclist_init(&duckVM->gclist, duckVM, maxObjects);
}

void duckVM_quit(duckVM_t *duckVM) {
	dl_error_t e;
	(void) dl_array_quit(&duckVM->stack);
	(void) dl_array_quit(&duckVM->globals);
	(void) dl_array_quit(&duckVM->globals_map);
	e = duckVM_gclist_garbageCollect(duckVM);
	(void) duckVM_gclist_quit(&duckVM->gclist);
	(void) dl_array_quit(&duckVM->errors);
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
