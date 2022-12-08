#include "duckVM.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "duckLisp.h"
#include <stdio.h>

dl_error_t duckVM_gclist_init(duckVM_gclist_t *gclist,
                              dl_memoryAllocation_t *memoryAllocation,
                              const dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	gclist->memoryAllocation = memoryAllocation;


	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->objects,
				  maxObjects * sizeof(duckLisp_object_t));
	if (e) goto cleanup;
	gclist->objects_length = maxObjects;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->freeObjects,
				  maxObjects * sizeof(duckLisp_object_t *));
	if (e) goto cleanup;
	gclist->freeObjects_length = maxObjects;


	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->objectInUse,
				  maxObjects * sizeof(dl_bool_t));
	if (e) goto cleanup;


	/**/ dl_memclear(gclist->objects, gclist->objects_length * sizeof(duckLisp_object_t));


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

static dl_error_t duckVM_gclist_markObject(duckVM_gclist_t *gclist, duckLisp_object_t *object) {
	dl_error_t e = dl_error_ok;
	if (object && !gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)]) {
		gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)] = dl_true;
		if (object->type == duckLisp_object_type_list) {
			e = duckVM_gclist_markObject(gclist, object->value.list);
			if (e) goto cleanup;
		}
		else if (object->type == duckLisp_object_type_cons) {
			e = duckVM_gclist_markObject(gclist, object->value.cons.car);
			if (e) goto cleanup;
			e = duckVM_gclist_markObject(gclist, object->value.cons.cdr);
			if (e) goto cleanup;
		}
		else if (object->type == duckLisp_object_type_closure) {
			e = duckVM_gclist_markObject(gclist, object->value.closure.upvalue_array);
			if (e) goto cleanup;
		}
		else if (object->type == duckLisp_object_type_upvalue) {
			if (object->value.upvalue.type == duckVM_upvalue_type_heap_object) {
				e = duckVM_gclist_markObject(gclist, object->value.upvalue.value.heap_object);
				if (e) goto cleanup;
			}
			else if (object->value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
				e = duckVM_gclist_markObject(gclist, object->value.upvalue.value.heap_upvalue);
				if (e) goto cleanup;
			}
		}
		else if (object->type == duckLisp_object_type_upvalueArray) {
			if (object->value.upvalue_array.initialized) {
				DL_DOTIMES(k, object->value.upvalue_array.length) {
					e = duckVM_gclist_markObject(gclist, object->value.upvalue_array.upvalues[k]);
					if (e) goto cleanup;
				}
			}
		}
		else if (object->type == duckLisp_object_type_vector) {
			e = duckVM_gclist_markObject(gclist, object->value.vector.internal_vector);
			if (e) goto cleanup;
		}
		else if (object->type == duckLisp_object_type_internalVector) {
			if (object->value.internal_vector.initialized) {
				DL_DOTIMES(k, object->value.internal_vector.length) {
					e = duckVM_gclist_markObject(gclist, object->value.internal_vector.values[k]);
					if (e) goto cleanup;
				}
			}
		}
		/* else ignore, since the stack is the root of GC. Would cause a cycle (infinite loop) if we handled it. */
	}
 cleanup:
	return e;
}

static dl_error_t duckVM_gclist_garbageCollect(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	/* Clear the in use flags. */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.objects_length; i++) {
		duckVM->gclist.objectInUse[i] = dl_false;
	}

	/* Mark the cells in use. */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->stack.elements_length; i++) {
		duckLisp_object_t *object = &DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, i);
		// Will need to check for closures now too. Can probably call markObject here instead of using this `if`.
		if (object->type == duckLisp_object_type_list) {
			e = duckVM_gclist_markObject(&duckVM->gclist, object->value.list);
			if (e) goto cleanup;
		}
		else if (object->type == duckLisp_object_type_closure) {
			e = duckVM_gclist_markObject(&duckVM->gclist, object->value.closure.upvalue_array);
			if (e) break;
		}
		else if (object->type == duckLisp_object_type_vector) {
			e = duckVM_gclist_markObject(&duckVM->gclist, object->value.vector.internal_vector);
			if (e) break;
		}
		/* Don't check for conses, upvalues, or upvalue arrays since they should never be on the stack. */
	}

	/* Free cells if not marked. */

	duckVM->gclist.freeObjects_length = 0;  // This feels horribly inefficient.
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.objects_length; i++) {
		if (!duckVM->gclist.objectInUse[i]) {
			duckVM->gclist.freeObjects[duckVM->gclist.freeObjects_length++] = &duckVM->gclist.objects[i];
			if ((duckVM->gclist.objects[i].type == duckLisp_object_type_upvalueArray)
			    && duckVM->gclist.objects[i].value.upvalue_array.initialized
			    && (duckVM->gclist.objects[i].value.upvalue_array.upvalues != dl_null)) {
				e = DL_FREE(duckVM->memoryAllocation, &duckVM->gclist.objects[i].value.upvalue_array.upvalues);
				if (e) goto cleanup;
			}
			else if ((duckVM->gclist.objects[i].type == duckLisp_object_type_internalVector)
			    && duckVM->gclist.objects[i].value.internal_vector.initialized
			    && (duckVM->gclist.objects[i].value.internal_vector.values != dl_null)) {
				e = DL_FREE(duckVM->memoryAllocation, &duckVM->gclist.objects[i].value.internal_vector.values);
				if (e) goto cleanup;
			}
		}
	}

 cleanup:
	return e;
}

dl_error_t duckVM_gclist_pushObject(duckVM_t *duckVM, duckLisp_object_t **objectOut, duckLisp_object_t objectIn) {
	dl_error_t e = dl_error_ok;

	duckVM_gclist_t *gclist = &duckVM->gclist;

	// Try once
	if (gclist->freeObjects_length == 0) {
		// STOP THE WORLD
		puts("Collect");
		e = duckVM_gclist_garbageCollect(duckVM);
		if (e) goto cleanup;;

		// Try twice
		if (gclist->freeObjects_length == 0) {
			e = dl_error_outOfMemory;
			goto cleanup;
		}
	}

	duckLisp_object_t *heapObject = gclist->freeObjects[--gclist->freeObjects_length];
	*heapObject = objectIn;
	if (objectIn.type == duckLisp_object_type_upvalueArray) {
		heapObject->value.upvalue_array.initialized = dl_false;
		if (objectIn.value.upvalue_array.length > 0) {
			e = DL_MALLOC(duckVM->memoryAllocation,
			              (void **) &heapObject->value.upvalue_array.upvalues,
			              objectIn.value.upvalue_array.length,
			              duckLisp_object_t **);
			if (e) goto cleanup;
		}
		else {
			heapObject->value.upvalue_array.upvalues = dl_null;
		}
		if (e) goto cleanup;
	}
	else if (objectIn.type == duckLisp_object_type_internalVector) {
		heapObject->value.internal_vector.initialized = dl_false;
		if (objectIn.value.internal_vector.length > 0) {
			e = DL_MALLOC(duckVM->memoryAllocation,
			              (void **) &heapObject->value.internal_vector.values,
			              objectIn.value.internal_vector.length,
			              duckLisp_object_t **);
			if (e) goto cleanup;
		}
		else {
			heapObject->value.internal_vector.values = dl_null;
		}
		if (e) goto cleanup;
	}
	*objectOut = heapObject;

 cleanup:

	return e;
}


dl_error_t duckVM_init(duckVM_t *duckVM, dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	/**/ dl_array_init(&duckVM->errors, duckVM->memoryAllocation, sizeof(duckLisp_error_t), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->stack, duckVM->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->call_stack, duckVM->memoryAllocation, sizeof(unsigned char *), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->upvalue_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(duckLisp_object_t *),
	                   dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->upvalue_array_call_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(duckLisp_object_t **),
	                   dl_array_strategy_fit);
	e = dl_array_pushElement(&duckVM->upvalue_array_call_stack, dl_null);
	if (e) goto l_cleanup;
	/**/ dl_array_init(&duckVM->upvalue_array_length_call_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(dl_size_t),
	                   dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->statics, duckVM->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_fit);
	e = duckVM_gclist_init(&duckVM->gclist, duckVM->memoryAllocation, maxObjects);
	if (e) goto l_cleanup;

	l_cleanup:

	return e;
}

void duckVM_quit(duckVM_t *duckVM) {
	dl_error_t e;
	e = dl_array_quit(&duckVM->stack);
	e = duckVM_gclist_garbageCollect(duckVM);
	e = dl_array_quit(&duckVM->upvalue_array_call_stack);
	/**/ dl_array_quit(&duckVM->upvalue_array_length_call_stack);
	/**/ duckVM_gclist_quit(&duckVM->gclist);
	/**/ dl_memclear(&duckVM->errors, sizeof(dl_array_t));
	e = dl_array_quit(&duckVM->statics);
	e = dl_array_quit(&duckVM->call_stack);
	e = dl_array_quit(&duckVM->upvalue_stack);
	(void) e;
}

static dl_error_t stack_push(duckVM_t *duckVM, duckLisp_object_t *object) {
	dl_error_t e = dl_error_ok;
	e = dl_array_pushElement(&duckVM->stack, object);
	if (e) goto cleanup;
	e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
 cleanup:
	return e;
}

/* This is only used by the FFI. */
static dl_error_t stack_pop(duckVM_t *duckVM, duckLisp_object_t *object) {
	dl_error_t e = dl_error_ok;
	e = dl_array_popElement(&duckVM->stack, object);
	if (e) goto cleanup;
	e = dl_array_popElement(&duckVM->upvalue_stack, dl_null);
 cleanup:
	return e;
}

static dl_error_t stack_pop_multiple(duckVM_t *duckVM, dl_size_t pops) {
	dl_error_t e = dl_error_ok;
	e = dl_array_popElements(&duckVM->upvalue_stack, dl_null, pops);
	if (e) goto cleanup;
	e = dl_array_popElements(&duckVM->stack, dl_null, pops);
 cleanup:
	return e;
 }

static dl_error_t call_stack_push(duckVM_t *duckVM,
                                  dl_uint8_t *ip,
                                  duckLisp_object_t **upvalues,
                                  dl_size_t upvalues_length) {
	dl_error_t e = dl_error_ok;
	e = dl_array_pushElement(&duckVM->call_stack, &ip);
	if (e) goto cleanup;
	e = dl_array_pushElement(&duckVM->upvalue_array_call_stack, &upvalues);
	if (e) goto cleanup;
	e = dl_array_pushElement(&duckVM->upvalue_array_length_call_stack, &upvalues_length);
 cleanup:
	return e;
}

static dl_error_t call_stack_pop(duckVM_t *duckVM, dl_uint8_t **ip) {
	dl_error_t e = dl_error_ok;
	e = dl_array_popElement(&duckVM->call_stack, ip);
	if (e) goto cleanup;
	e = dl_array_popElement(&duckVM->upvalue_array_call_stack, dl_null);
	if (e) goto cleanup;
	e = dl_array_popElement(&duckVM->upvalue_array_length_call_stack, dl_null);
 cleanup:
	return e;
}

dl_error_t duckVM_execute(duckVM_t *duckVM, duckLisp_object_t *return_value, dl_uint8_t *bytecode) {
	dl_error_t e = dl_error_ok;

	dl_uint8_t *ip = bytecode;
	dl_ptrdiff_t ptrdiff1;
	dl_ptrdiff_t ptrdiff2;
	dl_ptrdiff_t ptrdiff3;
	dl_size_t size1;
	dl_uint8_t uint8;
	duckLisp_object_t object1 = {0};
	duckLisp_object_t object2 = {0};
	duckLisp_object_t object3 = {0};
	duckLisp_object_t *objectPtr1;
	duckLisp_object_t *objectPtr2;
	/**/ dl_memclear(&object1, sizeof(duckLisp_object_t));
	duckLisp_object_t cons1;
	dl_bool_t bool1;
	dl_bool_t parsedBytecode;
	dl_uint8_t opcode;

	do {
		ptrdiff1 = 0;
		size1 = 0;
		parsedBytecode = dl_false;
		/**/ dl_memclear(&object1, sizeof(duckLisp_object_t));
		opcode = *(ip++);
		switch (opcode) {
		case duckLisp_instruction_nop:
			break;

		case duckLisp_instruction_pushSymbol32:
			object1.value.symbol.id = *(ip++);
			object1.value.symbol.id = *(ip++) + (object1.value.symbol.id << 8);
			object1.value.symbol.id = *(ip++) + (object1.value.symbol.id << 8);
			object1.value.symbol.id = *(ip++) + (object1.value.symbol.id << 8);
			object1.value.symbol.value_length = *(ip++);
			object1.value.symbol.value_length = *(ip++) + (object1.value.symbol.value_length << 8);
			object1.value.symbol.value_length = *(ip++) + (object1.value.symbol.value_length << 8);
			object1.value.symbol.value_length = *(ip++) + (object1.value.symbol.value_length << 8);
			object1.value.symbol.value = (char *) ip;
			ip += object1.value.symbol.value_length;
			object1.type = duckLisp_object_type_symbol;
			e = stack_push(duckVM, &object1);
			break;
		case duckLisp_instruction_pushSymbol16:
			object1.value.symbol.id = *(ip++);
			object1.value.symbol.id = *(ip++) + (object1.value.symbol.id << 8);
			object1.value.symbol.value_length = *(ip++);
			object1.value.symbol.value_length = *(ip++) + (object1.value.symbol.value_length << 8);
			object1.value.symbol.value = (char *) ip;
			ip += object1.value.symbol.value_length;
			object1.type = duckLisp_object_type_symbol;
			e = stack_push(duckVM, &object1);
			break;
		case duckLisp_instruction_pushSymbol8:
			object1.value.symbol.id = *(ip++);
			object1.value.symbol.value_length = *(ip++);
			object1.value.symbol.value = (char *) ip;
			ip += object1.value.symbol.value_length;
			object1.type = duckLisp_object_type_symbol;
			e = stack_push(duckVM, &object1);
			break;

		case duckLisp_instruction_pushString32:
			object1.value.string.value_length = *(ip++);
			object1.value.string.value_length = *(ip++) + (object1.value.string.value_length << 8);
			// Fall through
		case duckLisp_instruction_pushString16:
			object1.value.string.value_length = *(ip++) + (object1.value.string.value_length << 8);
			// Fall through
		case duckLisp_instruction_pushString8:
			object1.value.string.value_length = *(ip++) + (object1.value.string.value_length << 8);
			object1.value.string.value = (char *) ip;
			ip += object1.value.string.value_length;
			object1.type = duckLisp_object_type_string;
			e = stack_push(duckVM, &object1);
			break;

		case duckLisp_instruction_pushBooleanFalse:
			object1.type = duckLisp_object_type_bool;
			object1.value.integer = dl_false;
			e = stack_push(duckVM, &object1);
			break;
		case duckLisp_instruction_pushBooleanTrue:
			object1.type = duckLisp_object_type_bool;
			object1.value.integer = dl_true;
			e = stack_push(duckVM, &object1);
			break;

		case duckLisp_instruction_pushInteger32:
			object1.value.integer = *(ip++);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.type = duckLisp_object_type_integer;
			object1.value.integer = ((object1.value.integer > 0x7FFFFFFF)
			                         ? -(0x100000000 - object1.value.integer)
			                         : object1.value.integer);
			e = stack_push(duckVM, &object1);
			break;
		case duckLisp_instruction_pushInteger16:
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.type = duckLisp_object_type_integer;
			object1.value.integer = ((object1.value.integer > 0x7FFF)
			                         ? -(0x10000 - object1.value.integer)
			                         : object1.value.integer);
			e = stack_push(duckVM, &object1);
			break;
		case duckLisp_instruction_pushInteger8:
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.type = duckLisp_object_type_integer;
			object1.value.integer = ((object1.value.integer > 0x7F)
			                         ? -(0x100 - object1.value.integer)
			                         : object1.value.integer);
			e = stack_push(duckVM, &object1);
			break;

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
			if (e) break;
			object2 = object1;
			e = stack_push(duckVM, &object2);
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
				// Need another stack with same depth as call stack to pull out upvalues.
				// Like to live dangerously?
				if (ptrdiff1 < 0) {
					e = dl_error_invalidValue;
					break;
				}
				duckLisp_object_t *upvalue = DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
				                                                    duckLisp_object_t **)[ptrdiff1];
				if (upvalue->value.upvalue.type == duckVM_upvalue_type_stack_index) {
					e = dl_array_get(&duckVM->stack, &object1, upvalue->value.upvalue.value.stack_index);
					if (e) break;
				}
				else if (upvalue->value.upvalue.type == duckVM_upvalue_type_heap_object) {
					object1 = *upvalue->value.upvalue.value.heap_object;
				}
				else {
					while (upvalue->value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
						upvalue = upvalue->value.upvalue.value.heap_upvalue;
					}
					if (upvalue->value.upvalue.type == duckVM_upvalue_type_stack_index) {
						e = dl_array_get(&duckVM->stack, &object1, upvalue->value.upvalue.value.stack_index);
						if (e) break;
					}
					else {
						object1 = *upvalue->value.upvalue.value.heap_object;
					}
				}
				// We are eventually going to have *major* problems with strings.
				e = stack_push(duckVM, &object1);
				break;
			}

		case duckLisp_instruction_pushClosure32:
			/* Fall through */
		case duckLisp_instruction_pushVaClosure32:
			object1.value.closure.variadic = (opcode == duckLisp_instruction_pushVaClosure32);
			object1.value.closure.name = *(ip++);
			object1.value.closure.name = *(ip++) + (object1.value.closure.name << 8);
			object1.value.closure.name = *(ip++) + (object1.value.closure.name << 8);
			object1.value.closure.name = *(ip++) + (object1.value.closure.name << 8);
			object1.type = duckLisp_object_type_closure;

			object1.value.closure.arity = *(ip++);

			size1 = *(ip++);
			size1 = *(ip++) + (size1 << 8);
			size1 = *(ip++) + (size1 << 8);
			size1 = *(ip++) + (size1 << 8);

			/* This could also point to a static version instead since this array is never changed
			   and multiple closures could use the same array. */

			{
				duckLisp_object_t upvalue_array;
				upvalue_array.type = duckLisp_object_type_upvalueArray;
				upvalue_array.value.upvalue_array.initialized = dl_false;
				upvalue_array.value.upvalue_array.length = size1;
				e = duckVM_gclist_pushObject(duckVM, &object1.value.closure.upvalue_array, upvalue_array);
				if (e) break;
			}
			/* Immediately push on stack so that the GC can see it. Allocating upvalues could trigger a GC. */
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;

			dl_bool_t recursive = dl_false;

			DL_DOTIMES(k, object1.value.closure.upvalue_array->value.upvalue_array.length) {
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
					if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > (duckVM->upvalue_stack.elements_length - 1))) {
						e = dl_error_invalidValue;
						break;
					}
				}
				duckLisp_object_t *upvalue_pointer = dl_null;
				if ((dl_size_t) ptrdiff1 == (duckVM->upvalue_stack.elements_length - 1)) {
					/* *Recursion* Capture upvalue we are about to push. */
					recursive = dl_true;
					/* Our upvalue definitely doesn't exist yet. */
					duckLisp_object_t upvalue;
					upvalue.type = duckLisp_object_type_upvalue;
					upvalue.value.upvalue.type = duckVM_upvalue_type_stack_index;  /* Lie for now. */
					upvalue.value.upvalue.value.stack_index = ptrdiff1;
					e = duckVM_gclist_pushObject(duckVM, &upvalue_pointer, upvalue);
					if (e) break;
					/* Break push_scope abstraction. */
					e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
					/* Upvalue stack slot exists now, so insert the new UV. */
					DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckLisp_object_t *, ptrdiff1) = upvalue_pointer;
				}
				else if (ptrdiff1 < 0) {
					/* Capture upvalue in currently executing function. */
					ptrdiff1 = -(ptrdiff1 + 1);
					duckLisp_object_t upvalue;
					upvalue.type = duckLisp_object_type_upvalue;
					upvalue.value.upvalue.type = duckVM_upvalue_type_heap_upvalue;
					/* Link new upvalue to upvalue from current context. */
					upvalue.value.upvalue.value.heap_upvalue = DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
					                                                                  duckLisp_object_t **)[ptrdiff1];
					e = duckVM_gclist_pushObject(duckVM, &upvalue_pointer, upvalue);
					if (e) break;
				}
				else {
					/* Capture upvalue on stack. */
					upvalue_pointer = DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckLisp_object_t *, ptrdiff1);
					if (upvalue_pointer == dl_null) {
						duckLisp_object_t upvalue;
						upvalue.type = duckLisp_object_type_upvalue;
						upvalue.value.upvalue.type = duckVM_upvalue_type_stack_index;
						upvalue.value.upvalue.value.stack_index = ptrdiff1;
						e = duckVM_gclist_pushObject(duckVM, &upvalue_pointer, upvalue);
						if (e) break;
						// Is this a bug? I'm not confident that this element exists.
						/* Keep reference to upvalue on stack so that we know where to release the object to. */
						DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckLisp_object_t *, ptrdiff1) = upvalue_pointer;
					}
				}
				object1.value.closure.upvalue_array->value.upvalue_array.upvalues[k] = upvalue_pointer;
			}
			if (!recursive) {
				/* Break push_scope abstraction. */
				e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
				if (e) break;
			}
			/* e = stack_push(duckVM, &object1); */
			object1.value.closure.upvalue_array->value.upvalue_array.initialized = dl_true;
			DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckLisp_object_t) = object1;
			break;

		case duckLisp_instruction_releaseUpvalues8:
			size1 = *(ip++);

			DL_DOTIMES(k, size1) {
				ptrdiff1 = *(ip++);
				ptrdiff1 = duckVM->stack.elements_length - ptrdiff1;
				if ((ptrdiff1 < 0) || ((dl_size_t) ptrdiff1 > duckVM->upvalue_stack.elements_length)) {
					e = dl_error_invalidValue;
					break;
				}
				duckLisp_object_t *upvalue = DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckLisp_object_t *, ptrdiff1);
				if (upvalue != dl_null) {
					duckLisp_object_t *object = &DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, ptrdiff1);
					e = duckVM_gclist_pushObject(duckVM, &upvalue->value.upvalue.value.heap_object, *object);
					if (e) break;
					upvalue->value.upvalue.type = duckVM_upvalue_type_heap_object;
					/* Render the original object unusable. */
					object->type = duckLisp_object_type_list;
					object->value.list = dl_null;
				}
			}
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
				duckLisp_object_t *upvalue = DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
				                                                    duckLisp_object_t **)[ptrdiff1];
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

		case duckLisp_instruction_funcall8:
			ptrdiff1 = *(ip++);
			uint8 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_closure) {
				e = dl_error_invalidValue;
				break;
			}
			if (object1.value.closure.variadic) {
				if (uint8 < object1.value.closure.arity) {
					e = dl_error_invalidValue;
					break;
				}
				/* Create list. */
				duckLisp_object_t *lastConsPtr = dl_null;
				DL_DOTIMES(k, (dl_size_t) (uint8 - object1.value.closure.arity)) {
					duckLisp_object_t cons;
					duckLisp_object_t *consPtr = dl_null;
					duckLisp_object_t object;
					duckLisp_object_t *objectPtr = dl_null;
					e = stack_pop(duckVM, &object);
					if (e) break;
					/* object = DL_ARRAY_GETADDRESS(duckVM->stack, */
					/*                              duckLisp_object_t, */
					/*                              ((duckVM->stack.elements_length - 1) - k)); */
					e = duckVM_gclist_pushObject(duckVM, &objectPtr, object);
					if (e) break;
					cons.type = duckLisp_object_type_cons;
					cons.value.cons.car = objectPtr;
					cons.value.cons.cdr = lastConsPtr;
					e = duckVM_gclist_pushObject(duckVM, &consPtr, cons);
					if (e) break;
					lastConsPtr = consPtr;
				}
				if (e) break;
				/* Push list. */
				object2.value.list = lastConsPtr;
				object2.type = duckLisp_object_type_list;
				e = stack_push(duckVM, &object2);
				if (e) break;
			}
			else {
				if (object1.value.closure.arity != uint8) {
					e = dl_error_invalidValue;
					break;
				}
			}
			/* Call. */
			e = call_stack_push(duckVM,
			                    ip,
			                    object1.value.closure.upvalue_array->value.upvalue_array.upvalues,
			                    object1.value.closure.upvalue_array->value.upvalue_array.length);
			if (e) break;
			ip = &bytecode[object1.value.closure.name];
			break;

		case duckLisp_instruction_apply8:
			ptrdiff1 = *(ip++);
			uint8 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_closure) {
				e = dl_error_invalidValue;
				break;
			}
			duckLisp_object_t rest;
			e = stack_pop(duckVM, &rest);
			if (e) break;
			if (rest.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				break;
			}
			while ((uint8 < object1.value.closure.arity) && (rest.value.list != dl_null)) {
				if ((rest.value.list->type != duckLisp_object_type_cons)
				    || rest.value.list->type != duckLisp_object_type_cons) {
					e = dl_error_invalidValue;
					break;
				}
				/* (push (car rest) stack) */
				e = stack_push(duckVM, rest.value.list->value.cons.car);
				if (e) break;
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
				duckLisp_object_t *lastConsPtr = rest.value.list;
				DL_DOTIMES(k, (dl_size_t) (uint8 - object1.value.closure.arity)) {
					duckLisp_object_t cons;
					duckLisp_object_t *consPtr = dl_null;
					duckLisp_object_t object;
					duckLisp_object_t *objectPtr = dl_null;
					e = stack_pop(duckVM, &object);
					if (e) break;
					/* object = DL_ARRAY_GETADDRESS(duckVM->stack, */
					/*                              duckLisp_object_t, */
					/*                              ((duckVM->stack.elements_length - 1) - k)); */
					e = duckVM_gclist_pushObject(duckVM, &objectPtr, object);
					if (e) break;
					cons.value.cons.car = objectPtr;
					cons.value.cons.cdr = lastConsPtr;
					cons.type = duckLisp_object_type_cons;
					e = duckVM_gclist_pushObject(duckVM, &consPtr, cons);
					if (e) break;
					lastConsPtr = consPtr;
				}
				if (e) break;
				/* Push list. */
				object2.value.list = lastConsPtr;
				object2.type = duckLisp_object_type_list;
				e = stack_push(duckVM, &object2);
				if (e) break;
			}
			else {
				if (object1.value.closure.arity != uint8) {
					e = dl_error_invalidValue;
					break;
				}
			}
			/* Call. */
			e = call_stack_push(duckVM,
			                    ip,
			                    object1.value.closure.upvalue_array->value.upvalue_array.upvalues,
			                    object1.value.closure.upvalue_array->value.upvalue_array.length);
			if (e) break;
			ip = &bytecode[object1.value.closure.name];
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
			call_stack_push(duckVM, ip, dl_null, 0);
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

		/* 	// Not implemented properly. */
		/* // I probably don't need an `if` if I research the standard a bit. */
		/* case duckLisp_instruction_acall32: */
		/* 	ptrdiff1 = *(ip++); */
		/* 	ptrdiff1 = *(ip++) + (ptrdiff1 << 8); */
		/* 	ptrdiff1 = *(ip++) + (ptrdiff1 << 8); */
		/* 	ptrdiff1 = *(ip++) + (ptrdiff1 << 8); */
		/* 	e = dl_array_pushElement(&duckVM->call_stack, &ip); */
		/* 	if (e) break; */
		/* 	if (ptrdiff1 & 0x80000000ULL) { */
		/* 		ip -= ((~((dl_size_t) ptrdiff1) + 1) & 0xFFFFFFFFULL); */
		/* 	} */
		/* 	else { */
		/* 		ip += ptrdiff1; */
		/* 	} */
		/* 	--ip; */
		/* 	break; */
		/* case duckLisp_instruction_acall16: */
		/* 	ptrdiff1 = *(ip++); */
		/* 	ptrdiff1 = *(ip++) + (ptrdiff1 << 8); */
		/* 	e = dl_array_pushElement(&duckVM->call_stack, &ip); */
		/* 	if (e) break; */
		/* 	if (ptrdiff1 & 0x8000ULL) { */
		/* 		ip -= ((~ptrdiff1 + 1) & 0xFFFFULL); */
		/* 	} */
		/* 	else { */
		/* 		ip += ptrdiff1; */
		/* 	} */
		/* 	--ip; */
		/* 	break; */
		/* case duckLisp_instruction_acall8: */
		/* 	ptrdiff1 = *(ip++); */
		/* 	ptrdiff2 = *(ip++); */
		/* 	e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1); */
		/* 	if (e) break; */
		/* 	if (object1.type != duckLisp_object_type_integer) { */
		/* 		e = dl_error_invalidValue; */
		/* 		break; */
		/* 	} */
		/* 	e = dl_array_pushElement(&duckVM->call_stack, &ip); */
		/* 	if (e) break; */
		/* 	ip = &bytecode[object1.value.integer]; */
		/* 	e = stack_pop_multiple(duckVM, ptrdiff2); */
		/* 	if (e) break; */
		/* 	break; */

		case duckLisp_instruction_ccall32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			// Fall through
		case duckLisp_instruction_ccall16:
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			// Fall through
		case duckLisp_instruction_ccall8:
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = DL_ARRAY_GETADDRESS(duckVM->statics, duckLisp_object_t, ptrdiff1).value.function.callback(duckVM);
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
				if (object1.type == duckLisp_object_type_bool) {
					truthy = object1.value.boolean;
				}
				else if (object1.type == duckLisp_object_type_integer) {
					truthy = object1.value.integer != 0;
				}
				else if (object1.type == duckLisp_object_type_float) {
					truthy = object1.value.floatingPoint != 0.0;
				}
				else if (object1.type == duckLisp_object_type_symbol) {
					truthy = dl_true;
				}
				else if (object1.type == duckLisp_object_type_list) {
					truthy = object1.value.list != dl_null;
				}
				else if (object1.type == duckLisp_object_type_closure) {
					truthy = dl_true;
				}
				else if (object1.type == duckLisp_object_type_function) {
					truthy = dl_true;
				}
				else if (object1.type == duckLisp_object_type_string) {
					truthy = dl_true;
				}
				else if (object1.type == duckLisp_object_type_vector) {
					truthy = object1.value.vector.internal_vector != dl_null;
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
					--ip; // This accounts for the pop argument.
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
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			switch (object1.type) {
			case duckLisp_object_type_integer:
				object1.value.integer = !object1.value.integer;
				break;
			case duckLisp_object_type_bool:
				object1.value.boolean = !object1.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			e = stack_push(duckVM, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_not16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			switch (object1.type) {
			case duckLisp_object_type_integer:
				object1.value.integer = !object1.value.integer;
				break;
			case duckLisp_object_type_bool:
				object1.value.boolean = !object1.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			e = stack_push(duckVM, &object1);
			break;
		case duckLisp_instruction_not8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			switch (object1.type) {
			case duckLisp_object_type_integer:
				object1.value.integer = !object1.value.integer;
				break;
			case duckLisp_object_type_bool:
				object1.value.boolean = !object1.value.boolean;
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint *= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint *= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint *= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer * object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer *= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer *= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean * object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean * object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean *= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint *= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint *= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint *= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer * object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer *= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer *= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean * object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean * object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean *= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint *= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint *= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint *= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer * object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer *= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer *= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean * object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean * object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean *= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint /= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint /= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint /= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer / object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer /= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer /= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean / object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean / object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean /= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint /= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint /= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint /= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer / object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer /= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer /= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean / object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean / object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean /= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint /= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint /= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint /= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer / object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer /= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer /= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean / object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean / object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean /= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			e = stack_push(duckVM, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_add32:
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint += object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint += object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint += object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer + object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer += object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer += object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean + object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean + object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean += object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			e = stack_push(duckVM, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_add16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff2 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
			if (e) break;
			switch (object1.type) {
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint += object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint += object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint += object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer + object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer += object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer += object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean + object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean + object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean += object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			e = stack_push(duckVM, &object1);
			break;
		case duckLisp_instruction_add8:
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			e = dl_array_get(&duckVM->stack, &object2, duckVM->stack.elements_length - ptrdiff2);
			if (e) break;
			switch (object1.type) {
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint += object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint += object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint += object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer + object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer += object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer += object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean + object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean + object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean += object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint -= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint -= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint -= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer - object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer -= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer -= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean - object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean - object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean -= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint -= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint -= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint -= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer - object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer -= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer -= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean - object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean - object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean -= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint -= object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.floatingPoint -= object2.value.integer;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_bool:
					object1.value.floatingPoint -= object2.value.boolean;
					object1.type = duckLisp_object_type_float;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.integer - object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					/* Type already set. */
					object1.value.integer -= object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.integer -= object2.value.boolean;
					object1.type = duckLisp_object_type_integer;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.floatingPoint = object1.value.boolean - object2.value.floatingPoint;
					object1.type = duckLisp_object_type_float;
					break;
				case duckLisp_object_type_integer:
					object1.value.integer = object1.value.boolean - object2.value.integer;
					object1.type = duckLisp_object_type_integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean -= object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.floatingPoint > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.floatingPoint > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.floatingPoint > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.integer > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.integer > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.integer > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.boolean > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.boolean > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.boolean > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object1.type = duckLisp_object_type_bool;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.floatingPoint > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.floatingPoint > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.floatingPoint > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.integer > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.integer > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.integer > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.boolean > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.boolean > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.boolean > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object1.type = duckLisp_object_type_bool;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.floatingPoint > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.floatingPoint > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.floatingPoint > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.integer > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.integer > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.integer > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.boolean > object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.boolean > object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.boolean > object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object1.type = duckLisp_object_type_bool;
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
			case duckLisp_object_type_list:
				switch (object2.type) {
				case duckLisp_object_type_list:
					object1.value.boolean = object1.value.list == object2.value.list;
					break;
				default:
					object1.value.boolean = dl_false;
				}
				break;
			case duckLisp_object_type_symbol:
				switch (object2.type) {
				case duckLisp_object_type_symbol:
					object1.value.boolean = object1.value.symbol.id == object2.value.symbol.id;
					break;
				default:
					object1.value.boolean = dl_false;
				}
				break;
			case duckLisp_object_type_string:
				switch (object2.type) {
				case duckLisp_object_type_string:
					/**/ dl_string_compare(&bool1,
										   object1.value.string.value,
										   object1.value.string.value_length,
										   object2.value.string.value,
										   object2.value.string.value_length);
					object1.value.boolean = bool1;
					break;
				default:
					object1.value.boolean = dl_false;
				}
				break;
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.floatingPoint == object2.value.floatingPoint;
					break;
				default:
					object1.value.boolean = dl_false;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.integer == object2.value.integer;
					break;
				default:
					object1.value.boolean = dl_false;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.boolean > object2.value.boolean;
					break;
				default:
					object1.value.boolean = dl_false;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object1.type = duckLisp_object_type_bool;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.floatingPoint < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.floatingPoint < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.floatingPoint < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.integer < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.integer < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.integer < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.boolean < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.boolean < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.boolean < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object1.type = duckLisp_object_type_bool;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.floatingPoint < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.floatingPoint < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.floatingPoint < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.integer < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.integer < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.integer < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.boolean < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.boolean < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.boolean < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object1.type = duckLisp_object_type_bool;
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
			case duckLisp_object_type_float:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.floatingPoint < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.floatingPoint < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.floatingPoint < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_integer:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.integer < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.integer < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.integer < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			case duckLisp_object_type_bool:
				switch (object2.type) {
				case duckLisp_object_type_float:
					object1.value.boolean = object1.value.boolean < object2.value.floatingPoint;
					break;
				case duckLisp_object_type_integer:
					object1.value.boolean = object1.value.boolean < object2.value.integer;
					break;
				case duckLisp_object_type_bool:
					object1.value.boolean = object1.value.boolean < object2.value.boolean;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				break;
			default:
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object1.type = duckLisp_object_type_bool;
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
			cons1.type = duckLisp_object_type_cons;
			cons1.value.cons.car = dl_null;
			cons1.value.cons.cdr = dl_null;
			object3.type = duckLisp_object_type_list;
			e = duckVM_gclist_pushObject(duckVM, &object3.value.list, cons1);
			if (e) break;
			e = stack_push(duckVM, &object3);
			if (e) break;
			/* Create the elements of the cons. */
			if (object1.type == duckLisp_object_type_list) {
				object3.value.list->value.cons.car = object1.value.list;
			}
			else {
				e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
				if (e) break;
				object3.value.list->value.cons.car = objectPtr1;
			}
			if (object2.type == duckLisp_object_type_list) {
				object3.value.list->value.cons.cdr = object2.value.list;
			}
			else {
				e = duckVM_gclist_pushObject(duckVM, &objectPtr2, object2);
				if (e) break;
				object3.value.list->value.cons.cdr = objectPtr2;
			}
			DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckLisp_object_t) = object3;
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

			object1.type = duckLisp_object_type_vector;
			object1.value.vector.offset = 0;
			object1.value.vector.internal_vector = dl_null;

			{
				duckLisp_object_t internal_vector;
				internal_vector.type = duckLisp_object_type_internalVector;
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
			DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckLisp_object_t) = object1;
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
			if (object1.type != duckLisp_object_type_integer) {
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

			object1.type = duckLisp_object_type_vector;
			object1.value.vector.offset = 0;
			object1.value.vector.internal_vector = dl_null;

			{
				duckLisp_object_t internal_vector;
				internal_vector.type = duckLisp_object_type_internalVector;
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
			DL_ARRAY_GETTOPADDRESS(duckVM->stack, duckLisp_object_t) = object1;
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
			if (object1.type != duckLisp_object_type_vector) {
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
			if (object2.type != duckLisp_object_type_integer) {
				e = dl_error_invalidValue;
				break;
			}

			ptrdiff1 = object2.value.integer;
			if ((dl_size_t) (ptrdiff1 + object1.value.vector.offset)
			    >= object1.value.vector.internal_vector->value.internal_vector.length) {
				e = dl_error_invalidValue;
				break;
			}
			object2 = *object1.value.vector.internal_vector->value.internal_vector.values[object1.value.vector.offset
			                                                                              + ptrdiff1];
			e = stack_push(duckVM, &object2);
			if (e) break;
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
			if (object1.type != duckLisp_object_type_vector) {
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
			if (object2.type != duckLisp_object_type_integer) {
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
			if (object1.type == duckLisp_object_type_list) {
				if (object1.value.list == dl_null) {
					object2.type = duckLisp_object_type_list;
					object2.value.list = dl_null;
				}
				else {
					if (object1.value.list->type == duckLisp_object_type_cons) {
						duckLisp_object_t *cdr = object1.value.list->value.cons.cdr;
						if (cdr == dl_null) {
							object2.type = duckLisp_object_type_list;
							object2.value.list = dl_null;
						}
						/* cdr can be a cons or something else. */
						else if (cdr->type == duckLisp_object_type_cons) {
							object2.type = duckLisp_object_type_list;
							object2.value.list = cdr;
						}
						else {
							object2 = *cdr;
						}
					}
					else {
						e = dl_error_invalidValue;
						break;
					}
				}
			}
			else if (object1.type == duckLisp_object_type_vector) {
				if (object1.value.vector.internal_vector == dl_null) {
					object2.type = duckLisp_object_type_vector;
					object2.value.vector.internal_vector = dl_null;
				}
				else {
					if (object1.value.vector.internal_vector->type == duckLisp_object_type_internalVector) {
						object2.type = duckLisp_object_type_vector;
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
			else {
				e = dl_error_invalidValue;
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
			if (object1.type == duckLisp_object_type_list) {
				if (object1.value.list == dl_null) {
					object2.type = duckLisp_object_type_list;
					object2.value.list = dl_null;
				}
				else {
					if (object1.value.list->type == duckLisp_object_type_cons) {
						duckLisp_object_t *car = object1.value.list->value.cons.car;
						if (car == dl_null) {
							object2.type = duckLisp_object_type_list;
							object2.value.list = dl_null;
						}
						/* car can be a cons or something else. */
						else if (car->type == duckLisp_object_type_cons) {
							object2.type = duckLisp_object_type_list;
							object2.value.list = car;
						}
						else {
							object2 = *car;
						}
					}
					else {
						e = dl_error_invalidValue;
						break;
					}
				}
			}
			else if (object1.type == duckLisp_object_type_vector) {
				if (object1.value.vector.internal_vector == dl_null) {
					object2.type = duckLisp_object_type_vector;
					object2.value.vector.internal_vector = dl_null;
				}
				else {
					if (object1.value.vector.internal_vector->type == duckLisp_object_type_internalVector) {
						if ((object1.value.vector.internal_vector->value.internal_vector.length == 0)
						    || ((dl_size_t) object1.value.vector.offset
						        >= object1.value.vector.internal_vector->value.internal_vector.length)) {
							object2.type = duckLisp_object_type_vector;
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
			else {
				e = dl_error_invalidValue;
				break;
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;

		// I probably don't need an `if` if I research the standard a bit.
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

			if ((object2.type == duckLisp_object_type_list) && (object2.value.list != dl_null)) {
				if (object1.type == duckLisp_object_type_list) {
					if (object1.value.list == dl_null) object2.value.list->value.cons.car = dl_null;
					else object2.value.list->value.cons.car = object1.value.list;
				}
				else {
					e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
					if (e) break;
					object2.value.list->value.cons.car = objectPtr1;
				}
			}
			else if ((object2.type == duckLisp_object_type_vector)
			         && (object2.value.vector.internal_vector != dl_null)) {
				e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
				if (e) break;
				(object2.value.vector.internal_vector
				 ->value.internal_vector.values[object2.value.vector.offset]) = objectPtr1;
			}
			else e = dl_error_invalidValue;
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

			if ((object2.type == duckLisp_object_type_list) && (object2.value.list != dl_null)) {
				if (object1.type == duckLisp_object_type_list) {
					if (object1.value.list == dl_null) object2.value.list->value.cons.cdr = dl_null;
					else object2.value.list->value.cons.cdr = object1.value.list;
				}
				else {
					e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
					if (e) break;
					object2.value.list->value.cons.cdr = objectPtr1;
				}
			}
			else if ((object2.type == duckLisp_object_type_vector)
			         && (object2.value.vector.internal_vector != dl_null)
			         && (((object1.type == duckLisp_object_type_vector)
			              && (object1.value.vector.internal_vector == dl_null))
			             || ((object1.type == duckLisp_object_type_list)
			                 && (object1.value.list == dl_null)))) {
				object2.value.vector.internal_vector->value.internal_vector.length = object2.value.vector.offset;
			}
			else e = dl_error_invalidValue;
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
				object2.type = duckLisp_object_type_bool;
			if (object1.type == duckLisp_object_type_list) {
				object2.value.boolean = (object1.value.list == dl_null);
			}
			else if (object1.type == duckLisp_object_type_vector) {
				object2.value.boolean = object1.value.vector.internal_vector == dl_null;
			}
			else {
				object2.value.boolean = dl_false;
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_typeof32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			object2.type = duckLisp_object_type_integer;
			object2.value.integer = object1.type;
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_typeof16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			object2.type = duckLisp_object_type_integer;
			object2.value.integer = object1.type;
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_typeof8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			object2.type = duckLisp_object_type_integer;
			object2.value.integer = object1.type;
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;

		case duckLisp_instruction_return32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			parsedBytecode = dl_true;
			// Fall through
		case duckLisp_instruction_return16:
			if (!parsedBytecode) {
				ptrdiff1 = *(ip++);
				ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
				parsedBytecode = dl_true;
			}
			// Fall through
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
			e = call_stack_pop(duckVM, &ip);
			if (e == dl_error_bufferUnderflow) {
				e = dl_error_ok;
				goto l_cleanup;
			}
			break;
		case duckLisp_instruction_return0:
			e = call_stack_pop(duckVM, &ip);
			if (e == dl_error_bufferUnderflow) {
				e = dl_error_ok;
				goto l_cleanup;
			}
			break;

		case duckLisp_instruction_nil:
			object1.type = duckLisp_object_type_list;
			object1.value.list = dl_null;
			e = stack_push(duckVM, &object1);
			if (e) break;
			break;

		default:
			e = dl_error_invalidValue;
			goto l_cleanup;
		}
	} while (!e);

	l_cleanup:

	if (e) {
		puts("VM ERROR");
		printf("ip 0x%llX\n", (dl_size_t) (ip - bytecode));
		printf("*ip 0x%X\n", *ip);
	}
	else {
		if (duckVM->stack.elements_length == 0) e = duckVM_pushNil(duckVM);
		e = stack_pop(duckVM, return_value);
	}

	return e;
}

dl_error_t duckVM_callLocal(duckVM_t *duckVM, duckLisp_object_t *return_value, dl_ptrdiff_t function_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_object_t functionObject;

	// Allow addressing relative to the top of the stack with negative indices.
	if (function_index < 0) {
		function_index = duckVM->stack.elements_length + function_index;
	}
	if ((function_index < 0) || ((dl_size_t) function_index  >= duckVM->stack.elements_length)) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	functionObject = DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, function_index);
	if (functionObject.type != duckLisp_object_type_function) {
		// Push runtime error.
		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	if (functionObject.value.function.callback != dl_null) {
		e = functionObject.value.function.callback(duckVM);
	}
	else if (functionObject.value.function.bytecode != dl_null) {
		e = duckVM_execute(duckVM, return_value, functionObject.value.function.bytecode);
	}
	else {
		e = dl_error_invalidValue;
	}

	l_cleanup:

	return e;
}

dl_error_t duckVM_linkCFunction(duckVM_t *duckVM, dl_ptrdiff_t callback_index, dl_error_t (*callback)(duckVM_t *)) {
	dl_error_t e = dl_error_ok;

	duckLisp_object_t object;
	dl_memclear(&object, sizeof(duckLisp_object_t));
	object.type = duckLisp_object_type_function;
	object.value.function.callback = callback;

	// Make room for the object if the index reaches beyond the end.
	if ((dl_size_t) callback_index >= duckVM->statics.elements_length) {
		e = dl_array_pushElements(&duckVM->statics, dl_null, 1 + callback_index - duckVM->statics.elements_length);
		if (e) {
			goto l_cleanup;
		}
	}

	// Insert the callback.
	DL_ARRAY_GETADDRESS(duckVM->statics, duckLisp_object_t, callback_index) = object;

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

dl_error_t duckVM_pop(duckVM_t *duckVM, duckLisp_object_t *object) {
	return stack_pop(duckVM, object);
}

dl_error_t duckVM_push(duckVM_t *duckVM, duckLisp_object_t *object) {
	return stack_push(duckVM, object);
}

dl_error_t duckVM_pushNil(duckVM_t *duckVM) {
	duckLisp_object_t object;
	object.type = duckLisp_object_type_list;
	object.value.list = dl_null;
	return stack_push(duckVM, &object);
}

/* dl_error_t duckVM_pushReturn(duckVM_t *duckVM, duckLisp_object_t object) { */
/* 	dl_error_t e = dl_error_ok; */

/* 	e = dl_array_pushElement(&duckVM->stack, (void *) &object); */
/* 	duckVM->frame_pointer++; */

/* 	return e; */
/* } */

