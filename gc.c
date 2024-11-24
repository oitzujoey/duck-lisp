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
#include "DuckLib/string.h"
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


	/**/ dl_memclear(gclist->objects, gclist->objects_length * sizeof(duckVM_object_t));


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

static dl_error_t duckVM_gclist_pushObject(duckVM_t *duckVM, duckVM_object_t **objectOut, duckVM_object_t objectIn) {
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
			/**/ dl_memcopy_noOverlap(heapObject->value.symbol.name,
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
	/**/ duckVM_gclist_quit(&duckVM->gclist);
	(void) dl_array_quit(&duckVM->errors);
	duckVM->userData = dl_null;
	(void) e;
}

static dl_error_t stack_push(duckVM_t *duckVM, duckVM_object_t *object) {
	dl_error_t e = dl_error_ok;
	e = dl_array_pushElement(&duckVM->stack, object);
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("stack_push: Failed."));
		if (!e) e = eError;
	}
	return e;
}

/* This is only used by the FFI. */
static dl_error_t stack_pop(duckVM_t *duckVM, duckVM_object_t *object) {
	dl_error_t e = dl_error_ok;
	e = dl_array_popElement(&duckVM->stack, object);
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("stack_pop: Failed."));
		if (!e) e = eError;
	}
	return e;
}

static dl_error_t stack_pop_multiple(duckVM_t *duckVM, dl_size_t pops) {
	dl_error_t e = dl_error_ok;
	e = dl_array_popElements(&duckVM->stack, dl_null, pops);
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("stack_pop_multiple: Failed."));
		if (!e) e = eError;
	}
	return e;
}

static dl_error_t stack_getTop(duckVM_t *duckVM, duckVM_object_t *element) {
	return dl_array_getTop(&duckVM->stack, element);
}

static dl_error_t stack_get(duckVM_t *duckVM, duckVM_object_t *element, dl_ptrdiff_t index) {
	if (index < 0) {
		index = duckVM->stack.elements_length + index;
	}
	return dl_array_get(&duckVM->stack, element, index);
}

static dl_error_t stack_set(duckVM_t *duckVM, duckVM_object_t *element, dl_ptrdiff_t index) {
	if (index < 0) {
		index = duckVM->stack.elements_length + index;
	}
	return dl_array_set(&duckVM->stack, element, index);
}

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

 cleanup:
	return e;
}

/* Detect cycles in linked lists using Richard Brent's algorithm.
   source: https://stackoverflow.com/questions/2663115/how-to-detect-a-loop-in-a-linked-list */
dl_bool_t duckVM_listIsCyclic(duckVM_object_t *rootCons) {
	if (rootCons == dl_null) return dl_false;

	duckVM_object_t *slow = rootCons;
	duckVM_object_t *fast = rootCons;
	int taken = 0;
	int limit = 2;
	while ((fast->type == duckVM_object_type_cons) && (fast->value.cons.cdr != dl_null)) {
		fast = fast->value.cons.cdr;
		taken++;
		if (slow == fast) return dl_true;
		if (taken == limit) {
			taken = 0;
			limit *= 2;
			slow = fast;
		}
	}
	return dl_false;
}


///////////////////////////////////////
// Functions for C callbacks to use. //
///////////////////////////////////////

dl_error_t duckVM_garbageCollect(duckVM_t *duckVM) {
	return duckVM_gclist_garbageCollect(duckVM);
}

/* void duckVM_getArgLength(duckVM_t *duckVM, dl_size_t *length) { */
/* 	*length = DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, duckVM->frame_pointer).value.integer; */
/* } */

/* dl_error_t duckVM_getArg(duckVM_t *duckVM, duckLisp_object_t *object, dl_ptrdiff_t index) { */
/* 	if (index < DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, duckVM->frame_pointer).value.integer) { */
/* 		*object = DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, duckVM->frame_pointer + index); */
/* 		return dl_error_ok; */
/* 	} */
/* 	else { */
/* 		return dl_error_bufferOverflow; */
/* 	} */
/* } */

dl_error_t duckVM_object_pop(duckVM_t *duckVM, duckVM_object_t *object) {
	return stack_pop(duckVM, object);
}

dl_error_t duckVM_popAll(duckVM_t *duckVM) {
	return stack_pop_multiple(duckVM, duckVM->stack.elements_length);
}

dl_error_t duckVM_object_push(duckVM_t *duckVM, duckVM_object_t *object) {
	return stack_push(duckVM, object);
}

dl_error_t duckVM_allocateHeapObject(duckVM_t *duckVM, duckVM_object_t **heapObjectOut, duckVM_object_t objectIn) {
	return duckVM_gclist_pushObject(duckVM, heapObjectOut, objectIn);
}

dl_error_t duckVM_softReset(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	e = stack_pop_multiple(duckVM, duckVM->stack.elements_length);
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_softReset: Failed."));
		if (!e) e = eError;
	}
	return e;
}

/* Push the specified global variable onto the stack. */
dl_error_t duckVM_pushGlobal(duckVM_t *duckVM, const dl_ptrdiff_t key) {
	duckVM_object_t *globalPointer = dl_null;
	dl_error_t e = duckVM_global_get(duckVM, &globalPointer, key);
	if (e) return e;
	return stack_push(duckVM, globalPointer);
}

/* Set the specified global variable to the value of the object on top of the stack. */
dl_error_t duckVM_setGlobal(duckVM_t *duckVM, const dl_ptrdiff_t key) {
	duckVM_object_t *object = dl_null;
	dl_error_t e = stack_getTop(duckVM, object);
	if (e) return e;
	return duckVM_global_set(duckVM, object, key);
}


duckVM_object_t duckVM_object_makeBoolean(dl_bool_t boolean) {
	duckVM_object_t o;
	o.type = duckVM_object_type_bool;
	o.value.boolean = boolean;
	return o;
}

duckVM_object_t duckVM_object_makeInteger(dl_ptrdiff_t integer) {
	duckVM_object_t o;
	o.type = duckVM_object_type_integer;
	o.value.integer = integer;
	return o;
}

duckVM_object_t duckVM_object_makeFloat(double floatingPoint) {
	duckVM_object_t o;
	o.type = duckVM_object_type_float;
	o.value.floatingPoint = floatingPoint;
	return o;
}

dl_error_t duckVM_object_makeSymbol(duckVM_object_t *symbolOut,
                                    dl_size_t id,
                                    dl_uint8_t *string,
                                    dl_size_t string_length) {
	dl_error_t e = dl_error_ok;

	dl_uint8_t *name = NULL;
	if (string != NULL) {
		name = malloc(string_length * sizeof(dl_uint8_t));
		if (name == NULL) {
			e = dl_error_outOfMemory;
			goto cleanup;
		}
		(void) dl_memcopy_noOverlap(name, string, string_length);
	}

	duckVM_object_t symbol;
	symbol.type = duckVM_object_type_symbol;
	symbol.value.symbol.name = name;
	symbol.value.symbol.name_length = string_length;
	symbol.value.symbol.id = id;
	*symbolOut = symbol;

 cleanup: return e;
}

duckVM_object_t duckVM_object_makeList(duckVM_object_t *cons) {
	duckVM_object_t o;
	o.type = duckVM_object_type_list;
	o.value.list = cons;
	return o;
}

duckVM_object_t duckVM_object_makeCons(duckVM_object_t *car, duckVM_object_t *cdr) {
	duckVM_object_t o;
	o.type = duckVM_object_type_cons;
	o.value.cons.car = car;
	o.value.cons.cdr = cdr;
	return o;
}

duckVM_object_t duckVM_object_makeUser(void *data,
                                       dl_error_t (*marker)(duckVM_gclist_t *, dl_array_t *, struct duckVM_object_s *),
                                       dl_error_t (*destructor)(duckVM_gclist_t *, struct duckVM_object_s *)) {
	duckVM_object_t o;
	o.type = duckVM_object_type_user;
	o.value.user.data = data;
	o.value.user.marker = marker;
	o.value.user.destructor = destructor;
	return o;
}


dl_bool_t duckVM_object_getBoolean(duckVM_object_t object) {
	return object.value.boolean;
}

dl_ptrdiff_t duckVM_object_getInteger(duckVM_object_t object) {
	return object.value.integer;
}

double duckVM_object_getFloat(duckVM_object_t object) {
	return object.value.floatingPoint;
}

duckVM_list_t duckVM_object_getList(duckVM_object_t object) {
	return object.value.list;
}

duckVM_cons_t duckVM_object_getCons(duckVM_object_t object) {
	return object.value.cons;
}

dl_error_t duckVM_object_getSymbol(dl_size_t *id,
                                   dl_uint8_t **string,
                                   dl_size_t *length,
                                   duckVM_object_t object) {
	dl_error_t e = dl_error_ok;

	{
		dl_uint8_t *name = object.value.symbol.name;
		dl_size_t name_length = object.value.symbol.name_length;
		if (name_length) {
			*string = malloc(name_length * sizeof(dl_uint8_t));
			if (*string == NULL) {
				e = dl_error_outOfMemory;
				goto cleanup;
			}

			(void) dl_memcopy_noOverlap(*string, name, name_length);
		}
		else {
			*string = dl_null;
		}
		*length = name_length;
	}

	*id = object.value.symbol.id;

 cleanup: return e;
}


dl_error_t duckVM_list_getCons(duckVM_list_t list, duckVM_cons_t *cons) {
	duckVM_object_t *consObject = list;
	if (consObject == dl_null) {
		return dl_error_nullPointer;
	}
	*cons = duckVM_object_getCons(*consObject);
	return dl_error_ok;
}


static dl_bool_t templateForIsThing(duckVM_t *duckVM, dl_bool_t *result, duckVM_object_type_t type) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t object;
	e = dl_array_getTop(&duckVM->stack, &object);
	if (e) return e;
	*result = (object.type == type);
	return e;
}


dl_error_t duckVM_isNone(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_none);
}

dl_error_t duckVM_isBoolean(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_bool);
}

dl_error_t duckVM_isInteger(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_integer);
}

dl_error_t duckVM_isFloat(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_float);
}

dl_error_t duckVM_isSymbol(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_symbol);
}

dl_error_t duckVM_isType(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_type);
}

dl_error_t duckVM_isComposite(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_composite);
}

dl_error_t duckVM_isCons(duckVM_t *duckVM, dl_bool_t *result) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t object;
	e = dl_array_getTop(&duckVM->stack, &object);
	if (e) return e;
	*result = ((object.type == duckVM_object_type_list)
	           && (dl_null != object.value.list));
	return e;
}

dl_error_t duckVM_isList(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_list);
}

dl_error_t duckVM_isNil(duckVM_t *duckVM, dl_bool_t *result) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t object;
	e = dl_array_getTop(&duckVM->stack, &object);
	if (e) return e;
	*result = ((object.type == duckVM_object_type_list)
	           && (dl_null == object.value.list));
	return e;
}

dl_error_t duckVM_isUser(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_user);
}

dl_error_t duckVM_isEmpty(duckVM_t *duckVM, dl_bool_t *result) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t object;
	e = dl_array_getTop(&duckVM->stack, &object);
	if (e) return e;
	switch (object.type) {
	case duckVM_object_type_list:
		*result = (dl_null == object.value.list);
		break;
	default:
		*result = dl_false;
	}
	return e;
}



/* General operations */

dl_size_t duckVM_stackLength(duckVM_t *duckVM) {
	return duckVM->stack.elements_length;
}

/* Push an existing stack object onto the top of the stack. */
dl_error_t duckVM_push(duckVM_t *duckVM, dl_ptrdiff_t stack_index) {
	duckVM_object_t object;
	dl_error_t e = stack_get(duckVM, &object, stack_index);
	if (e) return e;
	return stack_push(duckVM, &object);
}

/* Pop an object off of the stack. */
dl_error_t duckVM_pop(duckVM_t *duckVM) {
	return stack_pop(duckVM, dl_null);
}

/* Pop the specified number of objects off of the stack. */
dl_error_t duckVM_popSeveral(duckVM_t *duckVM, dl_size_t number_to_pop) {
	return stack_pop_multiple(duckVM, number_to_pop);
}

/* Copy a stack object from the top to the specified position, overwriting the destination object. */
dl_error_t duckVM_copyFromTop(duckVM_t *duckVM, dl_ptrdiff_t destination_stack_index) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		e = stack_set(duckVM, &object, destination_stack_index);
		if (e) break;
	} while (0);
	return e;
}

/* Return the type of the object on the top of the stack. */
dl_error_t duckVM_typeOf(duckVM_t *duckVM, duckVM_object_type_t *type) {
	dl_error_t e = dl_error_ok;

	duckVM_object_t object;
	e = dl_array_getTop(&duckVM->stack, &object);
	if (e) return e;
	*type = object.type;

	return e;
}


/* Type-specific operations */

/* Booleans */

/* Push a boolean initialized to `false` onto the top of the stack. */
dl_error_t duckVM_pushBoolean(duckVM_t *duckVM) {
	duckVM_object_t object;
	object.type = duckVM_object_type_bool;
	object.value.boolean = dl_false;
	return stack_push(duckVM, &object);
}

/* Set the boolean at the top of the stack to the provided value. */
dl_error_t duckVM_setBoolean(duckVM_t *duckVM, dl_bool_t value) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_bool != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setBoolean: Not a boolean."));
			if (eError) e = eError;
			break;
		}
		object = duckVM_object_makeBoolean(value);
		e = dl_array_setTop(&duckVM->stack, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Copy a boolean off the top of the stack into the provided variable. */
dl_error_t duckVM_copyBoolean(duckVM_t *duckVM, dl_bool_t *value) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_bool != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyBoolean: Not a boolean."));
			if (eError) e = eError;
			break;
		}
		*value = object.value.boolean;
	} while (0);
	return e;
}


/* Integers */

/* Push an integer initialized to `0` onto the top of the stack. */
dl_error_t duckVM_pushInteger(duckVM_t *duckVM) {
	duckVM_object_t object;
	object.type = duckVM_object_type_integer;
	object.value.integer = 0;
	return stack_push(duckVM, &object);
}

/* Set the integer at the top of the stack to the provided value. */
dl_error_t duckVM_setInteger(duckVM_t *duckVM, dl_ptrdiff_t value) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_integer != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setInteger: Not an integer."));
			if (eError) e = eError;
			break;
		}
		object = duckVM_object_makeInteger(value);
		e = dl_array_setTop(&duckVM->stack, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Copy an integer off the top of the stack into the provided variable. */
dl_error_t duckVM_copySignedInteger(duckVM_t *duckVM, dl_ptrdiff_t *value) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_integer != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copySignedInteger: Not an integer."));
			if (eError) e = eError;
			break;
		}
		*value = object.value.integer;
	} while (0);
	return e;
}

/* Copy an integer off the top of the stack into the provided variable. */
dl_error_t duckVM_copyUnsignedInteger(duckVM_t *duckVM, dl_size_t *value) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_integer != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyUnsignedInteger: Not an integer."));
			if (eError) e = eError;
			break;
		}
		*value = object.value.integer;
	} while (0);
	return e;
}

/* Floats */

/* Push a float initialized to `0.0` onto the top of the stack. */
dl_error_t duckVM_pushFloat(duckVM_t *duckVM) {
	duckVM_object_t object;
	object.type = duckVM_object_type_float;
	object.value.floatingPoint = 0.0;
	return stack_push(duckVM, &object);
}

/* Set the float at the top of the stack to the provided value. */
dl_error_t duckVM_setFloat(duckVM_t *duckVM, double value) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_float != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setFloat: Not a float."));
			if (eError) e = eError;
			break;
		}
		object = duckVM_object_makeFloat(value);
		e = dl_array_setTop(&duckVM->stack, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Copy a float off the top of the stack into the provided variable. */
dl_error_t duckVM_copyFloat(duckVM_t *duckVM, double *value) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_float != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyFloat: Not a float."));
			if (eError) e = eError;
			break;
		}
		*value = object.value.floatingPoint;
	} while (0);
	return e;
}


/* Strings */

/* Push a string onto the top of the stack. Strings are immutable, which is why there isn't a `duckVM_setString`. */
dl_error_t duckVM_pushString(duckVM_t *duckVM, dl_uint8_t *string, dl_size_t string_length) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t object;
		e = duckVM_object_makeString(duckVM, &object, string, string_length);
		if (e) break;
		e = duckVM_object_push(duckVM, &object);
		if (e) break;
	} while (0);
	return e;
}


/* Symbols */

/* Push a symbol onto the top of the stack. */
dl_error_t duckVM_pushSymbol(duckVM_t *duckVM, dl_size_t id, dl_uint8_t *name, dl_size_t name_length) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t object;
		e = duckVM_object_makeSymbol(&object, id, name, name_length);
		if (e) break;
		e = duckVM_object_push(duckVM, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Push a symbol with an ID but no name onto the top of the stack. */
dl_error_t duckVM_pushCompressedSymbol(duckVM_t *duckVM, dl_size_t id) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t object;
		(void) duckVM_object_makeCompressedSymbol(&object, id);
		e = duckVM_object_push(duckVM, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Push the name string of the symbol on the top of the stack onto the top of the stack. */
dl_error_t duckVM_copySymbolName(duckVM_t *duckVM, dl_uint8_t **name, dl_size_t *name_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_symbol != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copySymbolName: Not a symbol."));
			if (eError) e = eError;
			break;
		}
		if (object.value.symbol.name != dl_null) {
			dl_uint8_t *symbol_name = object.value.symbol.name;
			dl_size_t symbol_name_length = object.value.symbol.name_length;
			*name = malloc(symbol_name_length * sizeof(dl_uint8_t));
			if (*name == NULL) {
				e = dl_error_outOfMemory;
				break;
			}
			(void) dl_memcopy_noOverlap(*name, symbol_name, symbol_name_length);
			*name_length = symbol_name_length;
		}
		else {
			*name = dl_null;
			name_length = 0;
			/* OK: This is symbol "compression". */
		}
	} while (0);
	return e;
}

/* Push the ID of the symbol on the top of the stack onto the top of the stack. */
dl_error_t duckVM_copySymbolId(duckVM_t *duckVM, dl_size_t *id) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_symbol != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copySymbolId: Not a symbol."));
			if (eError) e = eError;
			break;
		}
		*id = object.value.symbol.id;
	} while (0);
	return e;
}


/* Types */

/* Create a new unique type and push it onto the top of the stack. */
dl_error_t duckVM_pushNewType(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t object;
		object.type = duckVM_object_type_type;
		object.value.type = duckVM->nextUserType++;
		e = stack_push(duckVM, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Push the specified type onto the top of the stack. */
dl_error_t duckVM_pushExistingType(duckVM_t *duckVM, dl_size_t type) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t object;
		object.type = duckVM_object_type_type;
		object.value.type = type;
		e = duckVM_object_push(duckVM, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Copy a type off the top of the stack into the provided variable. */
dl_error_t duckVM_copyType(duckVM_t *duckVM, dl_size_t *type) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_type != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyType: Not a type."));
			if (eError) e = eError;
			break;
		}
		*type = object.value.type;
	} while (0);
	return e;
}


/* Lists -- See sequence operations below that operate on these objects. */

/* Push nil onto the stack. */
dl_error_t duckVM_pushNil(duckVM_t *duckVM) {
	duckVM_object_t object;
	object.type = duckVM_object_type_list;
	object.value.list = dl_null;
	return duckVM_object_push(duckVM, &object);
}

/* Push a cons onto the stack with both CAR and CDR set to nil. */
dl_error_t duckVM_pushCons(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t object;
		duckVM_object_t *heapCons = dl_null;
		duckVM_object_t cons = duckVM_object_makeCons(dl_null, dl_null);
		e = duckVM_allocateHeapObject(duckVM, &heapCons, cons);
		if (e) break;
		object = duckVM_object_makeList(heapCons);
		if (e) break;
		e = duckVM_object_push(duckVM, &object);
		if (e) break;
	} while (0);
	return e;
}


/* Sequences */
/* These are operations for lists, vectors, strings, and closures. Not all sequence types support all operations. */

/* Push the first element of the sequence on the top of the stack onto the top of the stack. */
dl_error_t duckVM_pushCar(duckVM_t *duckVM) {
	return duckVM_pushFirst(duckVM);
}

/* Push the first element of the sequence on the top of the stack onto the top of the stack. */
dl_error_t duckVM_pushFirst(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t first;
		duckVM_object_t sequence;
		duckVM_object_type_t type;
		e = dl_array_getTop(&duckVM->stack, &sequence);
		if (e) break;
		type = sequence.type;
		switch (type) {
		case duckVM_object_type_list: {
			duckVM_list_t list = sequence.value.list;
			if (list) {
				duckVM_object_t *car = list->value.cons.car;
				if (car) {
					if (duckVM_object_type_cons == car->type) {
						first = duckVM_object_makeList(car);
					}
					else {
						first = *car;
					}
				}
				else {
					first = duckVM_object_makeList(dl_null);
				}
			}
			else {
				/* Nil */
				first = duckVM_object_makeList(dl_null);
			}
			break;
		}
		case duckVM_object_type_cons: {
			/* This shouldn't be on the stack, but I guess we can accommodate it anyway. */
			duckVM_cons_t cons = sequence.value.cons;
			first = duckVM_object_makeList(cons.car);
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_pushFirst: Unsupported object type."));
			if (eError) e = eError;
			break;
		}
		}
		e = duckVM_object_push(duckVM, &first);
		if (e) break;
	} while (0);
	return e;
}

/* Get all elements except the first from the sequence on top of the stack and push them on top of the stack.
   Lists:
     Nil: Push nil on top of the stack.
     Cons: Push the CDR on top of the stack.
   Vectors:
     Push a new vector on top of the stack that contains all elements of the old vector except the first. These
     like with the CDR of a list, these remaining elements are references to the elements of the original list.
   Strings: Push a new string on top of the stack that contains all bytes of the old string except the first.
   This operation fails on closures. */
dl_error_t duckVM_pushCdr(duckVM_t *duckVM) {
	return duckVM_pushRest(duckVM);
}

/* Get all elements except the first from the sequence on top of the stack and push them on top of the stack.
   Lists:
     Nil: Push nil on top of the stack.
     Cons: Push the CDR on top of the stack.
   Vectors:
     Push a new vector on top of the stack that contains all elements of the old vector except the first. These
     like with the CDR of a list, these remaining elements are references to the elements of the original list.
   Strings: Push a new string on top of the stack that contains all bytes of the old string except the first.
   This operation fails on closures. */
dl_error_t duckVM_pushRest(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t rest;
		duckVM_object_t sequence;
		e = dl_array_getTop(&duckVM->stack, &sequence);
		if (e) break;
		switch (sequence.type) {
		case duckVM_object_type_list: {
			duckVM_list_t list = sequence.value.list;
			if (list) {
				duckVM_object_t *cdr = list->value.cons.cdr;
				if (cdr) {
					if (duckVM_object_type_cons == cdr->type) {
						rest = duckVM_object_makeList(cdr);
					}
					else {
						rest = *cdr;
					}
				}
				else {
					rest = duckVM_object_makeList(dl_null);
				}
			}
			else {
				/* Nil */
				rest = duckVM_object_makeList(dl_null);
			}
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_pushRest: Unsupported object type."));
			if (eError) e = eError;
			break;
		}
		}
		e = duckVM_object_push(duckVM, &rest);
		if (e) break;
	} while (0);
	return e;
}

/* Set the first element of the sequence at the specified stack index to the value of the object at the top of the
   stack.
   This operation fails on nil, strings, and closures. */
dl_error_t duckVM_setCar(duckVM_t *duckVM, dl_ptrdiff_t stack_index) {
	return duckVM_setFirst(duckVM, stack_index);
}

/* Set the CDR of the list at the specified stack index to the value of the object at the top of the stack.
   Only conses support this operation. */
dl_error_t duckVM_setCdr(duckVM_t *duckVM, dl_ptrdiff_t stack_index) {
	return duckVM_setRest(duckVM, stack_index);
}

/* Set the CDR of the list at the specified stack index to the value of the object at the top of the stack.
   Only conses support this operation. */
dl_error_t duckVM_setRest(duckVM_t *duckVM, dl_ptrdiff_t stack_index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t value;
		duckVM_object_t *value_pointer = dl_null;
		duckVM_object_t sequence;
		duckVM_object_type_t type;

		e = dl_array_getTop(&duckVM->stack, &value);
		if (e) break;
		e = duckVM_allocateHeapObject(duckVM, &value_pointer, value);
		if (e) break;
		e = stack_get(duckVM, &sequence, stack_index);
		if (e) break;
		type = sequence.type;
		switch (type) {
		case duckVM_object_type_list: {
			duckVM_list_t list = sequence.value.list;
			if (list) {
				list->value.cons.cdr = value_pointer;
			}
			else {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_setRest: List is nil."));
				if (eError) e = eError;
				break;
			}
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setRest: Unsupported object type."));
			if (eError) e = eError;
			break;
		}
		}
		if (e) break;
	} while (0);
	return e;
}

/* Get the specified element of the sequence on top of the stack, and push it on top of the stack.
   Lists:Push the indexed element on top of the stack.
   Vectors: Push the indexed element on top of the stack.
   Strings: Push the indexed byte on top of the stack as an integer.
   This operation fails on sequences are shorter than the index. */
dl_error_t duckVM_pushElement(duckVM_t *duckVM, dl_ptrdiff_t sequence_index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t sequence;
		duckVM_object_type_t type;
		duckVM_object_t element;
		e = dl_array_getTop(&duckVM->stack, &sequence);
		if (e) break;
		type = sequence.type;
		switch (type) {
		case duckVM_object_type_list: {
			duckVM_object_t *element_pointer = sequence.value.list;
			DL_DOTIMES(k, sequence_index) {
				if (element_pointer) {
					if (duckVM_object_type_cons == element_pointer->type) {
						element_pointer = element_pointer->value.cons.cdr;
					}
					else {
						e = dl_error_invalidValue;
						eError = duckVM_error_pushRuntime(duckVM,
						                                  DL_STR("duckVM_pushElement: Ran out of elements in improper list."));
						if (eError) e = eError;
						break;
					}
				}
				else {
					break;
				}
			}
			if (e) break;
			element = *element_pointer;
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_pushElement: Unsupported object type."));
			if (eError) e = eError;
			break;
		}
		}
		if (e) break;
		e = stack_push(duckVM, &element);
		if (e) break;
	} while (0);
	return e;
}

/* Set the specified element of the sequence at the specified stack index to the value on top of the stack.
   Lists:
     Cons: Set the CAR to the value on top of the stack if this is the cons referenced by the index.
   Vectors: Set the indexed element to the value on top of the stack.
   This operation fails on lists and vectors that are shorter than the sequence index, and strings and closures. */
dl_error_t duckVM_setElement(duckVM_t *duckVM, dl_ptrdiff_t sequence_index, dl_ptrdiff_t stack_index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t value;
		duckVM_object_t *value_pointer = dl_null;
		duckVM_object_t sequence;
		duckVM_object_type_t type;
		e = dl_array_getTop(&duckVM->stack, &value);
		if (e) break;
		e = duckVM_allocateHeapObject(duckVM, &value_pointer, value);
		if (e) break;
		e = stack_get(duckVM, &sequence, stack_index);
		if (e) break;
		type = sequence.type;
		switch (type) {
		case duckVM_object_type_list: {
			duckVM_list_t list = sequence.value.list;
			if (list) {
				list->value.cons.car = value_pointer;
			}
			else {
				/* Nil */
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_setElement: List is nil."));
				break;
			}
			break;
			/* This looks like a bug: */ }{
			duckVM_object_t *element_pointer = sequence.value.list;
			DL_DOTIMES(k, sequence_index) {
				if (element_pointer) {
					if (duckVM_object_type_cons == element_pointer->type) {
						element_pointer = element_pointer->value.cons.cdr;
					}
					else {
						e = dl_error_invalidValue;
						eError = duckVM_error_pushRuntime(duckVM,
						                                  DL_STR("duckVM_setElement: Ran out of elements in improper list."));
						if (eError) e = eError;
						break;
					}
				}
				else {
					e = dl_error_invalidValue;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_setElement: Ran out of elements in list."));
					if (eError) e = eError;
					break;
				}
			}
			if (e) break;
			if (element_pointer) {
				if (duckVM_object_type_cons == element_pointer->type) {
					element_pointer->value.cons.car = value_pointer;
				}
				else {
					e = dl_error_invalidValue;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_setElement: Ran out of elements in improper list."));
					if (eError) e = eError;
					break;
				}
			}
			else {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_setElement: Ran out of elements in list."));
				if (eError) e = eError;
				break;
			}
			break;
		}
		default: {
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setElement: Unsupported object type."));
			if (eError) e = eError;
			break;
		}
		}
		if (e) break;
	} while (0);
	return e;
}

/* Return the length of the sequence into the passed variable. */
dl_error_t duckVM_length(duckVM_t *duckVM, dl_size_t *length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t sequence;
		duckVM_object_t *sequencePointer = dl_null;
		duckVM_object_type_t type;
		dl_size_t local_length = 0;
		e = dl_array_getTop(&duckVM->stack, &sequence);
		if (e) break;
		type = sequence.type;
		switch (type) {
		case duckVM_object_type_list: {
			if (duckVM_listIsCyclic(sequence.value.list)) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_length: List is circular."));
				if (!e) e = eError;
				break;
			}
			local_length = 0;
			{
				sequencePointer = sequence.value.list;
				while ((sequencePointer != dl_null) && (sequencePointer->type == duckVM_object_type_cons)) {
					local_length++;
					sequencePointer = sequencePointer->value.cons.cdr;
				}
			}
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                             DL_STR("duckVM_pushRest: Unsupported object type."));
			if (eError) e = eError;
			break;
		}
		}
		*length = local_length;
	} while (0);
	return e;
}


/* Pretty printing functions for debugging. */

dl_error_t duckVM_gclist_prettyPrint(dl_array_t *string_array, duckVM_gclist_t gclist) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_gclist_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("objects["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, gclist.objects_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = "));
	if (e) goto cleanup;
	if (gclist.objects == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("{...}"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("freeObjects["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, gclist.freeObjects_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = "));
	if (e) goto cleanup;
	if (gclist.freeObjects == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("{...}"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("objectInUse["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, gclist.objects_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = "));
	if (e) goto cleanup;
	if (gclist.objectInUse == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("{...}"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_prettyPrint(dl_array_t *string_array, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("errors = \""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, duckVM.errors.elements, duckVM.errors.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("stack = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckVM.stack.elements_length) {
		e = duckVM_object_prettyPrint(string_array, DL_ARRAY_GETADDRESS(duckVM.stack, duckVM_object_t, i), duckVM);
		if (e) goto cleanup;
		if ((dl_size_t) i != duckVM.stack.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("globals = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckVM.globals.elements_length) {
		DL_DOTIMES(j, duckVM.globals_map.elements_length) {
			dl_ptrdiff_t target_index = DL_ARRAY_GETADDRESS(duckVM.globals, dl_ptrdiff_t, i);
			if (target_index == i) {
				e = dl_string_fromPtrdiff(string_array, j);
				if (e) goto cleanup;
				e = dl_array_pushElements(string_array, DL_STR(": "));
				if (e) goto cleanup;
			}
		}
		e = duckVM_object_prettyPrint(string_array, DL_ARRAY_GETADDRESS(duckVM.globals, duckVM_object_t, i), duckVM);
		if (e) goto cleanup;
		if ((dl_size_t) i != duckVM.globals.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("gclist = "));
	if (e) goto cleanup;
	e = duckVM_gclist_prettyPrint(string_array, duckVM.gclist);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("nextUserType = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckVM.nextUserType);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

#ifdef USE_PARENTHESIS_INFERENCE
	e = dl_array_pushElements(string_array, DL_STR("inferrerContext = "));
	if (e) goto cleanup;
	if (duckVM.inferrerContext == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
	}
#endif /* USE_PARENTHESIS_INFERENCE */

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_symbol_prettyPrint(dl_array_t *string_array, duckVM_symbol_t symbol) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_symbol_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, symbol.name, symbol.name_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("id = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, symbol.id);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_list_prettyPrint(dl_array_t *string_array, duckVM_list_t list, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_list_t) {"));
	if (e) goto cleanup;

	if (list == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = duckVM_object_prettyPrint(string_array, *list, duckVM);
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_cons_prettyPrint(dl_array_t *string_array, duckVM_cons_t cons, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_cons_t) {"));
	if (e) goto cleanup;

	if (cons.car == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = duckVM_object_prettyPrint(string_array, *cons.car, duckVM);
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("."));
	if (e) goto cleanup;

	if (cons.cdr == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = duckVM_object_prettyPrint(string_array, *cons.cdr, duckVM);
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_user_prettyPrint(dl_array_t *string_array, duckVM_user_t user) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_user_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("data = "));
	if (e) goto cleanup;
	if (user.data == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("destructor = "));
	if (e) goto cleanup;
	if (user.destructor == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("marker = "));
	if (e) goto cleanup;
	if (user.marker == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_object_type_prettyPrint(dl_array_t *string_array, duckVM_object_type_t object_type) {
	switch (object_type) {
	case duckVM_object_type_none:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_none"));
	case duckVM_object_type_bool:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_bool"));
	case duckVM_object_type_integer:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_integer"));
	case duckVM_object_type_float:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_float"));
	case duckVM_object_type_list:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_list"));
	case duckVM_object_type_symbol:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_symbol"));
	case duckVM_object_type_type:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_type"));
	case duckVM_object_type_composite:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_composite"));
	case duckVM_object_type_user:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_user"));
	case duckVM_object_type_cons:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_cons"));
	case duckVM_object_type_last:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_last"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}

dl_error_t duckVM_object_prettyPrint(dl_array_t *string_array, duckVM_object_t object, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_object_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("inUse = "));
	if (e) goto cleanup;
	if (object.inUse) {
		e = dl_array_pushElements(string_array, DL_STR("true"));
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("false"));
	}
	if (e) goto cleanup;

	switch (object.type) {
	case duckVM_object_type_bool:
		e = dl_array_pushElements(string_array, DL_STR("bool: (dl_bool_t) "));
		if (e) goto cleanup;
		e = dl_string_fromBool(string_array, object.value.boolean);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_integer:
		e = dl_array_pushElements(string_array, DL_STR("integer: (dl_ptrdiff_t) "));
		if (e) goto cleanup;
		e = dl_string_fromPtrdiff(string_array, object.value.integer);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_float:
		e = dl_array_pushElements(string_array, DL_STR("float: (double) ..."));
		if (e) goto cleanup;
		break;
	case duckVM_object_type_symbol:
		e = duckVM_symbol_prettyPrint(string_array, object.value.symbol);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_list:
		e = duckVM_list_prettyPrint(string_array, object.value.list, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_cons:
		e = duckVM_cons_prettyPrint(string_array, object.value.cons, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_type:
		e = dl_array_pushElements(string_array, DL_STR("type: (dl_size_t) "));
		if (e) goto cleanup;
		e = dl_string_fromSize(string_array, object.value.type);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_user:
		e = duckVM_user_prettyPrint(string_array, object.value.user);
		if (e) goto cleanup;
		break;
	default:
		e = dl_array_pushElements(string_array, DL_STR("INVALID"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
