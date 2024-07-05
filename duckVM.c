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

#include "duckVM.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "duckLisp.h"


duckVM_object_t duckVM_object_makeUpvalueArray(duckVM_object_t **upvalues, dl_size_t length);

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
                              dl_memoryAllocation_t *memoryAllocation,
                              duckVM_t *duckVM,
                              const dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	gclist->memoryAllocation = memoryAllocation;
	gclist->duckVM = duckVM;


	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->objects,
				  maxObjects * sizeof(duckVM_object_t));
	if (e) goto cleanup;
	gclist->objects_length = maxObjects;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->freeObjects,
				  maxObjects * sizeof(duckVM_object_t *));
	if (e) goto cleanup;
	gclist->freeObjects_length = maxObjects;


	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->objectInUse,
				  maxObjects * sizeof(dl_bool_t));
	if (e) goto cleanup;


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

	e = dl_free(gclist->memoryAllocation, (void **) &gclist->freeObjects);
	gclist->freeObjects_length = 0;


	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->objects);
	e = eError ? eError : e;
	gclist->objects_length = 0;


	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->objectInUse);
	e = eError ? eError : e;

	return e;
}

static dl_error_t duckVM_gclist_markObject(duckVM_gclist_t *gclist, duckVM_object_t *object, dl_bool_t stack) {
	dl_error_t e = dl_error_ok;

	/* Array of pointers that need to be traced. */
	dl_array_t dispatchStack;
	(void) dl_array_init(&dispatchStack, gclist->memoryAllocation, sizeof(duckVM_object_t *), dl_array_strategy_double);

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
			else if (object->type == duckVM_object_type_closure) {
				e = dl_array_pushElement(&dispatchStack, &object->value.closure.upvalue_array);
				if (e) goto cleanup;
				e = dl_array_pushElement(&dispatchStack, &object->value.closure.bytecode);
				if (e) goto cleanup;
			}
			else if (object->type == duckVM_object_type_upvalue) {
				if (object->value.upvalue.type == duckVM_upvalue_type_heap_object) {
					e = dl_array_pushElement(&dispatchStack, &object->value.upvalue.value.heap_object);
					if (e) goto cleanup;
				}
				else if (object->value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
					e = dl_array_pushElement(&dispatchStack, &object->value.upvalue.value.heap_upvalue);
					if (e) goto cleanup;
				}
			}
			else if (object->type == duckVM_object_type_upvalueArray) {
				DL_DOTIMES(k, object->value.upvalue_array.length) {
					e = dl_array_pushElement(&dispatchStack, &object->value.upvalue_array.upvalues[k]);
					if (e) goto cleanup;
				}
			}
			else if (object->type == duckVM_object_type_vector) {
				e = dl_array_pushElement(&dispatchStack, &object->value.vector.internal_vector);
				if (e) goto cleanup;
			}
			else if (object->type == duckVM_object_type_internalVector) {
				if (object->value.internal_vector.initialized) {
					DL_DOTIMES(k, object->value.internal_vector.length) {
						e = dl_array_pushElement(&dispatchStack, &object->value.internal_vector.values[k]);
						if (e) goto cleanup;
					}
				}
			}
			else if (object->type == duckVM_object_type_string) {
				if (object->value.string.internalString) {
					e = dl_array_pushElement(&dispatchStack, &object->value.string.internalString);
					if (e) goto cleanup;
				}
			}
			else if (object->type == duckVM_object_type_symbol) {
				if (object->value.symbol.internalString) {
					e = dl_array_pushElement(&dispatchStack, &object->value.symbol.internalString);
					if (e) goto cleanup;
				}
			}
			else if (object->type == duckVM_object_type_composite) {
				e = dl_array_pushElement(&dispatchStack, &object->value.composite);
				if (e) goto cleanup;
			}
			else if (object->type == duckVM_object_type_internalComposite) {
				e = dl_array_pushElement(&dispatchStack, &object->value.internalComposite.value);
				if (e) goto cleanup;
				e = dl_array_pushElement(&dispatchStack, &object->value.internalComposite.function);
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
	e = dl_array_quit(&dispatchStack);
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

	/* Upvalue stack */
	DL_DOTIMES(i, duckVM->upvalue_stack.elements_length) {
		duckVM_object_t *object = DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_object_t *, i);
		if (object != dl_null) {
			e = duckVM_gclist_markObject(gclistPointer, object, dl_false);
			if (e) goto cleanup;
		}
	}

	/* Globals */
	DL_DOTIMES(i, duckVM->globals.elements_length) {
		duckVM_object_t *object = DL_ARRAY_GETADDRESS(duckVM->globals, duckVM_object_t *, i);
		if (object != dl_null) {
			e = duckVM_gclist_markObject(gclistPointer, object, dl_false);
			if (e) goto cleanup;
		}
	}

	/* Call stack */
	DL_DOTIMES(i, duckVM->call_stack.elements_length) {
		duckVM_object_t *object = (DL_ARRAY_GETADDRESS(duckVM->call_stack, duckVM_callFrame_t, i)
		                             .bytecode);
		if (object != dl_null) {
			e = duckVM_gclist_markObject(gclistPointer, object, dl_false);
			if (e) goto cleanup;
		}
	}

	/* Current bytecode */
	if (duckVM->currentBytecode != dl_null) {
		e = duckVM_gclist_markObject(gclistPointer, duckVM->currentBytecode, dl_false);
			if (e) goto cleanup;
	}

	/* Free cells if not marked. */
	gclistPointer->freeObjects_length = 0;  /* This feels horribly inefficient. (That's 'cause it is.) */
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < gclistCopy.objects_length; i++) {
		if (!gclistCopy.objectInUse[i]) {
			duckVM_object_t *objectPointer = &gclistCopy.objects[i];
			gclistCopy.freeObjects[gclistPointer->freeObjects_length++] = objectPointer;
			duckVM_object_t object = *objectPointer;
			duckVM_object_type_t type = object.type;
			if ((type == duckVM_object_type_upvalueArray)
			    /* Prevent multiple frees. */
			    && (object.value.upvalue_array.upvalues != dl_null)) {
				e = DL_FREE(duckVM->memoryAllocation, &objectPointer->value.upvalue_array.upvalues);
				if (e) goto cleanup;
			}
			else if ((type == duckVM_object_type_internalVector)
			         && object.value.internal_vector.initialized
			         /* Prevent multiple frees. */
			         && (object.value.internal_vector.values != dl_null)) {
				e = DL_FREE(duckVM->memoryAllocation, &objectPointer->value.internal_vector.values);
				if (e) goto cleanup;
			}
			else if ((type == duckVM_object_type_bytecode)
			         /* Prevent multiple frees. */
			         && (object.value.bytecode.bytecode != dl_null)) {
				e = DL_FREE(duckVM->memoryAllocation, &objectPointer->value.bytecode.bytecode);
				if (e) goto cleanup;
			}
			else if ((type == duckVM_object_type_internalString)
			         /* Prevent multiple frees. */
			         && (object.value.internalString.value != dl_null)) {
				e = DL_FREE(duckVM->memoryAllocation, &objectPointer->value.internalString.value);
				if (e) goto cleanup;
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
	if (objectIn.type == duckVM_object_type_upvalueArray) {
		if (objectIn.value.upvalue_array.length > 0) {
			e = DL_MALLOC(duckVM->memoryAllocation,
			              (void **) &heapObject->value.upvalue_array.upvalues,
			              objectIn.value.upvalue_array.length,
			              duckVM_object_t **);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_gclist_pushObject: Upvalue array allocation failed."));
				if (!e) e = eError;
				goto cleanup;
			}
			/* Don't copy the source array. */
		}
		else {
			heapObject->value.upvalue_array.upvalues = dl_null;
		}
		if (e) goto cleanup;
	}
	else if (objectIn.type == duckVM_object_type_internalVector) {
		if (objectIn.value.internal_vector.length > 0) {
			e = DL_MALLOC(duckVM->memoryAllocation,
			              (void **) &heapObject->value.internal_vector.values,
			              objectIn.value.internal_vector.length,
			              duckVM_object_t **);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_gclist_pushObject: Vector allocation failed."));
				if (!e) e = eError;
				goto cleanup;
			}
			if (objectIn.value.internal_vector.initialized) {
				(void) dl_memcopy_noOverlap(heapObject->value.internal_vector.values,
				                            objectIn.value.internal_vector.values,
				                            objectIn.value.internal_vector.length * sizeof(duckVM_object_t *));
			}
		}
		else {
			heapObject->value.internal_vector.values = dl_null;
		}
		if (e) goto cleanup;
	}
	else if (objectIn.type == duckVM_object_type_bytecode) {
		if (objectIn.value.bytecode.bytecode_length > 0) {
			e = DL_MALLOC(duckVM->memoryAllocation,
			              &heapObject->value.bytecode.bytecode,
			              objectIn.value.bytecode.bytecode_length,
			              dl_uint8_t);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_gclist_pushObject: Bytecode allocation failed."));
				if (!e) e = eError;
				goto cleanup;
			}
			/**/ dl_memcopy_noOverlap(heapObject->value.bytecode.bytecode,
			                          objectIn.value.bytecode.bytecode,
			                          objectIn.value.bytecode.bytecode_length);
		}
		else {
			heapObject->value.bytecode.bytecode = dl_null;
			heapObject->value.bytecode.bytecode_length = 0;
		}
		if (e) goto cleanup;
	}
	else if (objectIn.type == duckVM_object_type_internalString) {
		if (objectIn.value.internalString.value_length > 0) {
			e = DL_MALLOC(duckVM->memoryAllocation,
			              &heapObject->value.internalString.value,
			              objectIn.value.internalString.value_length,
			              dl_uint8_t);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_gclist_pushObject: String allocation failed."));
				if (!e) e = eError;
				goto cleanup;
			}
			/**/ dl_memcopy_noOverlap(heapObject->value.internalString.value,
			                          objectIn.value.internalString.value,
			                          objectIn.value.internalString.value_length);
		}
		else {
			heapObject->value.internalString.value = dl_null;
			heapObject->value.internalString.value_length = 0;
		}
		if (e) goto cleanup;
	}
	*objectOut = heapObject;

 cleanup:

	return e;
}


dl_error_t duckVM_init(duckVM_t *duckVM, dl_memoryAllocation_t *memoryAllocation, dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	duckVM->memoryAllocation = memoryAllocation;
	duckVM->currentBytecode = dl_null;
	duckVM->nextUserType = duckVM_object_type_last;
	/**/ dl_array_init(&duckVM->errors, duckVM->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);
	/**/ dl_array_init(&duckVM->stack, duckVM->memoryAllocation, sizeof(duckVM_object_t), dl_array_strategy_double);
	/**/ dl_array_init(&duckVM->call_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(duckVM_callFrame_t),
	                   dl_array_strategy_double);
	/**/ dl_array_init(&duckVM->upvalue_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(duckVM_object_t *),
	                   dl_array_strategy_double);
	/**/ dl_array_init(&duckVM->upvalue_array_call_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(duckVM_upvalueArray_t),
	                   dl_array_strategy_double);
	e = dl_array_pushElement(&duckVM->upvalue_array_call_stack, dl_null);
	if (e) goto cleanup;
	/**/ dl_array_init(&duckVM->globals,
	                   duckVM->memoryAllocation,
	                   sizeof(duckVM_object_t *),
	                   dl_array_strategy_double);
	/**/ dl_array_init(&duckVM->globals_map,
	                   duckVM->memoryAllocation,
	                   sizeof(dl_ptrdiff_t),
	                   dl_array_strategy_double);
	e = duckVM_gclist_init(&duckVM->gclist, duckVM->memoryAllocation, duckVM, maxObjects);
	if (e) goto cleanup;
	duckVM->duckLisp = dl_null;
	duckVM->userData = dl_null;

 cleanup:
	return e;
}

void duckVM_quit(duckVM_t *duckVM) {
	dl_error_t e;
	e = dl_array_quit(&duckVM->stack);
	e = dl_array_quit(&duckVM->upvalue_stack);
	e = dl_array_quit(&duckVM->globals);
	e = dl_array_quit(&duckVM->globals_map);
	e = dl_array_quit(&duckVM->call_stack);
	duckVM->currentBytecode = dl_null;
	e = duckVM_gclist_garbageCollect(duckVM);
	e = dl_array_quit(&duckVM->upvalue_array_call_stack);
	/**/ duckVM_gclist_quit(&duckVM->gclist);
	e = dl_array_quit(&duckVM->errors);
	duckVM->duckLisp = dl_null;
	duckVM->userData = dl_null;
	(void) e;
}

static dl_error_t stack_push(duckVM_t *duckVM, duckVM_object_t *object) {
	dl_error_t e = dl_error_ok;
	e = dl_array_pushElement(&duckVM->stack, object);
	if (e) goto cleanup;
	e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
 cleanup:
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
	if (e) goto cleanup;
	e = dl_array_popElement(&duckVM->upvalue_stack, dl_null);
 cleanup:
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("stack_pop: Failed."));
		if (!e) e = eError;
	}
	return e;
}

static dl_error_t stack_pop_multiple(duckVM_t *duckVM, dl_size_t pops) {
	dl_error_t e = dl_error_ok;
	e = dl_array_popElements(&duckVM->upvalue_stack, dl_null, pops);
	if (e) goto cleanup;
	e = dl_array_popElements(&duckVM->stack, dl_null, pops);
 cleanup:
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

static dl_error_t call_stack_push(duckVM_t *duckVM,
                                  dl_uint8_t *ip,
                                  duckVM_object_t *bytecode,
                                  duckVM_upvalueArray_t *upvalueArray) {
	dl_error_t e = dl_error_ok;
	duckVM_callFrame_t frame;
	frame.ip = ip;
	frame.bytecode = bytecode;
	e = dl_array_pushElement(&duckVM->call_stack, &frame);
	if (e) goto cleanup;
	e = dl_array_pushElement(&duckVM->upvalue_array_call_stack, upvalueArray);
	if (e) goto cleanup;
 cleanup:
	if (e) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("call_stack_push: Failed."));
		if (!e) e = eError;
	}
	return e;
}

static dl_error_t call_stack_pop(duckVM_t *duckVM, dl_uint8_t **ip, duckVM_object_t **bytecode) {
	dl_error_t e = dl_error_ok;
	duckVM_callFrame_t frame;
	e = dl_array_popElement(&duckVM->call_stack, &frame);
	if (e) goto cleanup;
	*ip = frame.ip;
	*bytecode = frame.bytecode;
	e = dl_array_popElement(&duckVM->upvalue_array_call_stack, dl_null);
 cleanup:
	if (e && (e != dl_error_bufferUnderflow)) {
		dl_error_t eError = duckVM_error_pushRuntime(duckVM, DL_STR("call_stack_pop: Failed."));
		if (!e) e = eError;
	}
	return e;
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


static dl_error_t duckVM_instruction_prepareForFuncall(duckVM_t *duckVM,
                                                       duckVM_object_t *functionObject,
                                                       dl_uint8_t numberOfArgs) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	do {
		duckVM_object_t restListObject;
		while (functionObject->type == duckVM_object_type_composite) {
			*functionObject = *functionObject->value.composite->value.internalComposite.function;
		}
		if (functionObject->type == duckVM_object_type_function) {
			break;
		}
		else if (functionObject->type != duckVM_object_type_closure) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->funcall: Object is not a callback or closure."));
			if (!e) e = eError;
			break;
		}
		if (functionObject->value.closure.variadic) {
			if (numberOfArgs < functionObject->value.closure.arity) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->funcall: Too few arguments."));
				if (!e) e = eError;
				break;
			}
			/* Create list. */
			/* The original version of this did something really dirty though it didn't look like it. It popped
			   multiple objects off the stack, then allocated a bunch more on the heap. This is fine 99.999% of the
			   time, but since the stack is the root the garbage collector uses to trace memory, it was possible to
			   trigger collection of the object (and possibly all its children) during one of those allocations.
			   Moral of the story: Never perform an allocation if you have an allocated object disconnected from the
			   stack. */
			/* First push the new object on the stack to register it with the collector. */
			restListObject.value.list = dl_null;
			restListObject.type = duckVM_object_type_list;
			e = stack_push(duckVM, &restListObject);
			if (e) break;
			{
				duckVM_object_t nil;
				nil.value.list = dl_null;
				nil.type = duckVM_object_type_list;
				e = stack_push(duckVM, &nil);
				if (e) break;
			}
			dl_size_t args_length = (numberOfArgs - functionObject->value.closure.arity);
			duckVM_object_t *lastConsPtr = dl_null;
			DL_DOTIMES(k, args_length) {
				duckVM_object_t cons;
				duckVM_object_t *consPtr = dl_null;
				duckVM_object_t object;
				duckVM_object_t *objectPtr = dl_null;
				/* Reverse order, because lisp lists are backwards. */
				object = DL_ARRAY_GETADDRESS(duckVM->stack,
				                             duckVM_object_t,
				                             duckVM->stack.elements_length - 3 - k);
				e = duckVM_gclist_pushObject(duckVM, &objectPtr, object);
				if (e) break;
				cons.type = duckVM_object_type_cons;
				cons.value.cons.car = objectPtr;
				cons.value.cons.cdr = lastConsPtr;
				/* Make visible to collector before allocating another. */
				DL_ARRAY_GETADDRESS(duckVM->stack,
				                    duckVM_object_t,
				                    duckVM->stack.elements_length - 1).value.list = objectPtr;
				e = duckVM_gclist_pushObject(duckVM, &consPtr, cons);
				if (e) break;
				/* Make visible to collector before allocating another. The object just created is linked in the
				   cons. */
				DL_ARRAY_GETADDRESS(duckVM->stack,
				                    duckVM_object_t,
				                    duckVM->stack.elements_length - 2).value.list = consPtr;
				lastConsPtr = consPtr;
			}
			if (e) break;
			/* Pop the temporary object. */
			e = stack_pop(duckVM, dl_null);
			if (e) break;
			/* Copy the list to the proper position. */
			restListObject.value.list = lastConsPtr;
			DL_ARRAY_GETADDRESS(duckVM->stack,
			                    duckVM_object_t,
			                    (duckVM->stack.elements_length - 1 - args_length)) = restListObject;
			/* Pop all objects other than the list. */
			e = stack_pop_multiple(duckVM, args_length);
			if (e) break;
		}
		else {
			if (functionObject->value.closure.arity != numberOfArgs) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->funcall: Incorrect number of arguments."));
				if (!e) e = eError;
				break;
			}
		}
	} while (0);
	return e;
}                


int duckVM_executeInstruction(duckVM_t *duckVM,
                              duckVM_object_t *bytecode,
                              unsigned char **ipPtr,
                              duckVM_halt_mode_t *halt) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_uint8_t *ptr1 = dl_null;
	dl_ptrdiff_t ptrdiff1 = 0;
	dl_ptrdiff_t ptrdiff2 = 0;
	dl_ptrdiff_t ptrdiff3 = 0;
	dl_size_t size1 = 0;
	dl_size_t size2 = 0;
	dl_uint8_t uint8 = 0;
	duckVM_object_t object1 = {0};
	duckVM_object_t object2 = {0};
	duckVM_object_t object3 = {0};
	duckVM_object_t *objectPtr1 = {0};
	duckVM_object_t *objectPtr2 = {0};
	duckVM_object_t cons1 = {0};
	dl_bool_t bool1 = dl_false;
	dl_bool_t parsedBytecode = dl_false;
	unsigned char *ip = *ipPtr;
	unsigned char opcode = *(ip++);
	switch (opcode) {
	case duckLisp_instruction_nop:
		break;

	case duckLisp_instruction_pushSymbol32:
		size2 = *(ip++);
		size2 = *(ip++) + (size2 << 8);
		size2 = *(ip++) + (size2 << 8);
		size2 = *(ip++) + (size2 << 8);
		size1 = *(ip++);
		size1 = *(ip++) + (size1 << 8);
		size1 = *(ip++) + (size1 << 8);
		size1 = *(ip++) + (size1 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_pushSymbol16:
		if (!parsedBytecode) {
			size2 = *(ip++);
			size2 = *(ip++) + (size2 << 8);
			size1 = *(ip++);
			size1 = *(ip++) + (size1 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_pushSymbol8:
		if (!parsedBytecode) {
			size2 = *(ip++);
			size1 = *(ip++);
		}
		{
			dl_uint8_t *lastIp = ip;
			ip += size1;
			e = duckVM_object_makeSymbol(duckVM, &object1, size2, lastIp, size1);
			if (e) break;
		}
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-symbol: stack_push failed."));
			if (!e) e = eError;
		}
		break;

	case duckLisp_instruction_pushStrippedSymbol32:
		size2 = *(ip++);
		size2 = *(ip++) + (size2 << 8);
		size2 = *(ip++) + (size2 << 8);
		size2 = *(ip++) + (size2 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_pushStrippedSymbol16:
		if (!parsedBytecode) {
			size2 = *(ip++);
			size2 = *(ip++) + (size2 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_pushStrippedSymbol8:
		if (!parsedBytecode) {
			size2 = *(ip++);
		}
		(void) duckVM_object_makeCompressedSymbol(&object1, size2);
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->push-stripped-symbol: stack_push failed."));
			if (!e) e = eError;
		}
		break;

	case duckLisp_instruction_pushString32:
		size1 = *(ip++);
		size1 = *(ip++) + (size1 << 8);
		// Fall through
	case duckLisp_instruction_pushString16:
		size1 = *(ip++) + (size1 << 8);
		// Fall through
	case duckLisp_instruction_pushString8:
		size1 = *(ip++) + (size1 << 8);
		{
			dl_uint8_t *string = ip;
			dl_size_t string_length = size1;
			ip += size1;
			e = duckVM_object_makeString(duckVM, &object1, string, string_length);
			if (e) {
				(eError
				 = duckVM_error_pushRuntime(duckVM,
				                            DL_STR("duckVM_execute->push-string: duckVM_object_makeString failed.")));
				if (!e) e = eError;
			}
			e = stack_push(duckVM, &object1);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-string: stack_push failed."));
				if (!e) e = eError;
			}
		}
		break;

	case duckLisp_instruction_pushBooleanFalse:
		object1 = duckVM_object_makeBoolean(dl_false);
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-boolean-false: stack_push failed."));
			if (!e) e = eError;
		}
		break;
	case duckLisp_instruction_pushBooleanTrue:
		object1 = duckVM_object_makeBoolean(dl_true);
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-boolean-true: stack_push failed."));
			if (!e) e = eError;
		}
		break;

	case duckLisp_instruction_pushInteger32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		parsedBytecode = dl_true;
		size1 = 0x7FFFFFFF;
		/* Fall through */
	case duckLisp_instruction_pushInteger16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			size1 = 0x7FFF;
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_pushInteger8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			size1 = 0x7F;
		}
		ptrdiff1 = (((dl_size_t) ptrdiff1 > size1)
		            ? -(0x100 - ptrdiff1)
		            : ptrdiff1);
		object1 = duckVM_object_makeInteger(ptrdiff1);
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-integer: stack_push failed."));
			if (!e) e = eError;
		}
		break;

	case duckLisp_instruction_pushDoubleFloat: {
		dl_uint64_t totallyNotADoubleFloat = 0;
		totallyNotADoubleFloat = *(ip++);
		totallyNotADoubleFloat = *(ip++) + (totallyNotADoubleFloat << 8);
		totallyNotADoubleFloat = *(ip++) + (totallyNotADoubleFloat << 8);
		totallyNotADoubleFloat = *(ip++) + (totallyNotADoubleFloat << 8);
		totallyNotADoubleFloat = *(ip++) + (totallyNotADoubleFloat << 8);
		totallyNotADoubleFloat = *(ip++) + (totallyNotADoubleFloat << 8);
		totallyNotADoubleFloat = *(ip++) + (totallyNotADoubleFloat << 8);
		totallyNotADoubleFloat = *(ip++) + (totallyNotADoubleFloat << 8);
		object1 = duckVM_object_makeFloat(*((double *) &totallyNotADoubleFloat));
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-double-float: stack_push failed."));
			if (!e) e = eError;
		}
		break;
	}

	case duckLisp_instruction_pushIndex32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		// Fall through
	case duckLisp_instruction_pushIndex16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		// Fall through
	case duckLisp_instruction_pushIndex8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-index: dl_array_get failed."));
			if (!e) e = eError;
			break;
		}
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->push-index: stack_push failed."));
			if (!e) e = eError;
		}
		break;

	case duckLisp_instruction_pushUpvalue32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_pushUpvalue16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_pushUpvalue8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		{
			duckVM_upvalueArray_t upvalueArray;
			if (ptrdiff1 < 0) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->push-upvalue: Index pointing to upvalue is negative."));
				if (!e) e = eError;
				break;
			}
			e = dl_array_getTop(&duckVM->upvalue_array_call_stack, &upvalueArray);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->push-upvalue: dl_array_get failed."));
				if (!e) e = eError;
				break;
			}
			e = duckVM_upvalueArray_getUpvalue(duckVM, upvalueArray, &object1, ptrdiff1);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->push-upvalue: duckVM_upvalueArray_getUpvalue failed."));
				if (!e) e = eError;
				break;
			}
			e = stack_push(duckVM, &object1);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->push-upvalue: stack_push failed."));
				if (!e) e = eError;
				break;
			}
			break;
		}

	case duckLisp_instruction_pushClosure32:
		/* Fall through */
	case duckLisp_instruction_pushVaClosure32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptr1 = ip;
		if (ptrdiff1 & 0x80000000ULL) {
			ptrdiff1 = -((~ptrdiff1 + 1) & 0xFFFFFFFFULL);
		}
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_pushClosure16:
		/* Fall through */
	case duckLisp_instruction_pushVaClosure16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptr1 = ip;
			if (ptrdiff1 & 0x8000ULL) {
				ptrdiff1 = -((~ptrdiff1 + 1) & 0xFFFFULL);
			}
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_pushClosure8:
		/* Fall through */
	case duckLisp_instruction_pushVaClosure8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptr1 = ip;
			if (ptrdiff1 & 0x80ULL) {
				ptrdiff1 = -((~ptrdiff1 + 1) & 0xFFULL);
			}
		}

		object1 = duckVM_object_makeClosure(ptrdiff1,
		                                    bytecode,
		                                    dl_null,
		                                    *(ip++),
		                                    ((opcode == duckLisp_instruction_pushVaClosure32)
		                                     || (opcode == duckLisp_instruction_pushVaClosure16)
		                                     || (opcode == duckLisp_instruction_pushVaClosure8)));

		size1 = *(ip++);
		size1 = *(ip++) + (size1 << 8);
		size1 = *(ip++) + (size1 << 8);
		size1 = *(ip++) + (size1 << 8);

		object1.value.closure.name += ptr1 - bytecode->value.bytecode.bytecode;

		/* This could also point to a static version instead since this array is never changed
		   and multiple closures could use the same array. */

		e = duckVM_gclist_pushObject(duckVM,
		                             &object1.value.closure.upvalue_array,
		                             duckVM_object_makeUpvalueArray(dl_null, size1));
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->push-closure: duckVM_gclist_pushObject failed."));
			if (!e) e = eError;
			break;
		}
		duckVM_upvalueArray_t upvalueArray;
		e = duckVM_closure_getUpvalueArray(object1.value.closure, &upvalueArray);
		if (e) break;
		DL_DOTIMES(k, upvalueArray.length) {
			upvalueArray.upvalues[k] = dl_null;
		}
		/* Immediately push on stack so that the GC can see it. Allocating upvalues could trigger a GC. */
		e = dl_array_pushElement(&duckVM->stack, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->push-closure: dl_array_pushElement failed."));
			if (!e) e = eError;
			break;
		}

		dl_bool_t recursive = dl_false;

		DL_DOTIMES(k, upvalueArray.length) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			if (ptrdiff1 > 0x7FFFFFFF) {
				/* Nested upvalue */
				ptrdiff1 = -(0x100000000 - ptrdiff1);
			}
			else {
				/* Normal upvalue */
				/* stack - 1 because we already pushed. */
				ptrdiff1 = (duckVM->stack.elements_length - 1) - ptrdiff1;
				if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->upvalue_stack.elements_length)) {
					e = dl_error_invalidValue;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->push-closure: Stack index out of bounds."));
					if (!e) e = eError;
					break;
				}
			}
			duckVM_object_t *upvalue_pointer = dl_null;
			if ((dl_size_t) ptrdiff1 == duckVM->upvalue_stack.elements_length) {
				/* *Recursion* Capture upvalue we are about to push. This should only happen once. */
				recursive = dl_true;
				/* Our upvalue definitely doesn't exist yet. */
				duckVM_object_t upvalue;
				upvalue.type = duckVM_object_type_upvalue;
				upvalue.value.upvalue.type = duckVM_upvalue_type_stack_index;  /* Lie for now. */
				upvalue.value.upvalue.value.stack_index = ptrdiff1;
				e = duckVM_gclist_pushObject(duckVM, &upvalue_pointer, upvalue);
				if (e) {
					e = dl_error_shouldntHappen;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->push-closure: duckVM_gclist_pushObject failed."));
					if (!e) e = eError;
					break;
				}
				/* Break push_scope abstraction. */
				e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
				if (e) {
					e = dl_error_shouldntHappen;
					(eError
					 = duckVM_error_pushRuntime(duckVM,
					                            DL_STR("duckVM_execute->push-closure: dl_array_pushElement failed.")));
					if (!e) e = eError;
					break;
				}
				/* Upvalue stack slot exists now, so insert the new UV. */
				DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_object_t *, ptrdiff1) = upvalue_pointer;
			}
			else if (ptrdiff1 < 0) {
				/* Capture upvalue in currently executing function. */
				ptrdiff1 = -(ptrdiff1 + 1);
				duckVM_object_t upvalue;
				upvalue.type = duckVM_object_type_upvalue;
				upvalue.value.upvalue.type = duckVM_upvalue_type_heap_upvalue;
				/* Link new upvalue to upvalue from current context. */
				upvalue.value.upvalue.value.heap_upvalue = (DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
				                                                                   duckVM_upvalueArray_t)
				                                            .upvalues[ptrdiff1]);
				e = duckVM_gclist_pushObject(duckVM, &upvalue_pointer, upvalue);
				if (e) {
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->push-closure: duckVM_gclist_pushObject failed."));
					if (!e) e = eError;
					break;
				}
			}
			else {
				/* Capture upvalue on stack. */
				e = dl_array_get(&duckVM->upvalue_stack, &upvalue_pointer, ptrdiff1);
				if (e) {
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->push-closure: Retrieval of upvalue from upvalue stack failed."));
					if (!e) e = eError;
					break;
				}
				if (upvalue_pointer == dl_null) {
					duckVM_object_t upvalue;
					upvalue.type = duckVM_object_type_upvalue;
					upvalue.value.upvalue.type = duckVM_upvalue_type_stack_index;
					upvalue.value.upvalue.value.stack_index = ptrdiff1;
					e = duckVM_gclist_pushObject(duckVM, &upvalue_pointer, upvalue);
					if (e) {
						eError = duckVM_error_pushRuntime(duckVM,
						                                  DL_STR("duckVM_execute->push-closure: duckVM_gclist_pushObject failed."));
						if (!e) e = eError;
						break;
					}
					// Is this a bug? I'm not confident that this element exists.
					/* Keep reference to upvalue on stack so that we know where to release the object to. */
					DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_object_t *, ptrdiff1) = upvalue_pointer;
				}
				if (upvalue_pointer->type != duckVM_object_type_upvalue) {
					e = dl_error_shouldntHappen;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->push-closure: Captured object is not an upvalue."));
					if (!e) e = eError;
					break;
				}
			}
			if (e) break;
			upvalueArray.upvalues[k] = upvalue_pointer;
		}
		if (e) break;
		if (!recursive) {
			/* Break push_scope abstraction. */
			e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->push-closure: dl_array_pushElement failed."));
				if (!e) e = eError;
				break;
			}
		}
		DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckVM_object_t) = object1;

		break;

	case duckLisp_instruction_pushGlobal32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_pushGlobal16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_pushGlobal8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
		}
		{
			duckVM_object_t *global;
			e = duckVM_global_get(duckVM, &global, ptrdiff1);
			if (e) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->push-global: Could not find dynamic variable."));
				if (eError) e = eError;
				break;
			}
			e = stack_push(duckVM, global);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->push-global: stack_push failed."));
				if (!e) e = eError;
				break;
			}
		}
		break;

	case duckLisp_instruction_releaseUpvalues32:
	case duckLisp_instruction_releaseUpvalues16:
	case duckLisp_instruction_releaseUpvalues8:
		size1 = *(ip++);

		DL_DOTIMES(k, size1) {
			ptrdiff1 = *(ip++);
			switch (opcode) {
			case duckLisp_instruction_releaseUpvalues32:
				ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
				ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
				/* Fall through */
			case duckLisp_instruction_releaseUpvalues16:
				ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
				/* Fall through */
			case duckLisp_instruction_releaseUpvalues8:
				break;
			default:
				e = dl_error_invalidValue;
				break;
			}
			ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
			if (ptrdiff1 < 0) {
				e = dl_error_invalidValue;
				break;
			}
			duckVM_object_t *upvalue = DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_object_t *, ptrdiff1);
			if (upvalue != dl_null) {
				if (upvalue->type != duckVM_object_type_upvalue) {
					eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->release-upvalues: Captured object is not an upvalue."));
					if (!e) e = eError;
					break;
				}
				duckVM_object_t *object = &DL_ARRAY_GETADDRESS(duckVM->stack, duckVM_object_t, ptrdiff1);
				e = duckVM_gclist_pushObject(duckVM, &upvalue->value.upvalue.value.heap_object, *object);
				if (e) break;
				upvalue->value.upvalue.type = duckVM_upvalue_type_heap_object;
				/* Render the original object unusable. */
				object->type = duckVM_object_type_list;
				object->value.list = dl_null;
				DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_object_t *, ptrdiff1) = dl_null;
			}
		}
		if (e) break;
		break;

	case duckLisp_instruction_setUpvalue32:
		ptrdiff1 = *(ip++);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_setUpvalue16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_setUpvalue8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		{
			// Need another stack with same depth as call stack to pull out upvalues.
			// Like to live dangerously?
			if (ptrdiff1 < 0) {
				e = dl_error_invalidValue;
				break;
			}
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff2);
			if (e) break;
			duckVM_object_t *upvalue = DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
			                                                  duckVM_upvalueArray_t).upvalues[ptrdiff1];
			if (upvalue->value.upvalue.type == duckVM_upvalue_type_stack_index) {
				e = dl_array_set(&duckVM->stack, &object1, upvalue->value.upvalue.value.stack_index);
				if (e) break;
			}
			else if (upvalue->value.upvalue.type == duckVM_upvalue_type_heap_object) {
				*upvalue->value.upvalue.value.heap_object = object1;
			}
			else {
				while (upvalue->value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
					upvalue = upvalue->value.upvalue.value.heap_upvalue;
				}
				if (upvalue->value.upvalue.type == duckVM_upvalue_type_stack_index) {
					e = dl_array_set(&duckVM->stack, &object1, upvalue->value.upvalue.value.stack_index);
					if (e) break;
				}
				else {
					*upvalue->value.upvalue.value.heap_object = object1;
				}
			}
		}
		break;

	case duckLisp_instruction_setGlobal32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_setGlobal16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_setGlobal8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = duckVM_gclist_pushObject(duckVM,
		                             &objectPtr1,
		                             DL_ARRAY_GETADDRESS(duckVM->stack,
		                                                 duckVM_object_t,
		                                                 duckVM->stack.elements_length - ptrdiff1));
		if (e) break;
		e = duckVM_global_set(duckVM, objectPtr1, ptrdiff2);
		if (e) break;
		break;

	case duckLisp_instruction_funcall32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_funcall16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_funcall8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		uint8 = *(ip++);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = duckVM_instruction_prepareForFuncall(duckVM, &object1, uint8);
		if (e) break;
		if (object1.type == duckVM_object_type_function) {
			e = object1.value.function.callback(duckVM);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->funcall: C callback returned error."));
				if (!e) e = eError;
				break;
			}
			break;
		}
		else if (object1.type != duckVM_object_type_closure) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->funcall: Object is not a callback or closure."));
			if (!e) e = eError;
			break;
		}
		/* Call. */
		e = call_stack_push(duckVM, ip, bytecode, &object1.value.closure.upvalue_array->value.upvalue_array);
		if (e) break;
		bytecode = object1.value.closure.bytecode;
		ip = &bytecode->value.bytecode.bytecode[object1.value.closure.name];
		break;

	case duckLisp_instruction_apply32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_apply16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_apply8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		uint8 = *(ip++);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		while (object1.type == duckVM_object_type_composite) {
			object1 = *object1.value.composite->value.internalComposite.function;
		}
		if (object1.type != duckVM_object_type_closure) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->apply: Applied object is not a closure."));
			if (eError) e = eError;
			break;
		}
		duckVM_object_t rest;
		e = stack_pop(duckVM, &rest);
		if (e) break;
		if (rest.type != duckVM_object_type_list) {
			e = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->apply: Last argument is not a list."));
			e = dl_error_invalidValue;
			break;
		}
		while ((uint8 < object1.value.closure.arity) && (rest.value.list != dl_null)) {
			if (rest.value.list->type != duckVM_object_type_cons) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->apply: Object pointed to by list root is not a list."));
				if (eError) e = eError;
				break;
			}
			/* (push (car rest) stack) */
			{
				duckVM_object_t *car = rest.value.list->value.cons.car;
				if ((car == dl_null) || (car->type == duckVM_object_type_cons)) {
					duckVM_object_t list = duckVM_object_makeList(car);
					e = stack_push(duckVM, &list);
				}
				else {
					e = stack_push(duckVM, car);
				}
				if (e) break;
			}
			/* (setf rest (cdr rest)) */
			rest.value.list = rest.value.list->value.cons.cdr;
			uint8++;
		}
		if (e) break;
		if (object1.value.closure.variadic) {
			if (uint8 < object1.value.closure.arity) {
				e = dl_error_invalidValue;
				break;
			}
			/* Create list. */
			/* The original version of this did something really dirty though it didn't look like it. It popped
			   multiple objects off the stack, then allocated a bunch more on the heap. This is fine 99.999% of the
			   time, but since the stack is the root the garbage collector uses to trace memory, it was possible to
			   trigger collection of the object (and possibly all its children) during one of those allocations.
			   Moral of the story: Never perform an allocation if you have an allocated object disconnected from the
			   stack. */
			/* First push the new object on the stack to register it with the collector. */
			object2.value.list = rest.value.list;
			object2.type = duckVM_object_type_list;
			e = stack_push(duckVM, &object2);
			if (e) break;
			object3.value.list = dl_null;
			object3.type = duckVM_object_type_list;
			e = stack_push(duckVM, &object3);
			if (e) break;
			dl_size_t args_length = (uint8 - object1.value.closure.arity);
			duckVM_object_t *lastConsPtr = rest.value.list;
			DL_DOTIMES(k, args_length) {
				duckVM_object_t cons;
				duckVM_object_t *consPtr = dl_null;
				duckVM_object_t object;
				duckVM_object_t *objectPtr = dl_null;
				/* Reverse order, because lisp lists are backwards. */
				object = DL_ARRAY_GETADDRESS(duckVM->stack,
				                             duckVM_object_t,
				                             duckVM->stack.elements_length - 3 - k);
				e = duckVM_gclist_pushObject(duckVM, &objectPtr, object);
				if (e) break;
				cons.type = duckVM_object_type_cons;
				cons.value.cons.car = objectPtr;
				cons.value.cons.cdr = lastConsPtr;
				/* Make visible to collector before allocating another. */
				DL_ARRAY_GETADDRESS(duckVM->stack,
				                    duckVM_object_t,
				                    duckVM->stack.elements_length - 1).value.list = objectPtr;
				e = duckVM_gclist_pushObject(duckVM, &consPtr, cons);
				if (e) break;
				/* Make visible to collector before allocating another. The object just created is linked in the
				   cons. */
				DL_ARRAY_GETADDRESS(duckVM->stack,
				                    duckVM_object_t,
				                    duckVM->stack.elements_length - 2).value.list = consPtr;
				lastConsPtr = consPtr;
			}
			if (e) break;
			/* Pop the temporary object. */
			e = stack_pop(duckVM, dl_null);
			if (e) break;
			/* Copy the list to the proper position. */
			object2.value.list = lastConsPtr;
			DL_ARRAY_GETADDRESS(duckVM->stack,
			                    duckVM_object_t,
			                    (duckVM->stack.elements_length - 1 - args_length)) = object2;
			/* Pop all objects other than the list. */
			e = stack_pop_multiple(duckVM, args_length);
			if (e) break;
		}
		else {
			if (object1.value.closure.arity != uint8) {
				e = dl_error_invalidValue;
				break;
			}
		}
		/* Call. */
		e = call_stack_push(duckVM, ip, bytecode, &object1.value.closure.upvalue_array->value.upvalue_array);
		if (e) break;
		bytecode = object1.value.closure.bytecode;
		ip = &bytecode->value.bytecode.bytecode[object1.value.closure.name];
		break;

		/* I probably don't need an `if` if I research the standard a bit. */
	case duckLisp_instruction_call32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_call16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_call8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		call_stack_push(duckVM, ip, bytecode, dl_null);
		if (e) break;
		if (opcode == duckLisp_instruction_call32) {
			if (ptrdiff1 & 0x80000000ULL) {
				ip -= ((~ptrdiff1 + 1) & 0xFFFFFFFFULL);
			}
			else {
				ip += ptrdiff1;
			}
		}
		else if (opcode == duckLisp_instruction_call16) {
			if (ptrdiff1 & 0x8000ULL) {
				ip -= ((~ptrdiff1 + 1) & 0xFFFFULL);
			}
			else {
				ip += ptrdiff1;
			}
		}
		else {
			if (ptrdiff1 & 0x80ULL) {
				ip -= ((~ptrdiff1 + 1) & 0xFFULL);
			}
			else {
				ip += ptrdiff1;
			}
		}
		--ip;
		break;

	case duckLisp_instruction_ccall32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_ccall16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_ccall8:
		// I should probably delete this and have `funcall` handle callbacks.
		{
			duckVM_object_t *global;
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = duckVM_global_get(duckVM, &global, ptrdiff1);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->c-call: Could not find global callback."));
				if (!e) e = eError;
				break;
			}
			e = global->value.function.callback(duckVM);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->c-call: C callback returned error."));
				if (!e) e = eError;
				break;
			}
		}
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_jump32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		if (ptrdiff1 & 0x80000000ULL) {
			ip -= ((~ptrdiff1 + 1) & 0xFFFFFFFFULL);
		}
		else {
			ip += ptrdiff1;
		}
		break;
	case duckLisp_instruction_jump16:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		if (ptrdiff1 & 0x8000ULL) {
			ip -= ((~ptrdiff1 + 1) & 0xFFFFULL);
		}
		else {
			ip += ptrdiff1;
		}
		break;
	case duckLisp_instruction_jump8:
		ptrdiff1 = *(ip++);
		if (ptrdiff1 & 0x80ULL) {
			ip -= ((~ptrdiff1 + 1) & 0xFFULL);
		}
		else {
			ip += ptrdiff1;
		}
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_brnz32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_brnz16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_brnz8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - 1);
		if (e) break;
		ptrdiff2 = *(ip++);
		e = stack_pop_multiple(duckVM, ptrdiff2);
		if (e) break;
		{
			dl_bool_t truthy = dl_false;
			if (object1.type == duckVM_object_type_bool) {
				truthy = object1.value.boolean;
			}
			else if (object1.type == duckVM_object_type_integer) {
				truthy = object1.value.integer != 0;
			}
			else if (object1.type == duckVM_object_type_float) {
				truthy = object1.value.floatingPoint != 0.0;
			}
			else if (object1.type == duckVM_object_type_symbol) {
				truthy = dl_true;
			}
			else if (object1.type == duckVM_object_type_list) {
				truthy = object1.value.list != dl_null;
			}
			else if (object1.type == duckVM_object_type_closure) {
				truthy = dl_true;
			}
			else if (object1.type == duckVM_object_type_function) {
				truthy = dl_true;
			}
			else if (object1.type == duckVM_object_type_string) {
				truthy = dl_true;
			}
			else if (object1.type == duckVM_object_type_vector) {
				truthy = ((object1.value.vector.internal_vector != dl_null)
				          && ((dl_size_t) object1.value.vector.offset
				              < object1.value.vector.internal_vector->value.internal_vector.length));
			}
			if (truthy) {
				if ((opcode == duckLisp_instruction_brnz8) && (ptrdiff1 & 0x80ULL)) {
					ip -= ((~ptrdiff1 + 1) & 0xFFULL);
				}
				else if ((opcode == duckLisp_instruction_brnz16) && (ptrdiff1 & 0x8000ULL)) {
					ip -= ((~ptrdiff1 + 1) & 0xFFFFULL);
				}
				else if ((opcode == duckLisp_instruction_brnz32) && (ptrdiff1 & 0x80000000ULL)) {
					ip -= ((~ptrdiff1 + 1) & 0xFFFFFFFFULL);
				}
				else {
					ip += ptrdiff1;
				}
				--ip; /* This accounts for the pop argument. */
			}
		}
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_pop32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		// Fall through
	case duckLisp_instruction_pop16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		// Fall through
	case duckLisp_instruction_pop8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = stack_pop_multiple(duckVM, ptrdiff1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_move32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		// Fall through
	case duckLisp_instruction_move16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		// Fall through
	case duckLisp_instruction_move8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		object2 = object1;
		e = dl_array_set(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_not32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_not16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_not8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_vector:
			object1.type = duckVM_object_type_bool;
			object1.value.boolean = object1.value.vector.internal_vector == dl_null;
			break;
		case duckVM_object_type_list:
			object1.type = duckVM_object_type_bool;
			object1.value.boolean = object1.value.list == dl_null;
			break;
		case duckVM_object_type_integer:
			object1.value.integer = !object1.value.integer;
			break;
		case duckVM_object_type_float:
			object1.value.floatingPoint = !object1.value.floatingPoint;
			break;
		case duckVM_object_type_bool:
			object1.value.boolean = !object1.value.boolean;
			break;
		default:
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->not: Object is not a boolean, integer, float, list, or vector."));
			if (!e) e = eError;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_mul32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint *= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint *= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint *= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer * object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer *= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer *= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean * object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean * object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean *= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		if (e) break;
		break;
	case duckLisp_instruction_mul16:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint *= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint *= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint *= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer * object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer *= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer *= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean * object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean * object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean *= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		break;
	case duckLisp_instruction_mul8:
		ptrdiff1 = *(ip++);
		ptrdiff2 = *(ip++);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint *= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint *= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint *= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer * object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer *= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer *= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean * object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean * object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean *= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_div32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint /= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint /= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint /= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer / object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer /= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer /= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean / object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean / object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean /= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		if (e) break;
		break;
	case duckLisp_instruction_div16:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint /= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint /= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint /= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer / object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer /= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer /= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean / object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean / object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean /= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		break;
	case duckLisp_instruction_div8:
		ptrdiff1 = *(ip++);
		ptrdiff2 = *(ip++);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint /= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint /= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint /= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer / object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer /= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer /= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean / object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean / object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean /= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		break;

	case duckLisp_instruction_add32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_add16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_add8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint += object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint += object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint += object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->add: Invalid type combination."));
				if (eError) e = eError;
				break;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer + object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer += object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer += object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->add: Invalid type combination."));
				if (eError) e = eError;
				break;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean + object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean + object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean += object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->add: Invalid type combination."));
				if (eError) e = eError;
				break;
			}
			break;
		default:
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->add: Invalid type combination."));
			if (eError) e = eError;
			break;
		}
		if (e) break;
		e = stack_push(duckVM, &object1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_sub32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint -= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint -= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint -= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer - object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer -= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer -= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean - object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean - object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean -= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		if (e) break;
		break;
	case duckLisp_instruction_sub16:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint -= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint -= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint -= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer - object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer -= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer -= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean - object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean - object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean -= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		break;
	case duckLisp_instruction_sub8:
		ptrdiff1 = *(ip++);
		ptrdiff2 = *(ip++);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint -= object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.floatingPoint -= object2.value.integer;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_bool:
				object1.value.floatingPoint -= object2.value.boolean;
				object1.type = duckVM_object_type_float;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.integer - object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				/* Type already set. */
				object1.value.integer -= object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.integer -= object2.value.boolean;
				object1.type = duckVM_object_type_integer;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.floatingPoint = object1.value.boolean - object2.value.floatingPoint;
				object1.type = duckVM_object_type_float;
				break;
			case duckVM_object_type_integer:
				object1.value.integer = object1.value.boolean - object2.value.integer;
				object1.type = duckVM_object_type_integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean -= object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = stack_push(duckVM, &object1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_greater32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.floatingPoint > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.floatingPoint > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.floatingPoint > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.integer > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.integer > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.integer > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.boolean > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.boolean > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.boolean > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		object1.type = duckVM_object_type_bool;
		e = stack_push(duckVM, &object1);
		if (e) break;
		break;
	case duckLisp_instruction_greater16:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.floatingPoint > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.floatingPoint > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.floatingPoint > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.integer > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.integer > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.integer > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.boolean > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.boolean > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.boolean > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		object1.type = duckVM_object_type_bool;
		e = stack_push(duckVM, &object1);
		break;
	case duckLisp_instruction_greater8:
		ptrdiff1 = *(ip++);
		ptrdiff2 = *(ip++);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.floatingPoint > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.floatingPoint > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.floatingPoint > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.integer > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.integer > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.integer > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.boolean > object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.boolean > object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.boolean > object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		object1.type = duckVM_object_type_bool;
		e = stack_push(duckVM, &object1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_equal32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		// Fall through
	case duckLisp_instruction_equal16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		}
		// Fall through
	case duckLisp_instruction_equal8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_list:
			switch (object2.type) {
			case duckVM_object_type_list:
				object1.value.boolean = object1.value.list == object2.value.list;
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		case duckVM_object_type_symbol:
			switch (object2.type) {
			case duckVM_object_type_symbol:
				object1.value.boolean = object1.value.symbol.id == object2.value.symbol.id;
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		case duckVM_object_type_string:
			switch (object2.type) {
			case duckVM_object_type_string:
				if ((object1.value.string.length - object1.value.string.offset == 0)
				    && (object2.value.string.length - object2.value.string.offset == 0)) {
					object1.value.boolean = dl_true;
				}
				else if ((object1.value.string.length - object1.value.string.offset == 0)
				         || (object2.value.string.length - object2.value.string.offset == 0)) {
					object1.value.boolean = dl_false;
				}
				else {
					/**/ dl_string_compare(&bool1,
					                       (object1.value.string.internalString->value.internalString.value
					                        + object1.value.string.offset),
					                       object1.value.string.length - object1.value.string.offset,
					                       (object2.value.string.internalString->value.internalString.value
					                        + object2.value.string.offset),
					                       object2.value.string.length - object2.value.string.offset);
					object1.value.boolean = bool1;
				}
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.floatingPoint == object2.value.floatingPoint;
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.integer == object2.value.integer;
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.boolean == object2.value.boolean;
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		case duckVM_object_type_vector:
			switch (object2.type) {
			case duckVM_object_type_vector:
				object1.value.boolean = ((object1.value.vector.internal_vector == object2.value.vector.internal_vector)
				                         && (object1.value.vector.offset == object2.value.vector.offset));
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		case duckVM_object_type_type:
			switch (object2.type) {
			case duckVM_object_type_type:
				object1.value.boolean = (object1.value.type == object2.value.type);
				break;
			default:
				object1.value.boolean = dl_false;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		object1.type = duckVM_object_type_bool;
		e = stack_push(duckVM, &object1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_less32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.floatingPoint < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.floatingPoint < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.floatingPoint < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.integer < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.integer < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.integer < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.boolean < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.boolean < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.boolean < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		object1.type = duckVM_object_type_bool;
		e = stack_push(duckVM, &object1);
		if (e) break;
		break;
	case duckLisp_instruction_less16:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.floatingPoint < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.floatingPoint < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.floatingPoint < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.integer < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.integer < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.integer < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.boolean < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.boolean < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.boolean < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		object1.type = duckVM_object_type_bool;
		e = stack_push(duckVM, &object1);
		break;
	case duckLisp_instruction_less8:
		ptrdiff1 = *(ip++);
		ptrdiff2 = *(ip++);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		switch (object1.type) {
		case duckVM_object_type_float:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.floatingPoint < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.floatingPoint < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.floatingPoint < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_integer:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.integer < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.integer < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.integer < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		case duckVM_object_type_bool:
			switch (object2.type) {
			case duckVM_object_type_float:
				object1.value.boolean = object1.value.boolean < object2.value.floatingPoint;
				break;
			case duckVM_object_type_integer:
				object1.value.boolean = object1.value.boolean < object2.value.integer;
				break;
			case duckVM_object_type_bool:
				object1.value.boolean = object1.value.boolean < object2.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
			break;
		default:
			e = dl_error_invalidValue;
			goto cleanup;
		}
		object1.type = duckVM_object_type_bool;
		e = stack_push(duckVM, &object1);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_cons32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_cons16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_cons8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;
		/* Push dummy cons first so that it can't get collected. */
		cons1.type = duckVM_object_type_cons;
		cons1.value.cons.car = dl_null;
		cons1.value.cons.cdr = dl_null;
		object3.type = duckVM_object_type_list;
		e = duckVM_gclist_pushObject(duckVM, &object3.value.list, cons1);
		if (e) break;
		e = stack_push(duckVM, &object3);
		if (e) break;
		/* Create the elements of the cons. */
		if (object1.type == duckVM_object_type_list) {
			object3.value.list->value.cons.car = object1.value.list;
		}
		else {
			e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
			if (e) break;
			object3.value.list->value.cons.car = objectPtr1;
		}
		if (object2.type == duckVM_object_type_list) {
			object3.value.list->value.cons.cdr = object2.value.list;
		}
		else {
			e = duckVM_gclist_pushObject(duckVM, &objectPtr2, object2);
			if (e) break;
			object3.value.list->value.cons.cdr = objectPtr2;
		}
		DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckVM_object_t) = object3;
		break;

	case duckLisp_instruction_vector32:
		size1 = *(ip++);
		size1 = *(ip++) + (size1 << 8);
		/* Fall through */
	case duckLisp_instruction_vector16:
		size1 = *(ip++) + (size1 << 8);
		/* Fall through */
	case duckLisp_instruction_vector8:
		size1 = *(ip++) + (size1 << 8);

		object1.type = duckVM_object_type_vector;
		object1.value.vector.offset = 0;
		object1.value.vector.internal_vector = dl_null;

		{
			duckVM_object_t internal_vector;
			internal_vector.type = duckVM_object_type_internalVector;
			internal_vector.value.internal_vector.initialized = dl_false;
			internal_vector.value.internal_vector.length = size1;
			e = duckVM_gclist_pushObject(duckVM, &object1.value.vector.internal_vector, internal_vector);
			if (e) break;
		}
		/* Immediately push on stack so that the GC can see it. Allocating elements could trigger a GC. */
		e = dl_array_pushElement(&duckVM->stack, &object1);
		if (e) break;
		DL_DOTIMES(k, object1.value.vector.internal_vector->value.internal_vector.length) {
			ptrdiff1 = *(ip++);
			switch (opcode) {
			case duckLisp_instruction_vector32:
				ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
				ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
				/* Fall through */
			case duckLisp_instruction_vector16:
				ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
				/* Fall through */
			case duckLisp_instruction_vector8:
				break;
			default:
				e = dl_error_invalidValue;
				break;
			}
			if (e) break;
			/* stack - 1 because we already pushed. */
			dl_size_t old_stack_length = duckVM->stack.elements_length - 1;
			ptrdiff1 = old_stack_length - ptrdiff1;
			if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > old_stack_length)) {
				e = dl_error_invalidValue;
				break;
			}

			e = dl_array_get(&duckVM->stack, &object2, ptrdiff1);
			if (e) break;
			e = duckVM_gclist_pushObject(duckVM,
			                             &object1.value.vector.internal_vector->value.internal_vector.values[k],
			                             object2);
			if (e) break;
		}
		if (e) break;

		/* Break push_scope abstraction. */
		e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
		if (e) break;
		object1.value.vector.internal_vector->value.internal_vector.initialized = dl_true;
		DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckVM_object_t) = object1;
		break;

	case duckLisp_instruction_makeVector32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_makeVector16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_makeVector8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		ptrdiff2 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_makeVector32:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_makeVector16:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_makeVector8:
			break;
		default:
			e = dl_error_invalidValue;
			break;
		}
		if (e) break;

		ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
		if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object1, ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_integer) {
			e = dl_error_invalidValue;
			break;
		}
		if (object1.value.integer < 0) {
			e = dl_error_invalidValue;
			break;
		}
		size1 = object1.value.integer;

		ptrdiff2 = duckVM->stack.elements_length - ptrdiff2;
		if ((ptrdiff2 < 0) || ((dl_size_t) ptrdiff2 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object2, ptrdiff2);
		if (e) break;

		object1.type = duckVM_object_type_vector;
		object1.value.vector.offset = 0;
		object1.value.vector.internal_vector = dl_null;

		{
			duckVM_object_t internal_vector;
			internal_vector.type = duckVM_object_type_internalVector;
			internal_vector.value.internal_vector.initialized = dl_false;
			internal_vector.value.internal_vector.length = size1;
			e = duckVM_gclist_pushObject(duckVM, &object1.value.vector.internal_vector, internal_vector);
			if (e) break;
		}
		/* Immediately push on stack so that the GC can see it. Allocating elements could trigger a GC. */
		e = dl_array_pushElement(&duckVM->stack, &object1);
		if (e) break;
		e = duckVM_gclist_pushObject(duckVM,
		                             &objectPtr1,
		                             object2);
		if (e) break;
		DL_DOTIMES(k, object1.value.vector.internal_vector->value.internal_vector.length) {
			object1.value.vector.internal_vector->value.internal_vector.values[k] = objectPtr1;
		}

		if (e) break;

		/* Break push_scope abstraction. */
		e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
		if (e) break;
		object1.value.vector.internal_vector->value.internal_vector.initialized = dl_true;
		DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckVM_object_t) = object1;
		break;

	case duckLisp_instruction_getVecElt32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_getVecElt16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_getVecElt8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		ptrdiff2 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_getVecElt32:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_getVecElt16:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_getVecElt8:
			break;
		default:
			e = dl_error_cantHappen;
			break;
		}
		if (e) break;

		ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
		if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->get-vector-element: Vector stack index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object1, ptrdiff1);
		if (e) break;
		if ((object1.type != duckVM_object_type_vector) && (object1.type != duckVM_object_type_string)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->get-vector-element: dl_array_get failed."));
			if (!e) e = eError;
			break;
		}

		ptrdiff2 = duckVM->stack.elements_length - ptrdiff2;
		if ((ptrdiff2 < 0) || ((dl_size_t) ptrdiff2 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->get-vector-element: Index stack index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object2, ptrdiff2);
		if (e) break;
		if (object2.type != duckVM_object_type_integer) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->get-vector-element: dl_array_get failed."));
			if (!e) e = eError;
			break;
		}

		if (object1.type == duckVM_object_type_vector) {
			ptrdiff1 = object2.value.integer;
			if ((ptrdiff1 < 0)
			    || ((dl_size_t) (ptrdiff1 + object1.value.vector.offset)
			        >= object1.value.vector.internal_vector->value.internal_vector.length)) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->get-vector-element: Vector index out of bounds."));
				if (!e) e = eError;
				break;
			}
			object2 = *object1.value.vector.internal_vector->value.internal_vector.values[object1.value.vector.offset
			                                                                              + ptrdiff1];
		}
		else if (object1.type == duckVM_object_type_string) {
			ptrdiff1 = object2.value.integer;
			if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 >= object1.value.string.length - object1.value.string.offset)) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->get-vector-element: String index out of bounds."));
				if (!e) e = eError;
				break;
			}
			object2.type = duckVM_object_type_integer;
			object2.value.integer = (object1.value.string.internalString
			                         ->value.internalString.value[object1.value.string.offset
			                                                      + ptrdiff1]);
		}
		e = stack_push(duckVM, &object2);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->get-vector-element: stack_push failed."));
			if (!e) e = eError;
			break;
		}
		break;

	case duckLisp_instruction_setVecElt32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_setVecElt16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_setVecElt8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		ptrdiff2 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_setVecElt32:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_setVecElt16:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_setVecElt8:
			break;
		default:
			e = dl_error_invalidValue;
			break;
		}
		if (e) break;

		ptrdiff3 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_setVecElt32:
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			/* Fall through */
		case duckLisp_instruction_setVecElt16:
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			/* Fall through */
		case duckLisp_instruction_setVecElt8:
			break;
		default:
			e = dl_error_invalidValue;
			break;
		}
		if (e) break;

		ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
		if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object1, ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_vector) {
			e = dl_error_invalidValue;
			break;
		}

		ptrdiff2 = duckVM->stack.elements_length - ptrdiff2;
		if ((ptrdiff2 < 0) || ((dl_size_t) ptrdiff2 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object2, ptrdiff2);
		if (e) break;
		if (object2.type != duckVM_object_type_integer) {
			e = dl_error_invalidValue;
			break;
		}

		ptrdiff3 = duckVM->stack.elements_length - ptrdiff3;
		if ((ptrdiff3 < 0) || ((dl_size_t) ptrdiff3 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object3, ptrdiff3);
		if (e) break;

		/* Vector bounds check. */
		ptrdiff2 = object2.value.integer;
		if ((dl_size_t) (ptrdiff2 + object1.value.vector.offset)
		    >= object1.value.vector.internal_vector->value.internal_vector.length) {
			e = dl_error_invalidValue;
			break;
		}

		e = duckVM_gclist_pushObject(duckVM,
		                             &(object1.value.vector.internal_vector
		                               ->value.internal_vector.values[object1.value.vector.offset
		                                                              + ptrdiff2]),
		                             object3);
		if (e) break;
		/* Also push on stack. */
		e = stack_push(duckVM, &object3);
		if (e) break;
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_cdr32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_cdr16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_cdr8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		if (object1.type == duckVM_object_type_list) {
			if (object1.value.list == dl_null) {
				object2.type = duckVM_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				if (object1.value.list->type == duckVM_object_type_cons) {
					duckVM_object_t *cdr = object1.value.list->value.cons.cdr;
					if (cdr == dl_null) {
						object2.type = duckVM_object_type_list;
						object2.value.list = dl_null;
					}
					/* cdr can be a cons or something else. */
					else if (cdr->type == duckVM_object_type_cons) {
						object2.type = duckVM_object_type_list;
						object2.value.list = cdr;
					}
					else {
						object2 = *cdr;
					}
				}
				else {
					e = dl_error_shouldntHappen;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->cdr: Non-null list does not contain a cons."));
					if (!e) e = eError;
					break;
				}
			}
		}
		else if (object1.type == duckVM_object_type_vector) {
			if (object1.value.vector.internal_vector == dl_null) {
				object2.type = duckVM_object_type_vector;
				object2.value.vector.internal_vector = dl_null;
			}
			else {
				if (object1.value.vector.internal_vector->type == duckVM_object_type_internalVector) {
					object2.type = duckVM_object_type_vector;
					if ((object1.value.vector.internal_vector->value.internal_vector.length == 0)
					    /* -1 because we are going to try to get the cdr, which is the next element after. */
					    || ((dl_size_t) object1.value.vector.offset
					        >= object1.value.vector.internal_vector->value.internal_vector.length - 1)) {
						object2.value.vector.internal_vector = dl_null;
					}
					/* cdr can't be anything other than a vector. */
					else {
						object2.value.vector.internal_vector = object1.value.vector.internal_vector;
						object2.value.vector.offset = object1.value.vector.offset + 1;
					}
				}
				else {
					e = dl_error_invalidValue;
					break;
				}
			}
		}
		else if (object1.type == duckVM_object_type_string) {
			if (object1.value.string.internalString == dl_null) {
				object2.type = duckVM_object_type_string;
				object2.value.string.internalString = dl_null;
			}
			else {
				if (object1.value.string.internalString->type == duckVM_object_type_internalString) {
					object2.type = duckVM_object_type_string;
					if ((object1.value.string.length == 0)
					    /* -1 because we are going to try to get the cdr, which is the next element after. */
					    || ((dl_size_t) object1.value.string.offset >= object1.value.string.length - 1)) {
						object2.value.string.internalString = dl_null;
					}
					/* cdr can't be anything other than a string. */
					else {
						object2.value.string.internalString = object1.value.string.internalString;
						object2.value.string.offset = object1.value.string.offset + 1;
						object2.value.string.length = object1.value.string.length;
					}
				}
				else {
					e = dl_error_invalidValue;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->cdr: Internal string is wrong type."));
					if (eError) e = eError;
					break;
				}
			}
		}
		else {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->cdr: Argument is not a list, vector, or string."));
			if (eError) e = eError;
			break;
		}
		e = stack_push(duckVM, &object2);
		if (e) break;
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_car32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_car16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_car8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		if (object1.type == duckVM_object_type_list) {
			if (object1.value.list == dl_null) {
				object2.type = duckVM_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				if (object1.value.list->type == duckVM_object_type_cons) {
					duckVM_object_t *car = object1.value.list->value.cons.car;
					if (car == dl_null) {
						object2.type = duckVM_object_type_list;
						object2.value.list = dl_null;
					}
					/* car can be a cons or something else. */
					else if (car->type == duckVM_object_type_cons) {
						object2.type = duckVM_object_type_list;
						object2.value.list = car;
					}
					else {
						object2 = *car;
					}
				}
				else {
					e = dl_error_shouldntHappen;
					eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->cdr: Non-null list does not contain a cons."));
					if (!e) e = eError;
					break;
				}
			}
		}
		else if (object1.type == duckVM_object_type_vector) {
			if (object1.value.vector.internal_vector == dl_null) {
				object2.type = duckVM_object_type_vector;
				object2.value.vector.internal_vector = dl_null;
			}
			else {
				if (object1.value.vector.internal_vector->type == duckVM_object_type_internalVector) {
					if ((object1.value.vector.internal_vector->value.internal_vector.length == 0)
					    || ((dl_size_t) object1.value.vector.offset
					        >= object1.value.vector.internal_vector->value.internal_vector.length)) {
						object2.type = duckVM_object_type_vector;
						object2.value.vector.internal_vector = dl_null;
					}
					else {
						object2 = *(object1.value.vector.internal_vector
						            ->value.internal_vector.values[object1.value.vector.offset]);
					}
				}
				else {
					e = dl_error_invalidValue;
					break;
				}
			}
		}
		else if (object1.type == duckVM_object_type_string) {
			if (object1.value.string.internalString == dl_null) {
				object2.type = duckVM_object_type_string;
				object2.value.string.internalString = dl_null;
			}
			else {
				if (object1.value.string.internalString->type == duckVM_object_type_internalString) {
					if ((object1.value.string.length == 0)
					    || ((dl_size_t) object1.value.string.offset >= object1.value.string.length)) {
						object2.type = duckVM_object_type_string;
						object2.value.string.internalString = dl_null;
					}
					else {
						object2.type = duckVM_object_type_integer;
						object2.value.integer = (object1.value.string.internalString
						                         ->value.internalString.value[object1.value.string.offset]);
					}
				}
				else {
					e = dl_error_invalidValue;
					break;
				}
			}
		}
		else {
			e = dl_error_invalidValue;
			break;
		}
		e = stack_push(duckVM, &object2);
		if (e) break;
		break;

	case duckLisp_instruction_setCar32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		// Fall through
	case duckLisp_instruction_setCar16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		// Fall through
	case duckLisp_instruction_setCar8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;

		if ((object2.type == duckVM_object_type_list) && (object2.value.list != dl_null)) {
			if (object1.type == duckVM_object_type_list) {
				if (object1.value.list == dl_null) object2.value.list->value.cons.car = dl_null;
				else object2.value.list->value.cons.car = object1.value.list;
			}
			else {
				e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
				if (e) break;
				object2.value.list->value.cons.car = objectPtr1;
			}
		}
		else if ((object2.type == duckVM_object_type_vector)
		         && (object2.value.vector.internal_vector != dl_null)) {
			e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
			if (e) break;
			(object2.value.vector.internal_vector
			 ->value.internal_vector.values[object2.value.vector.offset]) = objectPtr1;
		}
		else {
			e = dl_error_invalidValue;
			break;
		}
		e = stack_push(duckVM, &object2);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_setCdr32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		// Fall through
	case duckLisp_instruction_setCdr16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		// Fall through
	case duckLisp_instruction_setCdr8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;

		if ((object2.type == duckVM_object_type_list) && (object2.value.list != dl_null)) {
			if (object1.type == duckVM_object_type_list) {
				if (object1.value.list == dl_null) object2.value.list->value.cons.cdr = dl_null;
				else object2.value.list->value.cons.cdr = object1.value.list;
			}
			else {
				e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
				if (e) break;
				object2.value.list->value.cons.cdr = objectPtr1;
			}
		}
		else if ((object2.type == duckVM_object_type_vector)
		         && (object2.value.vector.internal_vector != dl_null)
		         && (((object1.type == duckVM_object_type_vector)
		              && (object1.value.vector.internal_vector == dl_null))
		             || ((object1.type == duckVM_object_type_list)
		                 && (object1.value.list == dl_null)))) {
			object2.value.vector.internal_vector->value.internal_vector.length = object2.value.vector.offset;
		}
		else {
			e = dl_error_invalidValue;
			break;
		}
		e = stack_push(duckVM, &object2);
		break;

		// I probably don't need an `if` if I research the standard a bit.
	case duckLisp_instruction_nullp32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_nullp16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_nullp8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		object2.type = duckVM_object_type_bool;
		if (object1.type == duckVM_object_type_list) {
			object2.value.boolean = (object1.value.list == dl_null);
		}
		else if (object1.type == duckVM_object_type_vector) {
			object2.value.boolean = !((object1.value.vector.internal_vector != dl_null)
			                          && ((dl_size_t) object1.value.vector.offset
			                              < object1.value.vector.internal_vector->value.internal_vector.length));
		}
		else if (object1.type == duckVM_object_type_string) {
			object2.value.boolean = !((object1.value.string.internalString != dl_null)
			                          && ((dl_size_t) object1.value.string.offset < object1.value.string.length));
		}
		else {
			object2.value.boolean = dl_false;
		}
		e = stack_push(duckVM, &object2);
		if (e) break;
		break;

	case duckLisp_instruction_typeof32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_typeof16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_typeof8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		object2.type = duckVM_object_type_type;
		object2.value.type = ((object1.type == duckVM_object_type_composite)
		                      ? object1.value.composite->value.internalComposite.type
		                      : object1.type);
		e = stack_push(duckVM, &object2);
		if (e) break;
		break;

	case duckLisp_instruction_makeType:
		object1.type = duckVM_object_type_type;
		object1.value.type = duckVM->nextUserType++;
		e = stack_push(duckVM, &object1);
		if (e) break;
		break;

	case duckLisp_instruction_makeInstance32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_makeInstance16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_makeInstance8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		ptrdiff2 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_makeInstance32:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_makeInstance16:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_makeInstance8:
			break;
		default:
			e = dl_error_invalidValue;
			break;
		}
		if (e) break;

		ptrdiff3 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_makeInstance32:
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			/* Fall through */
		case duckLisp_instruction_makeInstance16:
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			/* Fall through */
		case duckLisp_instruction_makeInstance8:
			break;
		default:
			e = dl_error_invalidValue;
			break;
		}
		if (e) break;

		ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
		if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->make-instance: Index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object1, ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->make-instance: `type` argument must have type `type`."));
			if (!e) e = eError;
			break;
		}
		if (object1.value.type < duckVM_object_type_last) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->make-instance: Invalid instance type."));
			if (!e) e = eError;
			break;
		}

		ptrdiff2 = duckVM->stack.elements_length - ptrdiff2;
		if ((ptrdiff2 < 0) || ((dl_size_t) ptrdiff2 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->make-instance: Index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object2, ptrdiff2);
		if (e) break;

		ptrdiff3 = duckVM->stack.elements_length - ptrdiff3;
		if ((ptrdiff3 < 0) || ((dl_size_t) ptrdiff3 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->make-instance: Index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object3, ptrdiff3);
		if (e) break;

		{
			/* Allocate internal composite. */
			dl_size_t type = object1.value.type;
			object1.type = duckVM_object_type_internalComposite;
			/* Type. */
			object1.value.internalComposite.type = type;
			object1.value.internalComposite.value = dl_null;
			object1.value.internalComposite.function = dl_null;
			e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
			if (e) break;
			/* Link composite and internal composite to the stack. */
			object1.type = duckVM_object_type_composite;
			object1.value.composite = objectPtr1;
			e = stack_push(duckVM, &object1);
			if (e) break;
			/* Value. */
			e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object2);
			if (e) break;
			(DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckVM_object_t).value.composite
			 ->value.internalComposite.value) = objectPtr1;
			/* Function. */
			e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object3);
			if (e) break;
			(DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckVM_object_t).value.composite
			 ->value.internalComposite.function) = objectPtr1;
		}
		break;

	case duckLisp_instruction_compositeValue32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_compositeValue16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_compositeValue8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_composite) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->composite-value: Argument must be a composite type."));
			if (!e) e = eError;
			break;
		}
		e = stack_push(duckVM, object1.value.composite->value.internalComposite.value);
		if (e) break;
		break;

	case duckLisp_instruction_compositeFunction32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_compositeFunction16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_compositeFunction8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_composite) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->composite-function: Argument must be a composite type."));
			if (!e) e = eError;
			break;
		}
		e = stack_push(duckVM, object1.value.composite->value.internalComposite.function);
		if (e) break;
		break;

	case duckLisp_instruction_setCompositeValue32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_setCompositeValue16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_setCompositeValue8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;

		if (object1.type != duckVM_object_type_composite) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->set-composite-value: First argument must be a composite type."));
			if (!e) e = eError;
			break;
		}
		e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object2);
		if (e) break;
		object1.value.composite->value.internalComposite.value = objectPtr1;
		e = stack_push(duckVM, &object1);
		break;

	case duckLisp_instruction_setCompositeFunction32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff2 = *(ip++);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_setCompositeFunction16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_setCompositeFunction8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
		}
		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
		if (e) break;

		if (object1.type != duckVM_object_type_composite) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->set-composite-function: First argument must be a composite type."));
			if (!e) e = eError;
			break;
		}
		e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object2);
		if (e) break;
		object1.value.composite->value.internalComposite.function = objectPtr1;
		e = stack_push(duckVM, &object1);
		break;

	case duckLisp_instruction_length32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_length16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_length8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		{
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			object2.type = duckVM_object_type_integer;
			if (object1.type == duckVM_object_type_list) {
				if (duckVM_listIsCyclic(object1.value.list)) {
					e = dl_error_invalidValue;
					eError = duckVM_error_pushRuntime(duckVM,
					                                  DL_STR("duckVM_execute->length: List must not be circular."));
					if (!e) e = eError;
					break;
				}
				object2.value.integer = 0;
				{
					objectPtr1 = object1.value.list;
					while ((objectPtr1 != dl_null) && (objectPtr1->type == duckVM_object_type_cons)) {
						object2.value.integer++;
						objectPtr1 = objectPtr1->value.cons.cdr;
					}
				}
			}
			else if (object1.type == duckVM_object_type_vector) {
				object2.value.integer = (object1.value.vector.internal_vector
				                         ? (object1.value.vector.internal_vector->value.internal_vector.length
				                            - object1.value.vector.offset)
				                         : 0);
			}
			else if (object1.type == duckVM_object_type_string) {
				object2.value.integer = object1.value.string.length - object1.value.string.offset;
			}
			else {
				e = dl_error_invalidValue;
				(eError
				 = duckVM_error_pushRuntime(duckVM,
				                            DL_STR("duckVM_execute->length: Argument must be a list, vector, or string.")));
				if (eError) e = eError;
				break;
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
		}
		break;

	case duckLisp_instruction_symbolString32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_symbolString16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_symbolString8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_symbol) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->symbol-string: Argument must be a symbol."));
			if (eError) e = eError;
			break;
		}
		object2.type = duckVM_object_type_string;
		object2.value.string.internalString = object1.value.symbol.internalString;
		object2.value.string.offset = 0;
		object2.value.string.length = object1.value.symbol.internalString->value.internalString.value_length;
		e = stack_push(duckVM, &object2);
		if (e) break;
		break;

	case duckLisp_instruction_symbolId32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_symbolId16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_symbolId8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_symbol) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->symbol-id: Argument must be a symbol."));
			if (eError) e = eError;
			break;
		}
		object2.type = duckVM_object_type_integer;
		object2.value.integer = object1.value.symbol.id;
		e = stack_push(duckVM, &object2);
		if (e) break;
		break;

	case duckLisp_instruction_makeString32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_makeString16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through. */
	case duckLisp_instruction_makeString8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		{
			dl_uint8_t *string = dl_null;
			dl_size_t string_length = 0;
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type == duckVM_object_type_vector) {
				dl_size_t vector_length = object1.value.vector.internal_vector->value.internal_vector.length;
				string_length = vector_length - object1.value.vector.offset;
				e = DL_MALLOC(duckVM->memoryAllocation, &string, string_length, dl_uint8_t);
				if (e) break;
				for (dl_ptrdiff_t i = object1.value.vector.offset; (dl_size_t) i < vector_length; i++) {
					duckVM_object_t *element = object1.value.vector.internal_vector->value.internal_vector.values[i];
					if (element->type != duckVM_object_type_integer) {
						e = dl_error_invalidValue;
						eError = duckVM_error_pushRuntime(duckVM,
						                                  DL_STR("duckVM_execute->make-string: All elements of vector must be integers."));
						if (eError) e = eError;
						eError = DL_FREE(duckVM->memoryAllocation, &string);
						if (eError) e = eError;
						break;
					}
					/* Truncate to 8 bits. */
					string[i - object1.value.vector.offset] = element->value.integer;
				}
			}
			else if (object1.type == duckVM_object_type_list) {
				dl_array_t string_array;
				/**/ dl_array_init(&string_array, duckVM->memoryAllocation, sizeof(char), dl_array_strategy_double);
				objectPtr1 = object1.value.list;
				while (objectPtr1 != dl_null) {
					if (objectPtr1->type == duckVM_object_type_list) {
						objectPtr1 = objectPtr1->value.list;
					}
					else if (objectPtr1->type == duckVM_object_type_cons) {
						duckVM_object_t *car = objectPtr1->value.cons.car;
						if (car->type != duckVM_object_type_integer) {
							e = dl_error_invalidValue;
							eError = duckVM_error_pushRuntime(duckVM,
							                                  DL_STR("duckVM_execute->make-string: All list elements must be integers."));
							if (eError) e = eError;
							eError = dl_array_quit(&string_array);
							if (eError) e = eError;
							break;
						}
						uint8 = car->value.integer & 0xFF;
						e = dl_array_pushElement(&string_array, &uint8);
						if (e) {
							e = dl_error_invalidValue;
							eError = dl_array_quit(&string_array);
							if (eError) e = eError;
							break;
						}
						objectPtr1 = objectPtr1->value.cons.cdr;
					}
					else {
						e = dl_error_invalidValue;
						eError = duckVM_error_pushRuntime(duckVM,
						                                  DL_STR("duckVM_execute->make-string: Not a proper list."));
						if (eError) e = eError;
						eError = dl_array_quit(&string_array);
						if (eError) e = eError;
						break;
					}
				}
				string = string_array.elements;
				string_length = string_array.elements_length;
			}
			else {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_execute->make-string: Only vector arguments are supported."));
				if (eError) e = eError;
				eError = DL_FREE(duckVM->memoryAllocation, &string);
				if (eError) e = eError;
				break;
			}
			object1.type = duckVM_object_type_internalString;
			object1.value.internalString.value = string;
			object1.value.internalString.value_length = string_length;
			e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
			if (e) {
				eError = DL_FREE(duckVM->memoryAllocation, &string);
				if (eError) e = eError;
				break;
			}
			e = DL_FREE(duckVM->memoryAllocation, &string);
			if (e) break;
			object1.type = duckVM_object_type_string;
			object1.value.string.internalString = objectPtr1;
			object1.value.string.offset = 0;
			object1.value.string.length = string_length;
			e = stack_push(duckVM, &object1);
			if (e) break;
		}
		break;

	case duckLisp_instruction_concatenate32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_concatenate16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_concatenate8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		ptrdiff2 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_concatenate32:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_concatenate16:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_concatenate8:
			break;
		default:
			e = dl_error_cantHappen;
			break;
		}
		if (e) break;

		ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
		if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			(eError
			 = duckVM_error_pushRuntime(duckVM,
			                            DL_STR("duckVM_execute->concatenate: First string stack index out of bounds.")));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object1, ptrdiff1);
		if (e) break;
		if ((object1.type != duckVM_object_type_string) && (object1.type != duckVM_object_type_symbol)) {
			e = dl_error_invalidValue;
			(eError
			 = duckVM_error_pushRuntime(duckVM,
			                            DL_STR("duckVM_execute->concatenate: First argument must be a string or symbol.")));
			if (!e) e = eError;
			break;
		}

		ptrdiff2 = duckVM->stack.elements_length - ptrdiff2;
		if ((ptrdiff2 < 0) || ((dl_size_t) ptrdiff2 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			(eError
			 = duckVM_error_pushRuntime(duckVM,
			                            DL_STR("duckVM_execute->concatenate: Second string stack index out of bounds.")));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object2, ptrdiff2);
		if (e) break;
		if ((object2.type != duckVM_object_type_string) && (object2.type != duckVM_object_type_symbol)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->concatenate: Second argument must be a string or symbol."));
			if (!e) e = eError;
			break;
		}

		{
			dl_uint8_t *object1_string = dl_null;
			dl_size_t object1_string_length = 0;
			dl_uint8_t *object2_string = dl_null;
			dl_size_t object2_string_length = 0;
			if (object1.type == duckVM_object_type_string) {
				object1_string = (object1.value.string.internalString->value.internalString.value
				                  + object1.value.string.offset);
				object1_string_length = object1.value.string.length;
			}
			else {
				object1_string = object1.value.symbol.internalString->value.internalString.value;
				object1_string_length = object1.value.symbol.internalString->value.internalString.value_length;
			}
			if (object2.type == duckVM_object_type_string) {
				object2_string = (object2.value.string.internalString->value.internalString.value
				                  + object2.value.string.offset);
				object2_string_length = object2.value.string.length;
			}
			else {
				object2_string = object2.value.symbol.internalString->value.internalString.value;
				object2_string_length = object2.value.symbol.internalString->value.internalString.value_length;
			}
			object3.type = duckVM_object_type_internalString;
			object3.value.internalString.value = object1_string;
			object3.value.internalString.value_length = object1_string_length;
			e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object3);
			if (e) break;
			objectPtr1->value.internalString.value_length = object1_string_length + object2_string_length;
			e = DL_REALLOC(duckVM->memoryAllocation,
			               &objectPtr1->value.internalString.value,
			               objectPtr1->value.internalString.value_length,
			               dl_uint8_t);
			if (e) break;
			/**/ dl_memcopy_noOverlap((objectPtr1->value.internalString.value + object1_string_length),
			                          object2_string,
			                          object2_string_length);
			object3.type = duckVM_object_type_string;
			object3.value.string.offset = 0;
			object3.value.string.length = objectPtr1->value.internalString.value_length;
			object3.value.string.internalString = objectPtr1;
			e = stack_push(duckVM, &object3);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->concatenate: stack_push failed."));
				if (!e) e = eError;
				break;
			}
		}
		break;

	case duckLisp_instruction_substring32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_substring16:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		/* Fall through */
	case duckLisp_instruction_substring8:
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);

		ptrdiff2 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_substring32:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_substring16:
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			/* Fall through */
		case duckLisp_instruction_substring8:
			break;
		default:
			e = dl_error_cantHappen;
			break;
		}
		if (e) break;

		ptrdiff3 = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_substring32:
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			/* Fall through */
		case duckLisp_instruction_substring16:
			ptrdiff3 = *(ip++) + (ptrdiff3 << 8);
			/* Fall through */
		case duckLisp_instruction_substring8:
			break;
		default:
			e = dl_error_cantHappen;
			break;
		}
		if (e) break;

		ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
		if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: String stack index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object1, ptrdiff1);
		if (e) break;
		if (object1.type != duckVM_object_type_string) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: dl_array_get failed."));
			if (!e) e = eError;
			break;
		}

		ptrdiff2 = duckVM->stack.elements_length - ptrdiff2;
		if ((ptrdiff2 < 0) || ((dl_size_t) ptrdiff2 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: First integer stack index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object2, ptrdiff2);
		if (e) break;
		if (object2.type != duckVM_object_type_integer) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: dl_array_get failed."));
			if (!e) e = eError;
			break;
		}
		if ((object2.value.integer < 0)
		    || ((dl_size_t) object2.value.integer > object1.value.string.length - object1.value.string.offset)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: First integer out of bounds."));
			if (!e) e = eError;
			break;
		}

		ptrdiff3 = duckVM->stack.elements_length - ptrdiff3;
		if ((ptrdiff3 < 0) || ((dl_size_t) ptrdiff3 > duckVM->stack.elements_length)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: Second integer stack index out of bounds."));
			if (!e) e = eError;
			break;
		}
		e = dl_array_get(&duckVM->stack, &object3, ptrdiff3);
		if (e) break;
		if (object3.type != duckVM_object_type_integer) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->concatenate: dl_array_get failed."));
			if (!e) e = eError;
			break;
		}
		if ((object3.value.integer < 0)
		    || ((dl_size_t) object3.value.integer > object1.value.string.length - object1.value.string.offset)) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: Second integer out of bounds."));
			if (!e) e = eError;
			break;
		}

		if (object3.value.integer < object2.value.integer) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_execute->substring: Second integer must be greater than the first."));
			if (!e) e = eError;
			break;
		}

		object1.value.string.length = object1.value.string.offset + object3.value.integer;
		object1.value.string.offset = object1.value.string.offset + object2.value.integer;
		e = stack_push(duckVM, &object1);
		if (e) {
			eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute->concatenate: stack_push failed."));
			if (!e) e = eError;
			break;
		}
		break;

	case duckLisp_instruction_return32:
		ptrdiff1 = *(ip++);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
		parsedBytecode = dl_true;
		/* Fall through */
	case duckLisp_instruction_return16:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			parsedBytecode = dl_true;
		}
		/* Fall through */
	case duckLisp_instruction_return8:
		if (!parsedBytecode) {
			ptrdiff1 = *(ip++);
		}

		if (duckVM->stack.elements_length > 0) {
			e = dl_array_getTop(&duckVM->stack, &object1);
			if (e) break;
		}
		e = stack_pop_multiple(duckVM, ptrdiff1);
		if (e) break;
		if (duckVM->stack.elements_length > 0) {
			e = stack_pop_multiple(duckVM, 1);
			if (e) break;
			e = stack_push(duckVM, &object1);
			if (e) break;
		}
		e = call_stack_pop(duckVM, &ip, &bytecode);
		if (e == dl_error_bufferUnderflow) {
			*halt = duckVM_halt_mode_halt;
			e = dl_error_ok;
		}
		break;
	case duckLisp_instruction_return0:
		e = call_stack_pop(duckVM, &ip, &bytecode);
		if (e == dl_error_bufferUnderflow) {
			*halt = duckVM_halt_mode_halt;
			e = dl_error_ok;
		}
		break;

	case duckLisp_instruction_halt:
		*halt = duckVM_halt_mode_halt;
		break;

	case duckLisp_instruction_nil:
		object1.type = duckVM_object_type_list;
		object1.value.list = dl_null;
		e = stack_push(duckVM, &object1);
		if (e) break;
		break;

	default:
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_execute: Invalid opcode."));
		if (!e) e = eError;
		goto cleanup;
	}
 cleanup:
	*ipPtr = ip;
	return e;
}

dl_error_t duckVM_executeWithIp(duckVM_t *duckVM,
                                dl_uint8_t *bytecode,
                                dl_ptrdiff_t ipOffset,
                                dl_size_t bytecode_length) {
	dl_error_t e = dl_error_ok;

	duckVM_halt_mode_t halt = duckVM_halt_mode_run;
	/* Why a reference to bytecode? So it can be switched out with other bytecodes by `duckVM_executeInstruction`. */
	duckVM_object_t *bytecodeObject;
	dl_uint8_t *ip = dl_null;
	if ((ipOffset < 0) || (bytecode_length <= (dl_size_t) ipOffset)) {
		dl_error_t eError = dl_error_ok;
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM, DL_STR("duckVM_executeWithIp: IP out of bounds."));
		if (eError) e = eError;
		goto cleanup;
	}
	{
		duckVM_object_t temp;
		temp.type = duckVM_object_type_bytecode;
		temp.value.bytecode.bytecode = bytecode;
		temp.value.bytecode.bytecode_length = bytecode_length;
		e = duckVM_gclist_pushObject(duckVM, &bytecodeObject, temp);
		if (e) goto cleanup;
	}
	ip = &bytecodeObject->value.bytecode.bytecode[ipOffset];
	duckVM->currentBytecode = bytecodeObject;
	if (bytecode_length == 0) goto cleanup;
	do {
		e = duckVM_executeInstruction(duckVM, bytecodeObject, &ip, &halt);
	} while (!e && (halt == duckVM_halt_mode_run));
	duckVM->currentBytecode = dl_null;

 cleanup: return e;
}

dl_error_t duckVM_execute(duckVM_t *duckVM, dl_uint8_t *bytecode, dl_size_t bytecode_length) {
	return duckVM_executeWithIp(duckVM, bytecode, 0, bytecode_length);
}

dl_error_t duckVM_linkCFunction(duckVM_t *duckVM, dl_ptrdiff_t key, dl_error_t (*callback)(duckVM_t *)) {
	dl_error_t e = dl_error_ok;

	duckVM_object_t object;
	dl_memclear(&object, sizeof(duckVM_object_t));
	object.type = duckVM_object_type_function;
	object.value.function.callback = callback;

	{
		duckVM_object_t *objectPointer = dl_null;
		e = duckVM_gclist_pushObject(duckVM, &objectPointer, object);
		if (e) goto l_cleanup;
		e = duckVM_global_set(duckVM, objectPointer, key);
		if (e) goto l_cleanup;
	}

	l_cleanup:

	return e;
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
	if (e) goto cleanup;
	e = dl_array_popElements(&duckVM->upvalue_array_call_stack, dl_null, duckVM->call_stack.elements_length);
	if (e) goto cleanup;
	e = dl_array_popElements(&duckVM->call_stack, dl_null, duckVM->call_stack.elements_length);
	if (e) goto cleanup;
 cleanup:
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

duckVM_object_t duckVM_object_makeInternalString(dl_uint8_t *value, dl_size_t length) {
	duckVM_object_t o;
	o.type = duckVM_object_type_internalString;
	o.value.internalString.value = value;
	o.value.internalString.value_length = length;
	return o;
}

dl_error_t duckVM_object_makeString(duckVM_t *duckVM,
                                    duckVM_object_t *stringOut,
                                    dl_uint8_t *stringIn,
                                    dl_size_t stringIn_length) {
	duckVM_object_t internalString = duckVM_object_makeInternalString(stringIn, stringIn_length);
	duckVM_object_t *internalStringPointer = dl_null;
	dl_error_t e = duckVM_gclist_pushObject(duckVM, &internalStringPointer, internalString);
	if (e) goto cleanup;

	duckVM_object_t string;
	string.type = duckVM_object_type_string;
	string.value.string.internalString = internalStringPointer;
	string.value.string.offset = 0;
	string.value.string.length = internalString.value.internalString.value_length;
	*stringOut = string;

 cleanup: return e;
}

void duckVM_object_makeCompressedSymbol(duckVM_object_t *symbolOut, dl_size_t id) {
	duckVM_object_t symbol;
	symbol.type = duckVM_object_type_symbol;
	symbol.value.symbol.internalString = dl_null;
	symbol.value.symbol.id = id;
	*symbolOut = symbol;
}

dl_error_t duckVM_object_makeSymbol(duckVM_t *duckVM,
                                    duckVM_object_t *symbolOut,
                                    dl_size_t id,
                                    dl_uint8_t *string,
                                    dl_size_t string_length) {
	duckVM_object_t internalString = duckVM_object_makeInternalString(string, string_length);
	duckVM_object_t *internalStringPointer = dl_null;
	dl_error_t e = duckVM_gclist_pushObject(duckVM, &internalStringPointer, internalString);
	if (e) goto cleanup;

	duckVM_object_t symbol;
	symbol.type = duckVM_object_type_symbol;
	symbol.value.symbol.internalString = internalStringPointer;
	symbol.value.symbol.id = id;
	*symbolOut = symbol;

 cleanup: return e;
}

duckVM_object_t duckVM_object_makeFunction(dl_error_t (*callback)(duckVM_t *)) {
	duckVM_object_t o;
	o.type = duckVM_object_type_function;
	o.value.function.callback = callback;
	return o;
}

duckVM_object_t duckVM_object_makeClosure(dl_ptrdiff_t name,
                                          duckVM_object_t *bytecode,
                                          duckVM_object_t *upvalueArray,
                                          dl_uint8_t arity,
                                          dl_bool_t variadic) {
	duckVM_object_t o;
	o.type = duckVM_object_type_closure;
	o.value.closure.name = name;
	o.value.closure.bytecode = bytecode;
	o.value.closure.upvalue_array = upvalueArray;
	o.value.closure.arity = arity;
	o.value.closure.variadic = variadic;
	return o;
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

/* No `makeUpvalueObject` because C function calls can't handle unions well. */

duckVM_object_t duckVM_object_makeUpvalueArray(duckVM_object_t **upvalues, dl_size_t length) {
	duckVM_object_t o;
	o.type = duckVM_object_type_upvalueArray;
	o.value.upvalue_array.upvalues = upvalues;
	o.value.upvalue_array.length = length;
	return o;
}

duckVM_object_t duckVM_object_makeInternalVector(duckVM_object_t **values, dl_size_t length, dl_bool_t initialized) {
	duckVM_object_t o;
	o.type = duckVM_object_type_internalVector;
	o.value.internal_vector.values = values;
	o.value.internal_vector.length = length;
	o.value.internal_vector.initialized = initialized;
	return o;
}

dl_error_t duckVM_object_makeVector(duckVM_t *duckVM,
                                    duckVM_object_t *vectorOut,
                                    dl_array_t elements /* dl_array_t:duckVM_object_t * */) {
	dl_error_t e = dl_error_ok;

	duckVM_object_t internalVectorObject = duckVM_object_makeInternalVector(elements.elements,
	                                                                        elements.elements_length,
	                                                                        dl_true);
	duckVM_object_t *internalVectorObjectPointer = dl_null;
	e = duckVM_gclist_pushObject(duckVM, &internalVectorObjectPointer, internalVectorObject);
	if (e) goto cleanup;

	duckVM_object_t vectorObject;
	vectorObject.type = duckVM_object_type_vector;
	vectorObject.value.vector.internal_vector = internalVectorObjectPointer;
	vectorObject.value.vector.offset = 0;

	*vectorOut = vectorObject;

 cleanup: return e;
}

duckVM_object_t duckVM_object_makeBytecode(dl_uint8_t *bytecode, dl_size_t length) {
	duckVM_object_t o;
	o.type = duckVM_object_type_bytecode;
	o.value.bytecode.bytecode = bytecode;
	o.value.bytecode.bytecode_length = length;
	return o;
}

duckVM_object_t duckVM_object_makeInternalComposite(dl_size_t compositeType,
                                                    duckVM_object_t *value,
                                                    duckVM_object_t *function) {
	duckVM_object_t o;
	o.type = duckVM_object_type_internalComposite;
	o.value.internalComposite.type = compositeType;
	o.value.internalComposite.value = value;
	o.value.internalComposite.function = function;
	return o;
}

dl_error_t duckVM_object_makeComposite(duckVM_t *duckVM,
                                       duckVM_object_t *compositeOut,
                                       dl_size_t compositeType,
                                       duckVM_object_t *value,
                                       duckVM_object_t *function) {
	dl_error_t e = dl_error_ok;

	duckVM_object_t *internalCompositePointer = dl_null;
	{
		duckVM_object_t internalComposite = duckVM_object_makeInternalComposite(compositeType, value, function);
		e = duckVM_allocateHeapObject(duckVM, &internalCompositePointer, internalComposite);
		if (e) goto cleanup;
	}

	duckVM_object_t composite;
	composite.type = duckVM_object_type_composite;
	composite.value.composite = internalCompositePointer;
	*compositeOut = composite;

 cleanup: return e;
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

dl_error_t duckVM_object_getString(dl_memoryAllocation_t *memoryAllocation,
                                   dl_uint8_t **string,
                                   dl_size_t *length,
                                   duckVM_object_t object) {
	dl_error_t e = dl_error_ok;

	{
		dl_ptrdiff_t offset = object.value.string.offset;
		dl_size_t length = object.value.string.length;
		if (length) {
			e = DL_MALLOC(memoryAllocation, string, length - offset, dl_uint8_t);
			if (e) goto cleanup;

			duckVM_object_t *internalStringObject = object.value.string.internalString;
			if (internalStringObject == dl_null) {
				e = dl_error_nullPointer;
				goto cleanup;
			}

			duckVM_internalString_t internalString = internalStringObject->value.internalString;
			(void) dl_memcopy_noOverlap(*string, &internalString.value[offset], length - offset);
		}
		else {
			*string = dl_null;
		}
	}

	*length = object.value.string.length - object.value.string.offset;

 cleanup: return e;
}

duckVM_internalString_t duckVM_object_getInternalString(duckVM_object_t object) {
	return object.value.internalString;
}

duckVM_list_t duckVM_object_getList(duckVM_object_t object) {
	return object.value.list;
}

duckVM_cons_t duckVM_object_getCons(duckVM_object_t object) {
	return object.value.cons;
}

dl_error_t duckVM_object_getSymbol(dl_memoryAllocation_t *memoryAllocation,
                                   dl_size_t *id,
                                   dl_uint8_t **string,
                                   dl_size_t *length,
                                   duckVM_object_t object) {
	dl_error_t e = dl_error_ok;

	{
		duckVM_object_t *internalStringObject = object.value.symbol.internalString;
		if (internalStringObject == dl_null) {
			e = dl_error_nullPointer;
			goto cleanup;
		}
		duckVM_internalString_t internalString = internalStringObject->value.internalString;
		if (internalString.value_length) {
			e = DL_MALLOC(memoryAllocation, string, internalString.value_length, dl_uint8_t);
			if (e) goto cleanup;

			(void) dl_memcopy_noOverlap(*string, internalString.value, internalString.value_length);
		}
		else {
			*string = dl_null;
		}
		*length = internalString.value_length;
	}

	*id = object.value.symbol.id;

 cleanup: return e;
}


dl_error_t duckVM_string_getInternalString(duckVM_string_t string, duckVM_internalString_t *internalString) {
	duckVM_object_t *internalStringObject = string.internalString;
	if (internalStringObject == dl_null) {
		return dl_error_nullPointer;
	}
	*internalString = duckVM_object_getInternalString(*internalStringObject);
	return dl_error_ok;
}

dl_error_t duckVM_string_getElement(duckVM_string_t string, dl_uint8_t *byte, dl_ptrdiff_t index) {
	duckVM_internalString_t internalString;
	dl_error_t e = duckVM_string_getInternalString(string, &internalString);
	if (e) return e;
	*byte = internalString.value[index];
	return e;
}

dl_error_t duckVM_symbol_getInternalString(duckVM_symbol_t symbol, duckVM_internalString_t *internalString) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t *internalStringObject = symbol.internalString;
	if (internalStringObject == dl_null) {
		e = dl_error_nullPointer;
		return e;
	}
	*internalString = duckVM_object_getInternalString(*internalStringObject);
	return e;
}

dl_error_t duckVM_closure_getBytecode(duckVM_closure_t closure, duckVM_bytecode_t *bytecode) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t *bytecodeObject = closure.bytecode;
	if (bytecodeObject == dl_null) {
		return dl_error_nullPointer;
	}
	*bytecode = bytecodeObject->value.bytecode;
	return e;
}

dl_error_t duckVM_closure_getUpvalueArray(duckVM_closure_t closure, duckVM_upvalueArray_t *upvalueArray) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t *upvalueArrayObject = closure.upvalue_array;
	if (upvalueArrayObject == dl_null) {
		return dl_error_nullPointer;
	}
	*upvalueArray = upvalueArrayObject->value.upvalue_array;
	return e;
}

dl_error_t duckVM_closure_getUpvalue(duckVM_t *duckVM,
                                     duckVM_closure_t closure,
                                     duckVM_object_t *object,
                                     dl_ptrdiff_t index) {
	if (object == dl_null) return dl_error_nullPointer;

	duckVM_object_t *upvalueArrayObject = closure.upvalue_array;
	if (upvalueArrayObject == dl_null) {
		return dl_error_nullPointer;
	}
	if (upvalueArrayObject->type != duckVM_object_type_upvalueArray) {
		return dl_error_invalidValue;
	}
	duckVM_upvalueArray_t upvalueArray = upvalueArrayObject->value.upvalue_array;
	return duckVM_upvalueArray_getUpvalue(duckVM, upvalueArray, object, index);
}

dl_error_t duckVM_closure_setUpvalue(duckVM_t *duckVM,
                                     duckVM_closure_t closure,
                                     duckVM_object_t *object,
                                     dl_ptrdiff_t index) {
	if (object == dl_null) return dl_error_nullPointer;

	duckVM_object_t *upvalueArrayObject = closure.upvalue_array;
	if (upvalueArrayObject == dl_null) {
		return dl_error_nullPointer;
	}
	if (upvalueArrayObject->type != duckVM_object_type_upvalueArray) {
		return dl_error_invalidValue;
	}
	duckVM_upvalueArray_t upvalueArray = upvalueArrayObject->value.upvalue_array;
	return duckVM_upvalueArray_setUpvalue(duckVM, upvalueArray, object, index);
}

dl_error_t duckVM_list_getCons(duckVM_list_t list, duckVM_cons_t *cons) {
	duckVM_object_t *consObject = list;
	if (consObject == dl_null) {
		return dl_error_nullPointer;
	}
	*cons = duckVM_object_getCons(*consObject);
	return dl_error_ok;
}

dl_error_t duckVM_upvalueArray_getUpvalue(duckVM_t *duckVM,
                                          duckVM_upvalueArray_t upvalueArray,
                                          duckVM_object_t *object,
                                          dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	if (object == dl_null) return dl_error_nullPointer;

	duckVM_object_t *upvalueObject = upvalueArray.upvalues[index];

	/* Follow the chain of upvalues. */
	dl_bool_t found = dl_false;
	duckVM_upvalue_t upvalue;
	do {
		if (upvalueObject == dl_null) return dl_error_nullPointer;
		if (upvalueObject->type != duckVM_object_type_upvalue) return dl_error_invalidValue;
		upvalue = upvalueObject->value.upvalue;
		if (upvalue.type == duckVM_upvalue_type_heap_upvalue) {
			upvalueObject = upvalue.value.heap_upvalue;
		}
		else {
			found = dl_true;
		}
	} while (!found);

	/* Get the object. */
	if (upvalue.type == duckVM_upvalue_type_stack_index) {
		e = dl_array_get(&duckVM->stack, object, upvalue.value.stack_index);
		if (e) return e;
	}
	else if (upvalue.type == duckVM_upvalue_type_heap_object) {
		*object = *upvalue.value.heap_object;
	}
	return dl_error_ok;
}

dl_error_t duckVM_upvalueArray_setUpvalue(duckVM_t *duckVM,
                                          duckVM_upvalueArray_t upvalueArray,
                                          duckVM_object_t *object,
                                          dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	if (object == dl_null) return dl_error_nullPointer;

	duckVM_object_t *upvalueObject = upvalueArray.upvalues[index];

	/* Follow the chain of upvalues. */
	dl_bool_t found = dl_false;
	duckVM_upvalue_t upvalue;
	do {
		if (upvalueObject == dl_null) return dl_error_nullPointer;
		if (upvalueObject->type != duckVM_object_type_upvalue) return dl_error_invalidValue;
		upvalue = upvalueObject->value.upvalue;
		if (upvalue.type == duckVM_upvalue_type_heap_upvalue) {
			upvalueObject = upvalue.value.heap_upvalue;
		}
		else {
			found = dl_true;
		}
	} while (!found);

	/* Get the object. */
	if (upvalue.type == duckVM_upvalue_type_stack_index) {
		e = dl_array_set(&duckVM->stack, object, upvalue.value.stack_index);
		if (e) return e;
	}
	else if (upvalue.type == duckVM_upvalue_type_heap_object) {
		*upvalue.value.heap_object = *object;
	}
	return dl_error_ok;
}

dl_error_t duckVM_internalVector_getElement(duckVM_internalVector_t internalVector,
                                            duckVM_object_t **object,
                                            dl_ptrdiff_t index) {
	if ((dl_size_t) index >= internalVector.length) {
		return dl_error_invalidValue;
	}
	*object = internalVector.values[index];
	return dl_error_ok;
}

dl_error_t duckVM_vector_getInternalVector(duckVM_vector_t vector, duckVM_internalVector_t *internalVector) {
	duckVM_object_t *internalVectorObject = vector.internal_vector;
	if (internalVectorObject == dl_null) {
		return dl_error_nullPointer;
	}
	*internalVector = internalVectorObject->value.internal_vector;
	return dl_error_ok;
}

dl_error_t duckVM_vector_getLength(duckVM_vector_t vector, dl_size_t *length) {
	duckVM_internalVector_t internalVector;
	dl_error_t e = duckVM_vector_getInternalVector(vector, &internalVector);
	if (e) return e;
	*length = internalVector.length - vector.offset;
	return e;
}

dl_error_t duckVM_vector_getElement(duckVM_vector_t vector,
                                    duckVM_object_t **object,
                                    dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	dl_size_t length;
	e = duckVM_vector_getLength(vector, &length);
	if (e) return e;
	if ((dl_size_t) index >= length) {
		e = dl_error_invalidValue;
		return e;
	}
	dl_ptrdiff_t offset = vector.offset;
	duckVM_internalVector_t internalVector;
	e = duckVM_vector_getInternalVector(vector, &internalVector);
	if (e) return e;
	*object = internalVector.values[offset + index];

	return e;
}

dl_error_t duckVM_vector_setElement(duckVM_vector_t vector,
                                    duckVM_object_t *object,
                                    dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	dl_size_t length;
	e = duckVM_vector_getLength(vector, &length);
	if (e) return e;
	if ((dl_size_t) index >= length) {
		e = dl_error_invalidValue;
		return e;
	}
	dl_ptrdiff_t offset = vector.offset;
	duckVM_internalVector_t internalVector;
	e = duckVM_vector_getInternalVector(vector, &internalVector);
	if (e) return e;
	internalVector.values[offset + index] = object;

	return e;
}

dl_error_t duckVM_bytecode_getElement(duckVM_bytecode_t bytecode, dl_uint8_t *byte, dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;
	if ((dl_size_t) index >= bytecode.bytecode_length) {
		return dl_error_invalidValue;
	}
	*byte = bytecode.bytecode[index];
	return e;
}

dl_error_t duckVM_composite_getInternalComposite(duckVM_composite_t composite,
                                                 duckVM_internalComposite_t *internalComposite) {
	dl_error_t e = dl_error_ok;

	duckVM_object_t *internalCompositeObject = composite;
	if (internalCompositeObject == dl_null) {
		return dl_error_nullPointer;
	}
	*internalComposite = internalCompositeObject->value.internalComposite;

	return e;
}

dl_error_t duckVM_composite_getType(duckVM_composite_t composite, dl_size_t *type) {
	dl_error_t e = dl_error_ok;
	duckVM_internalComposite_t internalComposite;
	e = duckVM_composite_getInternalComposite(composite, &internalComposite);
	if (e) return e;
	*type = internalComposite.type;
	return e;
}

dl_error_t duckVM_composite_getValueObject(duckVM_composite_t composite, duckVM_object_t **value) {
	dl_error_t e = dl_error_ok;
	duckVM_internalComposite_t internalComposite;
	e = duckVM_composite_getInternalComposite(composite, &internalComposite);
	if (e) return e;
	*value = internalComposite.value;
	return e;
}

dl_error_t duckVM_composite_getFunctionObject(duckVM_composite_t composite, duckVM_object_t **function) {
	dl_error_t e = dl_error_ok;
	duckVM_internalComposite_t internalComposite;
	e = duckVM_composite_getInternalComposite(composite, &internalComposite);
	if (e) return e;
	*function = internalComposite.function;
	return e;
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

dl_error_t duckVM_isString(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_string);
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

dl_error_t duckVM_isVector(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_vector);
}

dl_error_t duckVM_isClosure(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_closure);
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

dl_error_t duckVM_isEmptyVector(duckVM_t *duckVM, dl_bool_t *result) {
	dl_error_t e = dl_error_ok;
	duckVM_object_t object;
	e = dl_array_getTop(&duckVM->stack, &object);
	if (e) return e;
	if (object.type == duckVM_object_type_vector) {
		duckVM_object_t *internal_vector_object = object.value.vector.internal_vector;
		if (internal_vector_object) {
			*result = (0 == internal_vector_object->value.internal_vector.length);
		}
		else {
			*result = dl_true;
		}
	}
	else {
		*result = dl_false;
	}
	return e;
}

dl_error_t duckVM_isUser(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_user);
}

dl_error_t duckVM_isUpvalue(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_upvalue);
}

dl_error_t duckVM_isUpvalueArray(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_upvalueArray);
}

dl_error_t duckVM_isInternalVector(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_internalVector);
}

dl_error_t duckVM_isBytecode(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_bytecode);
}

dl_error_t duckVM_isInternalComposite(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_internalComposite);
}

dl_error_t duckVM_isInternalString(duckVM_t *duckVM, dl_bool_t *result) {
	return templateForIsThing(duckVM, result, duckVM_object_type_internalString);
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
	case duckVM_object_type_vector: {
		duckVM_object_t *internal_vector_object = object.value.vector.internal_vector;
		if (internal_vector_object) {
			*result = (0 == internal_vector_object->value.internal_vector.length);
		}
		else {
			*result = dl_true;
		}
		break;
	}
	case duckVM_object_type_string:
		*result = (0 >= (object.value.string.length - object.value.string.offset));
		break;
	case duckVM_object_type_closure:{
		duckVM_object_t *upvalue_array = object.value.closure.upvalue_array;
		if (upvalue_array) {
			*result = (0 == upvalue_array->value.upvalue_array.length);
		}
		else {
			*result = dl_true;
		}
		break;
	}
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

/* Call the object at the given index as a function. */
dl_error_t duckVM_call(duckVM_t *duckVM, dl_ptrdiff_t stackIndex, dl_uint8_t numberOfArgs) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {

		duckVM_object_t functionObject;
		e = stack_get(duckVM, &functionObject, stackIndex);
		if (e) break;
		e = duckVM_instruction_prepareForFuncall(duckVM, &functionObject, numberOfArgs);
		if (e) break;
		if (functionObject.type == duckVM_object_type_function) {
			/* Call callback. */
			e = functionObject.value.function.callback(duckVM);
			if (e) {
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_instruction_funcall: C callback returned error."));
				if (!e) e = eError;
				break;
			}
			break;
		}
		else if (functionObject.type != duckVM_object_type_closure) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_instruction_funcall: Object is not a callback or closure."));
			if (!e) e = eError;
			break;
		}

		{
			/* Call bytecode. */
			duckVM_bytecode_t bytecode = functionObject.value.closure.bytecode->value.bytecode;
			dl_uint8_t bytecode_offset = functionObject.value.closure.name;
			/* stack: function *args */
			e = duckVM_executeWithIp(duckVM, bytecode.bytecode, bytecode_offset, bytecode.bytecode_length);
			if (e) break;
			/* stack: returnValue */
		}

	} while (0);
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

/* Copy a string off the top of the stack into the provided variable. The user must free the string using the VM's
   allocator. */
dl_error_t duckVM_copyString(duckVM_t *duckVM, dl_uint8_t **string, dl_size_t *string_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_string != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyString: Not a string."));
			if (eError) e = eError;
			break;
		}
		e = duckVM_object_getString(duckVM->memoryAllocation, string, string_length, object);
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
		e = duckVM_object_makeSymbol(duckVM, &object, id, name, name_length);
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
		duckVM_internalString_t internal_string;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_symbol != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copySymbolName: Not a symbol."));
			if (eError) e = eError;
			break;
		}
		if (object.value.symbol.internalString != dl_null) {
			e = duckVM_symbol_getInternalString(object.value.symbol, &internal_string);
			if (e) break;
			e = DL_MALLOC(duckVM->memoryAllocation, name, internal_string.value_length, dl_uint8_t);
			if (e) break;
			(void) dl_memcopy_noOverlap(*name, internal_string.value, internal_string.value_length);
			*name_length = internal_string.value_length;
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


/* Composites */

/* Push a composite value with the specified type onto the top of the stack. The value and function slots are set to
   nil. */
dl_error_t duckVM_pushComposite(duckVM_t *duckVM, dl_size_t type) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t nil;
		duckVM_object_t *nil_pointer;
		duckVM_object_t object;
		nil.type = duckVM_object_type_list;
		nil.value.list = dl_null;
		e = duckVM_allocateHeapObject(duckVM, &nil_pointer, nil);
		if (e) break;
		e = duckVM_object_makeComposite(duckVM, &object, type, nil_pointer, nil_pointer);
		if (e) break;
		e = duckVM_object_push(duckVM, &object);
		if (e) break;
	} while (0);
	return e;
}

/* Push the type slot of the composite on the top of the stack onto the top of the stack. */
dl_error_t duckVM_copyCompositeType(duckVM_t *duckVM, dl_size_t *type) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_composite != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyCompositeType: Not a composite."));
			if (eError) e = eError;
			break;
		}
		*type = object.value.composite->value.internalComposite.type;
	} while (0);
	return e;
}

/* Push the function slot of the composite on the top of the stack onto the top of the stack. */
dl_error_t duckVM_pushCompositeValue(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_composite != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_pushCompositeValue: Not a composite."));
			if (eError) e = eError;
			break;
		}
		e = duckVM_object_push(duckVM, object.value.composite->value.internalComposite.value);
		if (e) break;
	} while (0);
	return e;
}


/* Push the function slot of the composite on the top of the stack onto the top of the stack. */
dl_error_t duckVM_pushCompositeFunction(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_composite != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_pushCompositeFunction: Not a composite."));
			if (eError) e = eError;
			break;
		}
		e = duckVM_object_push(duckVM, object.value.composite->value.internalComposite.function);
		if (e) break;
	} while (0);
	return e;
}

/* Set the value slot of the composite at the specified index to the value at the top of the stack. */
dl_error_t duckVM_setCompositeValue(duckVM_t *duckVM, dl_ptrdiff_t stack_index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t composite;
		duckVM_object_t value;
		duckVM_object_t *heap_value = dl_null;
		e = dl_array_getTop(&duckVM->stack, &value);
		if (e) break;
		e = stack_get(duckVM, &composite, stack_index);
		if (e) break;
		if (duckVM_object_type_composite != composite.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setCompositeValue: Not a composite."));
			if (eError) e = eError;
			break;
		}
		e = duckVM_allocateHeapObject(duckVM, &heap_value, value);
		if (e) break;
		composite.value.composite->value.internalComposite.value = heap_value;
	} while (0);
	return e;
}

/* Set the function slot of the composite at the specified index to the value at the top of the stack. */
dl_error_t duckVM_setCompositeFunction(duckVM_t *duckVM, dl_ptrdiff_t stack_index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t composite;
		duckVM_object_t value;
		duckVM_object_t *heap_value = dl_null;
		e = dl_array_getTop(&duckVM->stack, &value);
		if (e) break;
		e = dl_array_get(&duckVM->stack, &composite, stack_index);
		if (e) break;
		if (duckVM_object_type_composite != composite.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setCompositeFunction: Not a composite."));
			if (eError) e = eError;
			break;
		}
		e = duckVM_allocateHeapObject(duckVM, &heap_value, value);
		if (e) break;
		composite.value.composite->value.internalComposite.function = heap_value;
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


/* Vectors -- See sequence operations below that operate on these objects. */

/* Push a vector with the specified length onto the stack with each element to nil. */
dl_error_t duckVM_pushVector(duckVM_t *duckVM, dl_size_t length) {
	dl_error_t e = dl_error_ok;
	do {
		duckVM_object_t vector;
		dl_array_t array;
		duckVM_object_t *nil_pointer = dl_null;
		(void) dl_array_init(&array, duckVM->memoryAllocation, sizeof(duckVM_object_t *), dl_array_strategy_fit);
		do {
			DL_DOTIMES(k, length) {
				e = dl_array_pushElement(&array, &nil_pointer);
				if (e) break;
			}
			if (e) break;
			e = duckVM_allocateHeapObject(duckVM, &nil_pointer, duckVM_object_makeList(dl_null));
			if (e) break;
			e = duckVM_object_makeVector(duckVM, &vector, array);
			if (e) break;
			e = stack_push(duckVM, &vector);
			if (e) break;
		} while (0);
		e = dl_array_quit(&array);
		if (e) break;
	} while (0);
	return e;
}


/* /\* Closures -- See sequence operations below that operate on these objects. *\/ */

/* Copy the "name" of the closure into the provided variable. Note that different closures may share the same "name". */
dl_error_t duckVM_copyClosureName(duckVM_t *duckVM, dl_ptrdiff_t *name) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_closure != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyClosureName: Not a closure."));
			if (eError) e = eError;
			break;
		}
		*name = object.value.closure.name;
	} while (0);
	return e;
}

/* Push the closure's bytecode object on the top of the stack. */
dl_error_t duckVM_pushClosureBytecode(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_closure != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_pushClosureBytecode: Not a closure."));
			if (eError) e = eError;
			break;
		}
		e = stack_push(duckVM, object.value.closure.bytecode);
		if (e) break;
	} while (0);
	return e;
}

/* Copy the arity of the closure into the provided variable. */
dl_error_t duckVM_copyClosureArity(duckVM_t *duckVM, dl_uint8_t *arity) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_closure != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyClosureArity: Not a closure."));
			if (eError) e = eError;
			break;
		}
		*arity = object.value.closure.arity;
	} while (0);
	return e;
}

/* Return whether or not the closure is variadic in the provided variable. */
dl_error_t duckVM_copyClosureIsVariadic(duckVM_t *duckVM, dl_bool_t *is_variadic) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	do {
		duckVM_object_t object;
		e = dl_array_getTop(&duckVM->stack, &object);
		if (e) break;
		if (duckVM_object_type_closure != object.type) {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_copyClosureName: Not a closure."));
			if (eError) e = eError;
			break;
		}
		*is_variadic = object.value.closure.variadic;
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
		case duckVM_object_type_vector: {
			duckVM_vector_t vector = sequence.value.vector;
			duckVM_object_t *firstPointer;
			e = duckVM_vector_getElement(vector, &firstPointer, 0);
			if (e) break;
			first = *firstPointer;
			break;
		}
		case duckVM_object_type_string: {
			duckVM_string_t string = sequence.value.string;
			dl_uint8_t byte;
			e = duckVM_string_getElement(string, &byte, 0);
			if (e) break;
			first = duckVM_object_makeInteger(byte);
			break;
		}
		case duckVM_object_type_closure: {
			duckVM_closure_t closure = sequence.value.closure;
			e = duckVM_closure_getUpvalue(duckVM, closure, &first, 0);
			if (e) break;
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
		case duckVM_object_type_vector: {
			duckVM_vector_t vector = sequence.value.vector;
			dl_ptrdiff_t offset = vector.offset;
			rest = sequence;
			offset++;
			if ((dl_size_t) offset >= vector.internal_vector->value.internal_vector.length) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                             DL_STR("duckVM_pushRest: Vector is empty."));
				if (eError) e = eError;
				break;
			}
			rest.value.vector.offset = offset;
			break;
		}
		case duckVM_object_type_string: {
			duckVM_string_t string = sequence.value.string;
			dl_ptrdiff_t offset = string.offset;
			rest = sequence;
			offset++;
			if ((dl_size_t) offset >= string.length) {
				e = dl_error_invalidValue;
				eError = duckVM_error_pushRuntime(duckVM,
				                                  DL_STR("duckVM_pushRest: String is empty."));
				if (eError) e = eError;
				break;
			}
			rest.value.string.offset = offset;
			break;
		}
		case duckVM_object_type_closure: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_pushRest: Closures are not supported."));
			if (eError) e = eError;
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

/* Set the first element of the sequence at the specified stack index to the value of the object at the top of the
   stack.
   This operation fails on nil, strings, and closures. */
dl_error_t duckVM_setFirst(duckVM_t *duckVM, dl_ptrdiff_t stack_index) {
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
				                                  DL_STR("duckVM_setFirst: List is nil."));
				if (eError) e = eError;
				break;
			}
			break;
		}
		case duckVM_object_type_vector: {
			duckVM_vector_t vector = sequence.value.vector;
			e = duckVM_vector_setElement(vector, value_pointer, 0);
			if (e) break;
			break;
		}
		case duckVM_object_type_string: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setFirst: Strings are not supported."));
			if (eError) e = eError;
			break;
		}
		case duckVM_object_type_closure: {
			duckVM_closure_t closure = sequence.value.closure;
			e = duckVM_closure_setUpvalue(duckVM, closure, value_pointer, 0);
			if (e) break;
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setFirst: Unsupported object type."));
			if (eError) e = eError;
			break;
		}
		}
		if (e) break;
	} while (0);
	return e;
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
		case duckVM_object_type_vector: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setRest: Vectors are not supported."));
			if (eError) e = eError;
			break;
		}
		case duckVM_object_type_string: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setRest: Strings are not supported."));
			if (eError) e = eError;
			break;
		}
		case duckVM_object_type_closure: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setRest: Closures are not supported."));
			if (eError) e = eError;
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
		case duckVM_object_type_vector: {
			duckVM_vector_t vector = sequence.value.vector;
			duckVM_object_t *element_pointer;
			e = duckVM_vector_getElement(vector, &element_pointer, sequence_index);
			if (e) break;
			element = *element_pointer;
			break;
		}
		case duckVM_object_type_string: {
			duckVM_string_t string = sequence.value.string;
			dl_uint8_t byte;
			e = duckVM_string_getElement(string, &byte, sequence_index);
			if (e) break;
			element = duckVM_object_makeInteger(byte);
			break;
		}
		case duckVM_object_type_closure: {
			duckVM_closure_t closure = sequence.value.closure;
			e = duckVM_closure_getUpvalue(duckVM, closure, &element, sequence_index);
			if (e) break;
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
		case duckVM_object_type_vector: {
			duckVM_vector_t vector = sequence.value.vector;
			e = duckVM_vector_setElement(vector, value_pointer, sequence_index);
			if (e) break;
			break;
		}
		case duckVM_object_type_string: {
			e = dl_error_invalidValue;
			eError = duckVM_error_pushRuntime(duckVM,
			                                  DL_STR("duckVM_setElement: Strings are not supported."));
			if (eError) e = eError;
			break;
		}
		case duckVM_object_type_closure: {
			duckVM_closure_t closure = sequence.value.closure;
			e = duckVM_closure_setUpvalue(duckVM, closure, value_pointer, sequence_index);
			if (e) break;
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
		case duckVM_object_type_vector: {
			duckVM_vector_t vector = sequence.value.vector;
			duckVM_object_t *internal_vector = vector.internal_vector;
			local_length = (internal_vector
			                ? (internal_vector->value.internal_vector.length - vector.offset)
			                : 0);
			break;
		}
		case duckVM_object_type_string: {
			duckVM_string_t string = sequence.value.string;
			local_length = string.length - string.offset;
			break;
		}
		case duckVM_object_type_closure: {
			duckVM_upvalueArray_t upvalue_array;
			e = duckVM_closure_getUpvalueArray(sequence.value.closure, &upvalue_array);
			if (e) break;
			local_length = upvalue_array.length;
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

dl_error_t duckVM_upvalue_type_prettyPrint(dl_array_t *string_array, duckVM_upvalue_type_t type) {
	switch (type) {
	case duckVM_upvalue_type_stack_index:
		return dl_array_pushElements(string_array, DL_STR("duckVM_upvalue_type_stack_index"));
	case duckVM_upvalue_type_heap_object:
		return dl_array_pushElements(string_array, DL_STR("duckVM_upvalue_type_heap_object"));
	case duckVM_upvalue_type_heap_upvalue:
		return dl_array_pushElements(string_array, DL_STR("duckVM_upvalue_type_heap_upvalue"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}

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

dl_error_t duckVM_callFrame_prettyPrint(dl_array_t *string_array, duckVM_callFrame_t callFrame) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_callFrame_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_uint8_t *ip = "));
	if (e) goto cleanup;
	if (callFrame.bytecode == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
	}
	else {
		e = dl_string_fromPtrdiff(string_array, callFrame.ip - callFrame.bytecode->value.bytecode.bytecode);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR(" + bytecode"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("bytecode = "));
	if (e) goto cleanup;
	if (callFrame.bytecode == dl_null) {
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

	e = dl_array_pushElements(string_array, DL_STR("call_stack = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckVM.call_stack.elements_length) {
		e = duckVM_callFrame_prettyPrint(string_array, DL_ARRAY_GETADDRESS(duckVM.call_stack, duckVM_callFrame_t, i));
		if (e) goto cleanup;
		if ((dl_size_t) i != duckVM.call_stack.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("currentBytecode = "));
	if (e) goto cleanup;
	if (duckVM.currentBytecode == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("upvalue_stack = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckVM.upvalue_stack.elements_length) {
		e = duckVM_upvalue_prettyPrint(string_array,
		                               DL_ARRAY_GETADDRESS(duckVM.upvalue_stack, duckVM_upvalue_t, i),
		                               duckVM);
		if (e) goto cleanup;
		if ((dl_size_t) i != duckVM.upvalue_stack.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("upvalue_array_call_stack = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckVM.upvalue_array_call_stack.elements_length) {
		e = duckVM_upvalueArray_prettyPrint(string_array,
		                                    DL_ARRAY_GETADDRESS(duckVM.upvalue_array_call_stack,
		                                                        duckVM_upvalueArray_t,
		                                                        i),
		                                    duckVM);
		if (e) goto cleanup;
		if ((dl_size_t) i != duckVM.upvalue_array_call_stack.elements_length - 1) {
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

	e = dl_array_pushElements(string_array, DL_STR("duckLisp = "));
	if (e) goto cleanup;
	/* *Don't* print *.duckLisp so we don't recurse infinitely. */
	if (duckVM.duckLisp == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("..."));
	}
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

dl_error_t duckVM_internalString_prettyPrint(dl_array_t *string_array, duckVM_internalString_t internalString) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_internalString_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("value["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, internalString.value_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = \""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, internalString.value, internalString.value_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_string_prettyPrint(dl_array_t *string_array, duckVM_string_t string) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_string_t) {"));
	if (e) goto cleanup;

	if (string.internalString == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("\""));
		if (e) goto cleanup;
		e = duckVM_internalString_prettyPrint(string_array, string.internalString->value.internalString);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("\"["));
		if (e) goto cleanup;
		e = dl_string_fromPtrdiff(string_array, string.offset);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR(":"));
		if (e) goto cleanup;
		e = dl_string_fromSize(string_array, string.length);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("]"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_symbol_prettyPrint(dl_array_t *string_array, duckVM_symbol_t symbol) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_symbol_t) {"));
	if (e) goto cleanup;

	if (symbol.internalString == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("\""));
		if (e) goto cleanup;
		e = duckVM_internalString_prettyPrint(string_array, symbol.internalString->value.internalString);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("\""));
		if (e) goto cleanup;
	}

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

dl_error_t duckVM_function_prettyPrint(dl_array_t *string_array, duckVM_function_t function) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_function_t) {"));
	if (e) goto cleanup;

	if (function.callback == dl_null) {
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

dl_error_t duckVM_closure_prettyPrint(dl_array_t *string_array, duckVM_closure_t closure, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_closure_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("name = "));
	if (e) goto cleanup;
	e = dl_string_fromPtrdiff(string_array, closure.name);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	if (closure.bytecode == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("bytecode = NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("bytecode["));
		if (e) goto cleanup;
		e = dl_string_fromSize(string_array, closure.bytecode->value.bytecode.bytecode_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("] = "));
		if (e) goto cleanup;
		if (closure.bytecode->value.bytecode.bytecode == dl_null) {
			e = dl_array_pushElements(string_array, DL_STR("NULL"));
			if (e) goto cleanup;
		}
		else {
			e = dl_array_pushElements(string_array, DL_STR("..."));
			if (e) goto cleanup;
		}
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	if (closure.upvalue_array == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("upvalue_array = NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("upvalue_array["));
		if (e) goto cleanup;
		e = dl_string_fromSize(string_array, closure.upvalue_array->value.upvalue_array.length);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("] = "));
		if (e) goto cleanup;
		if (closure.upvalue_array->value.upvalue_array.upvalues == dl_null) {
			e = dl_array_pushElements(string_array, DL_STR("NULL"));
			if (e) goto cleanup;
		}
		else {
			DL_DOTIMES(k, closure.upvalue_array->value.upvalue_array.length) {
				e = duckVM_object_prettyPrint(string_array,
				                              *closure.upvalue_array->value.upvalue_array.upvalues[k],
				                              duckVM);
				if (e) goto cleanup;
				if ((dl_size_t) k != closure.upvalue_array->value.upvalue_array.length - 1) {
					e = dl_array_pushElements(string_array, DL_STR(", "));
					if (e) goto cleanup;
				}
			}
		}
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("arity = "));
	if (e) goto cleanup;
	e = dl_string_fromUint8(string_array, closure.arity);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("variadic = "));
	if (e) goto cleanup;
	e = dl_string_fromBool(string_array, closure.variadic);
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

dl_error_t duckVM_upvalue_prettyPrint(dl_array_t *string_array, duckVM_upvalue_t upvalue, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;
	(void) duckVM;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_upvalue_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("value = "));
	if (e) goto cleanup;
	{
		while (upvalue.type == duckVM_upvalue_type_heap_upvalue) {
			e = dl_array_pushElements(string_array, DL_STR("-> "));
			if (e) goto cleanup;
			if (upvalue.value.heap_upvalue == dl_null) {
				break;
			}
			upvalue = upvalue.value.heap_upvalue->value.upvalue;
		}
	}
	switch (upvalue.type) {
	case duckVM_upvalue_type_stack_index:
		e = duckVM_object_prettyPrint(string_array,
		                              DL_ARRAY_GETADDRESS(duckVM.stack, duckVM_object_t, upvalue.value.stack_index),
		                              duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_upvalue_type_heap_object:
		e = duckVM_object_prettyPrint(string_array, *upvalue.value.heap_object, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_upvalue_type_heap_upvalue:
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
		break;
	default:
		e = dl_array_pushElements(string_array, DL_STR("INVALID"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("type = "));
	if (e) goto cleanup;
	e = duckVM_upvalue_type_prettyPrint(string_array, upvalue.type);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_upvalueArray_prettyPrint(dl_array_t *string_array,
                                           duckVM_upvalueArray_t upvalueArray,
                                           duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_upvalueArray_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("upvalues["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, upvalueArray.length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, upvalueArray.length) {
		e = duckVM_object_prettyPrint(string_array, *upvalueArray.upvalues[i], duckVM);
		if (e) goto cleanup;
		if ((dl_size_t) i != upvalueArray.length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_internalVector_prettyPrint(dl_array_t *string_array,
                                             duckVM_internalVector_t internalVector,
                                             duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_internalVector_t) {"));
	if (e) goto cleanup;

	if (internalVector.initialized) {
		e = dl_array_pushElements(string_array, DL_STR("values["));
		if (e) goto cleanup;
		e = dl_string_fromSize(string_array, internalVector.length);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("] = {"));
		if (e) goto cleanup;
		DL_DOTIMES(i, internalVector.length) {
			e = duckVM_object_prettyPrint(string_array, *internalVector.values[i], duckVM);
			if (e) goto cleanup;
			if ((dl_size_t) i != internalVector.length - 1) {
				e = dl_array_pushElements(string_array, DL_STR(", "));
				if (e) goto cleanup;
			}
		}
		e = dl_array_pushElements(string_array, DL_STR("}"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("values = UNINITIALIZED"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_vector_prettyPrint(dl_array_t *string_array, duckVM_vector_t vector, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_vector_t) {"));
	if (e) goto cleanup;

	if (vector.internal_vector == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = duckVM_internalVector_prettyPrint(string_array, vector.internal_vector->value.internal_vector, duckVM);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("["));
		if (e) goto cleanup;
		e = dl_string_fromPtrdiff(string_array, vector.offset);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR(":]"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_bytecode_prettyPrint(dl_array_t *string_array, duckVM_bytecode_t bytecode) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_bytecode_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("bytecode["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, bytecode.bytecode_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {"));
	if (e) goto cleanup;
	if (bytecode.bytecode == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_internalComposite_prettyPrint(dl_array_t *string_array,
                                                duckVM_internalComposite_t internalComposite,
                                                duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_internalComposite_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("type = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, internalComposite.type);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("value = "));
	if (e) goto cleanup;
	if (internalComposite.value == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = duckVM_object_prettyPrint(string_array, *internalComposite.value, duckVM);
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("function = "));
	if (e) goto cleanup;
	if (internalComposite.function == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = duckVM_object_prettyPrint(string_array, *internalComposite.function, duckVM);
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckVM_composite_prettyPrint(dl_array_t *string_array, duckVM_composite_t composite, duckVM_t duckVM) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckVM_composite_t) {"));
	if (e) goto cleanup;

	if (composite == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = duckVM_internalComposite_prettyPrint(string_array, composite->value.internalComposite, duckVM);
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
	case duckVM_object_type_string:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_string"));
	case duckVM_object_type_list:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_list"));
	case duckVM_object_type_symbol:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_symbol"));
	case duckVM_object_type_function:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_function"));
	case duckVM_object_type_closure:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_closure"));
	case duckVM_object_type_vector:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_vector"));
	case duckVM_object_type_type:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_type"));
	case duckVM_object_type_composite:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_composite"));
	case duckVM_object_type_user:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_user"));
	case duckVM_object_type_cons:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_cons"));
	case duckVM_object_type_upvalue:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_upvalue"));
	case duckVM_object_type_upvalueArray:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_upvalueArray"));
	case duckVM_object_type_internalVector:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_internalVector"));
	case duckVM_object_type_bytecode:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_bytecode"));
	case duckVM_object_type_internalComposite:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_internalComposite"));
	case duckVM_object_type_internalString:
		return dl_array_pushElements(string_array, DL_STR("duckVM_object_type_internalString"));
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
	case duckVM_object_type_internalString:
		e = duckVM_internalString_prettyPrint(string_array, object.value.internalString);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_string:
		e = duckVM_string_prettyPrint(string_array, object.value.string);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_symbol:
		e = duckVM_symbol_prettyPrint(string_array, object.value.symbol);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_function:
		e = duckVM_function_prettyPrint(string_array, object.value.function);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_closure:
		e = duckVM_closure_prettyPrint(string_array, object.value.closure, duckVM);
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
	case duckVM_object_type_upvalue:
		e = duckVM_upvalue_prettyPrint(string_array, object.value.upvalue, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_upvalueArray:
		e = duckVM_upvalueArray_prettyPrint(string_array, object.value.upvalue_array, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_internalVector:
		e = duckVM_internalVector_prettyPrint(string_array, object.value.internal_vector, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_vector:
		e = duckVM_vector_prettyPrint(string_array, object.value.vector, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_bytecode:
		e = duckVM_bytecode_prettyPrint(string_array, object.value.bytecode);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_type:
		e = dl_array_pushElements(string_array, DL_STR("type: (dl_size_t) "));
		if (e) goto cleanup;
		e = dl_string_fromSize(string_array, object.value.type);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_internalComposite:
		e = duckVM_internalComposite_prettyPrint(string_array, object.value.internalComposite, duckVM);
		if (e) goto cleanup;
		break;
	case duckVM_object_type_composite:
		e = duckVM_composite_prettyPrint(string_array, object.value.composite, duckVM);
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
