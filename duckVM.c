#include "duckVM.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "duckLisp.h"
#include <stdio.h>

static dl_error_t duckVM_gclist_markCons(duckVM_gclist_t *gclist, duckVM_gclist_cons_t *cons);
static dl_error_t duckVM_gclist_markUpvalue(duckVM_gclist_t *gclist, duckVM_upvalue_t *upvalue);
static dl_error_t duckVM_gclist_markUpvalueArray(duckVM_gclist_t *gclist, duckVM_upvalue_array_t *upvalueArray);

dl_error_t duckVM_gclist_init(duckVM_gclist_t *gclist,
                              dl_memoryAllocation_t *memoryAllocation,
                              const dl_size_t maxUpvalues,
                              const dl_size_t maxUpvalueArrays,
                              const dl_size_t maxConses,
                              const dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	gclist->memoryAllocation = memoryAllocation;


	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->upvalues,
				  maxUpvalues * sizeof(duckVM_upvalue_t));
	if (e) goto cleanup;
	gclist->upvalues_length = maxUpvalues;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->upvalueArrays,
				  maxUpvalues * sizeof(duckVM_upvalue_array_t));
	if (e) goto cleanup;
	gclist->upvalueArrays_length = maxUpvalueArrays;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->conses,
				  maxConses * sizeof(duckVM_gclist_cons_t));
	if (e) goto cleanup;
	gclist->conses_length = maxConses;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->objects,
				  maxObjects * sizeof(duckLisp_object_t));
	if (e) goto cleanup;
	gclist->objects_length = maxObjects;


	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->freeUpvalues,
				  maxUpvalues * sizeof(duckVM_upvalue_t *));
	if (e) goto cleanup;
	gclist->freeUpvalues_length = maxUpvalues;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->freeUpvalueArrays,
				  maxUpvalueArrays * sizeof(duckVM_upvalue_array_t *));
	if (e) goto cleanup;
	gclist->freeUpvalueArrays_length = maxUpvalueArrays;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->freeConses,
				  maxConses * sizeof(duckVM_gclist_cons_t *));
	if (e) goto cleanup;
	gclist->freeConses_length = maxConses;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->freeObjects,
				  maxObjects * sizeof(duckLisp_object_t *));
	if (e) goto cleanup;
	gclist->freeObjects_length = maxObjects;


	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->upvalueInUse,
				  maxUpvalues * sizeof(dl_bool_t));
	if (e) goto cleanup;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->upvalueArraysInUse,
				  maxUpvalueArrays * sizeof(dl_bool_t));
	if (e) goto cleanup;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->consInUse,
				  maxConses * sizeof(dl_bool_t));
	if (e) goto cleanup;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->objectInUse,
				  maxObjects * sizeof(dl_bool_t));
	if (e) goto cleanup;


	/**/ dl_memclear(gclist->upvalues, gclist->upvalues_length * sizeof(duckVM_upvalue_t));
	/**/ dl_memclear(gclist->upvalueArrays, gclist->upvalueArrays_length * sizeof(duckVM_upvalue_array_t));
	/**/ dl_memclear(gclist->conses, gclist->conses_length * sizeof(duckVM_gclist_cons_t));
	/**/ dl_memclear(gclist->objects, gclist->objects_length * sizeof(duckLisp_object_t));


	for (dl_ptrdiff_t i = 0; (dl_size_t) i < maxUpvalues; i++) {
		gclist->freeUpvalues[i] = &gclist->upvalues[i];
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < maxUpvalueArrays; i++) {
		gclist->freeUpvalueArrays[i] = &gclist->upvalueArrays[i];
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < maxConses; i++) {
		gclist->freeConses[i] = &gclist->conses[i];
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < maxObjects; i++) {
		gclist->freeObjects[i] = &gclist->objects[i];
	}

 cleanup:

	return e;
}

static dl_error_t duckVM_gclist_quit(duckVM_gclist_t *gclist) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	e = dl_free(gclist->memoryAllocation, (void **) &gclist->freeUpvalues);
	gclist->freeUpvalues_length = 0;

	e = dl_free(gclist->memoryAllocation, (void **) &gclist->freeUpvalueArrays);
	gclist->freeUpvalueArrays_length = 0;

	e = dl_free(gclist->memoryAllocation, (void **) &gclist->freeConses);
	gclist->freeConses_length = 0;

	e = dl_free(gclist->memoryAllocation, (void **) &gclist->freeObjects);
	gclist->freeObjects_length = 0;


	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->upvalues);
	e = eError ? eError : e;
	gclist->upvalues_length = 0;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->upvalueArrays);
	e = eError ? eError : e;
	gclist->upvalueArrays_length = 0;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->conses);
	e = eError ? eError : e;
	gclist->conses_length = 0;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->objects);
	e = eError ? eError : e;
	gclist->objects_length = 0;


	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->upvalueInUse);
	e = eError ? eError : e;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->upvalueArraysInUse);
	e = eError ? eError : e;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->consInUse);
	e = eError ? eError : e;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->objectInUse);
	e = eError ? eError : e;

	return e;
}

static dl_error_t duckVM_gclist_markObject(duckVM_gclist_t *gclist, duckLisp_object_t *object) {
	if (object) {
		gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)] = dl_true;
		if (object->type == duckLisp_object_type_list) {
			return duckVM_gclist_markCons(gclist, object->value.list);
		}
		else if (object->type == duckLisp_object_type_closure) {
			return duckVM_gclist_markUpvalueArray(gclist, object->value.closure.upvalue_array);
		}
	}
	return dl_error_ok;
}

static dl_error_t duckVM_gclist_markCons(duckVM_gclist_t *gclist, duckVM_gclist_cons_t *cons) {
	dl_error_t e = dl_error_ok;
	if (cons && !gclist->consInUse[(dl_ptrdiff_t) (cons - gclist->conses)]) {
		gclist->consInUse[(dl_ptrdiff_t) (cons - gclist->conses)] = dl_true;
		if ((cons->type == duckVM_gclist_cons_type_addrAddr) ||
			(cons->type == duckVM_gclist_cons_type_addrObject)) {
			e = duckVM_gclist_markCons(gclist, cons->car.addr);
		}
		else {
			e = duckVM_gclist_markObject(gclist, cons->car.data);
		}
		if (e) goto cleanup;
		if ((cons->type == duckVM_gclist_cons_type_addrAddr) ||
			(cons->type == duckVM_gclist_cons_type_objectAddr)) {
			e = duckVM_gclist_markCons(gclist, cons->cdr.addr);
		}
		else {
			e = duckVM_gclist_markObject(gclist, cons->cdr.data);
		}
	}
 cleanup:
	return e;
}

static dl_error_t duckVM_gclist_markUpvalue(duckVM_gclist_t *gclist, duckVM_upvalue_t *upvalue) {
	if (upvalue) {
		gclist->upvalueInUse[(dl_ptrdiff_t) (upvalue - gclist->upvalues)] = dl_true;
		if (upvalue->type == duckVM_upvalue_type_heap_object) {
			return duckVM_gclist_markObject(gclist, upvalue->value.heap_object);
		}
		else if (upvalue->type == duckVM_upvalue_type_heap_upvalue) {
			return duckVM_gclist_markUpvalue(gclist, upvalue->value.heap_upvalue);
		}
		/* else ignore, since the stack is the root of GC. Would cause a cycle (infinite loop) if we handled it. */
	}
	return dl_error_ok;
}

// @TODO: Check for cycles.
static dl_error_t duckVM_gclist_markUpvalueArray(duckVM_gclist_t *gclist, duckVM_upvalue_array_t *upvalueArray) {
	dl_error_t e = dl_error_ok;
	if (upvalueArray && !gclist->upvalueArraysInUse[(dl_ptrdiff_t) (upvalueArray - gclist->upvalueArrays)]) {
		gclist->upvalueArraysInUse[(dl_ptrdiff_t) (upvalueArray - gclist->upvalueArrays)] = dl_true;
		if (upvalueArray->initialized) {
			DL_DOTIMES(k, upvalueArray->length) {
				e = duckVM_gclist_markUpvalue(gclist, upvalueArray->upvalues[k]);
				if (e) break;
			}
		}
		/* else ignore, since the stack is the root of GC. Would cause a cycle (infinite loop) if we handled it. */
	}
	return e;
}

static dl_error_t duckVM_gclist_garbageCollect(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	/* Clear the in use flags. */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.upvalues_length; i++) {
		duckVM->gclist.upvalueInUse[i] = dl_false;
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.upvalueArrays_length; i++) {
		duckVM->gclist.upvalueArraysInUse[i] = dl_false;
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.conses_length; i++) {
		duckVM->gclist.consInUse[i] = dl_false;
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.objects_length; i++) {
		duckVM->gclist.objectInUse[i] = dl_false;
	}

	/* Mark the cells in use. */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->stack.elements_length; i++) {
		duckLisp_object_t *object = &DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, i);
		// Will need to check for closures now too. Can probably call markObject here instead of using this `if`.
		if (object->type == duckLisp_object_type_list) {
			e = duckVM_gclist_markCons(&duckVM->gclist, object->value.list);
			if (e) goto cleanup;
		}
		else if (object->type == duckLisp_object_type_closure) {
			e = duckVM_gclist_markUpvalueArray(&duckVM->gclist, object->value.closure.upvalue_array);
			if (e) break;
		}
	}

	/* Free cells if not marked. */

	duckVM->gclist.freeUpvalues_length = 0;  // This feels horribly inefficient. -- because it is. Google "fast garbage collector".
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.upvalues_length; i++) {
		if (!duckVM->gclist.upvalueInUse[i]) {
			duckVM->gclist.freeUpvalues[duckVM->gclist.freeUpvalues_length++] = &duckVM->gclist.upvalues[i];
		}
	}

	duckVM->gclist.freeUpvalueArrays_length = 0;  // This feels horribly inefficient.
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.upvalueArrays_length; i++) {
		if (!duckVM->gclist.upvalueArraysInUse[i]) {
			duckVM->gclist.freeUpvalueArrays[duckVM->gclist.freeUpvalueArrays_length++] = &duckVM->gclist.upvalueArrays[i];
			if (duckVM->gclist.upvalueArrays[i].initialized && (duckVM->gclist.upvalueArrays[i].upvalues != dl_null)) {
				e = dl_free(duckVM->memoryAllocation, (void **) &duckVM->gclist.upvalueArrays[i].upvalues);
				if (e) goto cleanup;
			}
		}
	}

	duckVM->gclist.freeConses_length = 0;  // This feels horribly inefficient.
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.conses_length; i++) {
		if (!duckVM->gclist.consInUse[i]) {
			duckVM->gclist.freeConses[duckVM->gclist.freeConses_length++] = &duckVM->gclist.conses[i];
		}
	}

	duckVM->gclist.freeObjects_length = 0;  // This feels horribly inefficient.
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.objects_length; i++) {
		if (!duckVM->gclist.objectInUse[i]) {
			duckVM->gclist.freeObjects[duckVM->gclist.freeObjects_length++] = &duckVM->gclist.objects[i];
		}
	}

 cleanup:
	return e;
}

/* These could have been macros in lisp. D: */
dl_error_t duckVM_gclist_pushUpvalue(duckVM_t *duckVM,
                                     duckVM_upvalue_t **upvalueOut,
                                     duckVM_upvalue_t upvalueIn) {
	dl_error_t e = dl_error_ok;

	duckVM_gclist_t *gclist = &duckVM->gclist;

	// Try once
	if (gclist->freeUpvalues_length == 0) {
		// STOP THE WORLD
		puts("pushUpvalue : Collect");
		e = duckVM_gclist_garbageCollect(duckVM);
		if (e) return e;

		// Try twice
		if (gclist->freeUpvalues_length == 0) {
			return dl_error_outOfMemory;
		}
	}

	duckVM_upvalue_t *upvalue = gclist->freeUpvalues[--gclist->freeUpvalues_length];
	*upvalue = upvalueIn;
	*upvalueOut = upvalue;

	return e;
}

dl_error_t duckVM_gclist_pushUpvalueArray(duckVM_t *duckVM,
                                          duckVM_upvalue_array_t **upvalueArrayOut,
                                          dl_size_t upvalues_length) {
	dl_error_t e = dl_error_ok;

	duckVM_gclist_t *gclist = &duckVM->gclist;

	// Try once
	if (gclist->freeUpvalueArrays_length == 0) {
		// STOP THE WORLD
		puts("pushUpvalueArray : Collect");
		e = duckVM_gclist_garbageCollect(duckVM);
		if (e) return e;

		// Try twice
		if (gclist->freeUpvalueArrays_length == 0) {
			return dl_error_outOfMemory;
		}
	}

	duckVM_upvalue_array_t *upvalueArray = gclist->freeUpvalueArrays[--gclist->freeUpvalueArrays_length];
	upvalueArray->initialized = dl_false;
	if (upvalues_length > 0) {
		e = DL_MALLOC(duckVM->memoryAllocation,
		              (void **) &upvalueArray->upvalues,
		              upvalues_length,
		              sizeof(duckVM_upvalue_t **));
		if (e) goto cleanup;
	}
	else {
		upvalueArray->upvalues = dl_null;
	}
	if (e) goto cleanup;
	upvalueArray->length = upvalues_length;
	*upvalueArrayOut = upvalueArray;

 cleanup:

	return e;
}

dl_error_t duckVM_gclist_pushCons(duckVM_t *duckVM, duckVM_gclist_cons_t **consOut, duckVM_gclist_cons_t consIn) {
	dl_error_t e = dl_error_ok;

	duckVM_gclist_t *gclist = &duckVM->gclist;

	// Try once
	if (gclist->freeConses_length == 0) {
		// STOP THE WORLD
		puts("pushCons : Collect");
		e = duckVM_gclist_garbageCollect(duckVM);
		if (e) return e;

		// Try twice
		if (gclist->freeConses_length == 0) {
			return dl_error_outOfMemory;
		}
	}

	*gclist->freeConses[--gclist->freeConses_length] = consIn;
	*consOut = gclist->freeConses[gclist->freeConses_length];

	return e;
}

dl_error_t duckVM_gclist_pushObject(duckVM_t *duckVM, duckLisp_object_t **objectOut, duckLisp_object_t objectIn) {
	dl_error_t e = dl_error_ok;

	duckVM_gclist_t *gclist = &duckVM->gclist;

	// Try once
	if (gclist->freeObjects_length == 0) {
		// STOP THE WORLD
		puts("pushObject: Collect");
		e = duckVM_gclist_garbageCollect(duckVM);
		if (e) return e;

		// Try twice
		if (gclist->freeObjects_length == 0) {
			return dl_error_outOfMemory;
		}
	}

	*gclist->freeObjects[--gclist->freeObjects_length] = objectIn;
	*objectOut = gclist->freeObjects[gclist->freeObjects_length];

	return e;
}


dl_error_t duckVM_init(duckVM_t *duckVM, dl_size_t maxUpvalues, dl_size_t maxUpvalueArrays, dl_size_t maxConses, dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	/**/ dl_array_init(&duckVM->errors, duckVM->memoryAllocation, sizeof(duckLisp_error_t), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->stack, duckVM->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->call_stack, duckVM->memoryAllocation, sizeof(unsigned char *), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->upvalue_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(duckVM_upvalue_t *),
	                   dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->upvalue_array_call_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(duckVM_upvalue_t **),
	                   dl_array_strategy_fit);
	e = dl_array_pushElement(&duckVM->upvalue_array_call_stack, dl_null);
	if (e) goto l_cleanup;
	/**/ dl_array_init(&duckVM->upvalue_array_length_call_stack,
	                   duckVM->memoryAllocation,
	                   sizeof(dl_size_t),
	                   dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->statics, duckVM->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_fit);
	e = duckVM_gclist_init(&duckVM->gclist, duckVM->memoryAllocation, maxUpvalues, maxUpvalueArrays, maxConses, maxObjects);
	if (e) goto l_cleanup;

	l_cleanup:

	return e;
}

void duckVM_quit(duckVM_t *duckVM) {
	dl_error_t e;
	e = dl_array_quit(&duckVM->stack);
	e = duckVM_gclist_garbageCollect(duckVM);
	/* No need for this because the object that the array came from is still on the stack. */
	/* DL_DOTIMES(i, duckVM->upvalue_array_length_call_stack.elements_length) { */
	/* 	duckLisp_object_t object = DL_ARRAY_GETADDRESS(duckVM->upvalue_array_length_call_stack, duckLisp_object_t, i); */
	/* 	if ((duckVM->gclist.objects[i].type == duckLisp_object_type_closure) */
	/* 	    && (object.value.closure.upvalue_array->upvalues != dl_null)) { */
	/* 		printf("duckVM_quit uv-stack closure %p\n", (void *) object.value.closure.upvalue_array->upvalues); */
	/* 		e = dl_free(duckVM->memoryAllocation, &object.value.closure.upvalue_array->upvalues); */
	/* 	} */
	/* } */
	/* DL_DOTIMES(i, duckVM->upvalue_array_call_stack.elements_length) { */
	/* 	e = dl_free(duckVM->memoryAllocation, */
	/* 	            &DL_ARRAY_GETADDRESS(duckVM->upvalue_array_call_stack, duckVM_upvalue_t**, i)); */
	/* } */
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
                                  duckVM_upvalue_t **upvalues,
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
	dl_size_t size1;
	dl_uint8_t uint8;
	duckLisp_object_t object1 = {0};
	duckLisp_object_t object2 = {0};
	duckLisp_object_t *objectPtr1;
	duckLisp_object_t *objectPtr2;
	/**/ dl_memclear(&object1, sizeof(duckLisp_object_t));
	duckVM_gclist_cons_t cons1;
	dl_bool_t bool1;
	dl_bool_t parsedBytecode;
	dl_uint8_t opcode;

	do {
		ptrdiff1 = 0;
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
			object1.value.string.value_length = *(ip)++;
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
				duckVM_upvalue_t *upvalue = DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
				                                                   duckVM_upvalue_t **)[ptrdiff1];
				if (upvalue->type == duckVM_upvalue_type_stack_index) {
					e = dl_array_get(&duckVM->stack, &object1, upvalue->value.stack_index);
					if (e) break;
				}
				else if (upvalue->type == duckVM_upvalue_type_heap_object) {
					object1 = *upvalue->value.heap_object;
				}
				else {
					while (upvalue->type == duckVM_upvalue_type_heap_upvalue) {
						upvalue = upvalue->value.heap_upvalue;
					}
					if (upvalue->type == duckVM_upvalue_type_stack_index) {
						e = dl_array_get(&duckVM->stack, &object1, upvalue->value.stack_index);
						if (e) break;
					}
					else {
						object1 = *upvalue->value.heap_object;
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

			e = duckVM_gclist_pushUpvalueArray(duckVM, &object1.value.closure.upvalue_array, size1);
			if (e) break;
			/* Immediately push on stack so that the GC can see it. Allocating upvalues could trigger a GC. */
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;

			dl_bool_t recursive = dl_false;

			DL_DOTIMES(k, object1.value.closure.upvalue_array->length) {
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
				duckVM_upvalue_t *upvalue_pointer = dl_null;
				if ((dl_size_t) ptrdiff1 == (duckVM->upvalue_stack.elements_length - 1)) {
					/* *Recursion* Capture upvalue we are about to push. */
					recursive = dl_true;
					/* Our upvalue definitely doesn't exist yet. */
					duckVM_upvalue_t upvalue;
					upvalue.type = duckVM_upvalue_type_stack_index;  /* Lie for now. */
					upvalue.value.stack_index = ptrdiff1;
					e = duckVM_gclist_pushUpvalue(duckVM, &upvalue_pointer, upvalue);
					if (e) break;
					/* Break push_scope abstraction. */
					e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
					/* Upvalue stack slot exists now, so insert the new UV. */
					DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_upvalue_t *, ptrdiff1) = upvalue_pointer;
				}
				else if (ptrdiff1 < 0) {
					/* Capture upvalue in currently executing function. */
					ptrdiff1 = -(ptrdiff1 + 1);
					duckVM_upvalue_t upvalue;
					upvalue.type = duckVM_upvalue_type_heap_upvalue;
					/* Link new upvalue to upvalue from current context. */
					upvalue.value.heap_upvalue = DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
					                                                    duckVM_upvalue_t **)[ptrdiff1];
					e = duckVM_gclist_pushUpvalue(duckVM, &upvalue_pointer, upvalue);
					if (e) break;
				}
				else {
					/* Capture upvalue on stack. */
					upvalue_pointer = DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_upvalue_t *, ptrdiff1);
					if (upvalue_pointer == dl_null) {
						duckVM_upvalue_t upvalue;
						upvalue.type = duckVM_upvalue_type_stack_index;
						upvalue.value.stack_index = ptrdiff1;
						e = duckVM_gclist_pushUpvalue(duckVM, &upvalue_pointer, upvalue);
						if (e) break;
						// Is this a bug? I'm not confident that this element exists.
						/* Keep reference to upvalue on stack so that we know where to release the object to. */
						DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_upvalue_t *, ptrdiff1) = upvalue_pointer;
					}
				}
				object1.value.closure.upvalue_array->upvalues[k] = upvalue_pointer;
			}
			if (!recursive) {
				/* Break push_scope abstraction. */
				e = dl_array_pushElement(&duckVM->upvalue_stack, dl_null);
				if (e) break;
			}
			/* e = stack_push(duckVM, &object1); */
			object1.value.closure.upvalue_array->initialized = dl_true;
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
				duckVM_upvalue_t *upvalue = DL_ARRAY_GETADDRESS(duckVM->upvalue_stack, duckVM_upvalue_t *, ptrdiff1);
				if (upvalue != dl_null) {
					duckLisp_object_t *object = &DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, ptrdiff1);
					e = duckVM_gclist_pushObject(duckVM, &upvalue->value.heap_object, *object);
					if (e) break;
					upvalue->type = duckVM_upvalue_type_heap_object;
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
				duckVM_upvalue_t *upvalue = DL_ARRAY_GETTOPADDRESS(duckVM->upvalue_array_call_stack,
				                                                   duckVM_upvalue_t **)[ptrdiff1];
				if (upvalue->type == duckVM_upvalue_type_stack_index) {
					e = dl_array_set(&duckVM->stack, &object1, upvalue->value.stack_index);
					if (e) break;
				}
				else if (upvalue->type == duckVM_upvalue_type_heap_object) {
					*upvalue->value.heap_object = object1;
				}
				else {
					while (upvalue->type == duckVM_upvalue_type_heap_upvalue) {
						upvalue = upvalue->value.heap_upvalue;
					}
					if (upvalue->type == duckVM_upvalue_type_stack_index) {
						e = dl_array_set(&duckVM->stack, &object1, upvalue->value.stack_index);
						if (e) break;
					}
					else {
						*upvalue->value.heap_object = object1;
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
				duckVM_gclist_cons_t *lastConsPtr = dl_null;
				DL_DOTIMES(k, (dl_size_t) (uint8 - object1.value.closure.arity)) {
					duckVM_gclist_cons_t cons;
					duckVM_gclist_cons_t *consPtr = dl_null;
					duckLisp_object_t object;
					duckLisp_object_t *objectPtr = dl_null;
					e = stack_pop(duckVM, &object);
					if (e) break;
					/* object = DL_ARRAY_GETADDRESS(duckVM->stack, */
					/*                              duckLisp_object_t, */
					/*                              ((duckVM->stack.elements_length - 1) - k)); */
					e = duckVM_gclist_pushObject(duckVM, &objectPtr, object);
					if (e) break;
					cons.car.data = objectPtr;
					cons.cdr.addr = lastConsPtr;
					cons.type = duckVM_gclist_cons_type_objectAddr;
					e = duckVM_gclist_pushCons(duckVM, &consPtr, cons);
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
			                    object1.value.closure.upvalue_array->upvalues,
			                    object1.value.closure.upvalue_array->length);
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
				if ((rest.value.list->type == duckVM_gclist_cons_type_objectObject)
				    || rest.value.list->type == duckVM_gclist_cons_type_addrObject) {
					e = dl_error_invalidValue;
					break;
				}
				/* (push (car rest) stack) */
				if (rest.value.list->type == duckVM_gclist_cons_type_addrAddr) {
					duckLisp_object_t object;
					object.type = duckLisp_object_type_list;
					object.value.list = rest.value.list->car.addr;
					e = stack_push(duckVM, &object);
				}
				else {
					e = stack_push(duckVM, rest.value.list->car.data);
				}
				if (e) break;
				/* (setf rest (cdr rest)) */
				rest.value.list = rest.value.list->cdr.addr;
				uint8++;
			}
			if (e) break;
			if (object1.value.closure.variadic) {
				if (uint8 < object1.value.closure.arity) {
					e = dl_error_invalidValue;
					break;
				}
				/* Create list. */
				duckVM_gclist_cons_t *lastConsPtr = rest.value.list;
				DL_DOTIMES(k, (dl_size_t) (uint8 - object1.value.closure.arity)) {
					duckVM_gclist_cons_t cons;
					duckVM_gclist_cons_t *consPtr = dl_null;
					duckLisp_object_t object;
					duckLisp_object_t *objectPtr = dl_null;
					e = stack_pop(duckVM, &object);
					if (e) break;
					/* object = DL_ARRAY_GETADDRESS(duckVM->stack, */
					/*                              duckLisp_object_t, */
					/*                              ((duckVM->stack.elements_length - 1) - k)); */
					e = duckVM_gclist_pushObject(duckVM, &objectPtr, object);
					if (e) break;
					cons.car.data = objectPtr;
					cons.cdr.addr = lastConsPtr;
					cons.type = duckVM_gclist_cons_type_objectAddr;
					e = duckVM_gclist_pushCons(duckVM, &consPtr, cons);
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
			                    object1.value.closure.upvalue_array->upvalues,
			                    object1.value.closure.upvalue_array->length);
			if (e) break;
			ip = &bytecode[object1.value.closure.name];
			break;

		/* I probably don't need an `if` if I research the standard a bit. */
		case duckLisp_instruction_call32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			call_stack_push(duckVM, ip, dl_null, 0);
			if (e) break;
			if (ptrdiff1 & 0x80000000ULL) {
				ip -= ((~((dl_size_t) ptrdiff1) + 1) & 0xFFFFFFFFULL);
			}
			else {
				ip += ptrdiff1;
			}
			--ip;
			break;
		case duckLisp_instruction_call16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			call_stack_push(duckVM, ip, dl_null, 0);
			if (e) break;
			if (ptrdiff1 & 0x8000ULL) {
				ip -= ((~ptrdiff1 + 1) & 0xFFFFULL);
			}
			else {
				ip += ptrdiff1;
			}
			--ip;
			break;
		case duckLisp_instruction_call8:
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
			call_stack_push(duckVM, ip, dl_null, 0);
			if (e) break;
			if (ptrdiff1 & 0x80ULL) {
				ip -= ((~ptrdiff1 + 1) & 0xFFULL);
			}
			else {
				ip += ptrdiff1;
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
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - 1);
			if (e) break;
			ptrdiff2 = *(ip++);
			e = stack_pop_multiple(duckVM, ptrdiff2);
			if (e) break;
			if (object1.type != duckLisp_object_type_bool) {
				e = dl_error_invalidValue;
				break;
			}
			if (object1.value.boolean) {
				if (ptrdiff1 & 0x80000000ULL) {
					ip -= ((~ptrdiff1 + 1) & 0xFFFFFFFFULL);
				}
				else {
					ip += ptrdiff1;
				}
				--ip; // This accounts for the pop argument.
			}
			break;
		case duckLisp_instruction_brnz16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - 1);
			if (e) break;
			ptrdiff2 = *(ip++);
			e = stack_pop_multiple(duckVM, ptrdiff2);
			if (e) break;
			if (object1.type != duckLisp_object_type_bool) {
				e = dl_error_invalidValue;
				break;
			}
			if (object1.value.boolean) {
				if (ptrdiff1 & 0x8000ULL) {
					ip -= ((~ptrdiff1 + 1) & 0xFFFFULL);
				}
				else {
					ip += ptrdiff1;
				}
				--ip; // This accounts for the pop argument.
			}
			break;
		case duckLisp_instruction_brnz8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - 1);
			if (e) break;
			ptrdiff2 = *(ip++);
			e = stack_pop_multiple(duckVM, ptrdiff2);
			if (e) break;
			if (object1.type != duckLisp_object_type_bool) {
				e = dl_error_invalidValue;
				break;
			}
			if (object1.value.boolean) {
				if (ptrdiff1 & 0x80ULL) {
					ip -= ((~ptrdiff1 + 1) & 0xFFULL);
				}
				else {
					ip += ptrdiff1;
				}
				--ip; // This accounts for the pop argument.
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
			if (object1.type == duckLisp_object_type_list) {
				cons1.car.addr = object1.value.list;
			}
			else {
				e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
				if (e) break;
				cons1.car.data = objectPtr1;
			}
			if (object2.type == duckLisp_object_type_list) {
				cons1.cdr.addr = object2.value.list;
			}
			else {
				e = duckVM_gclist_pushObject(duckVM, &objectPtr2, object2);
				if (e) break;
				cons1.cdr.data = objectPtr2;
			}
			// This is stupid.
			if (object1.type == duckLisp_object_type_list) {
				if (object2.type == duckLisp_object_type_list) cons1.type = duckVM_gclist_cons_type_addrAddr;
				else cons1.type = duckVM_gclist_cons_type_addrObject;
			}
			else {
				if (object2.type == duckLisp_object_type_list) cons1.type = duckVM_gclist_cons_type_objectAddr;
				else cons1.type = duckVM_gclist_cons_type_objectObject;
			}
			e = duckVM_gclist_pushCons(duckVM, &object1.value.list, cons1);
			if (e) break;

			object1.type = duckLisp_object_type_list;
			e = stack_push(duckVM, &object1);
			if (e) break;
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_cdr32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			if (object1.value.list == dl_null) {
				object2.type = duckLisp_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				switch (object1.value.list->type) {
				case duckVM_gclist_cons_type_addrAddr:
					// Fall through
				case duckVM_gclist_cons_type_objectAddr:
					object2.type = duckLisp_object_type_list;
					object2.value.list = object1.value.list->cdr.addr;
					break;
				case duckVM_gclist_cons_type_addrObject:
					// Fall through
				case duckVM_gclist_cons_type_objectObject:
					object2 = *(object1.value.list->cdr.data);
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_cdr16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			if (object1.value.list == dl_null) {
				object2.type = duckLisp_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				switch (object1.value.list->type) {
				case duckVM_gclist_cons_type_addrAddr:
					// Fall through
				case duckVM_gclist_cons_type_objectAddr:
					object2.type = duckLisp_object_type_list;
					object2.value.list = object1.value.list->cdr.addr;
					break;
				case duckVM_gclist_cons_type_addrObject:
					// Fall through
				case duckVM_gclist_cons_type_objectObject:
					object2 = *(object1.value.list->cdr.data);
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_cdr8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			if (object1.value.list == dl_null) {
				object2.type = duckLisp_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				switch (object1.value.list->type) {
				case duckVM_gclist_cons_type_addrAddr:
					// Fall through
				case duckVM_gclist_cons_type_objectAddr:
					object2.type = duckLisp_object_type_list;
					object2.value.list = object1.value.list->cdr.addr;
					break;
				case duckVM_gclist_cons_type_addrObject:
					// Fall through
				case duckVM_gclist_cons_type_objectObject:
					object2 = *(object1.value.list->cdr.data);
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_car32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			if (object1.value.list == dl_null) {
				object2.type = duckLisp_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				switch (object1.value.list->type) {
				case duckVM_gclist_cons_type_addrAddr:
					// Fall through
				case duckVM_gclist_cons_type_addrObject:
					object2.type = duckLisp_object_type_list;
					object2.value.list = object1.value.list->car.addr;
					break;
				case duckVM_gclist_cons_type_objectAddr:
					// Fall through
				case duckVM_gclist_cons_type_objectObject:
					object2 = *(object1.value.list->car.data);
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_car16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			if (object1.value.list == dl_null) {
				object2.type = duckLisp_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				switch (object1.value.list->type) {
				case duckVM_gclist_cons_type_addrAddr:
					// Fall through
				case duckVM_gclist_cons_type_addrObject:
					object2.type = duckLisp_object_type_list;
					object2.value.list = object1.value.list->car.addr;
					break;
				case duckVM_gclist_cons_type_objectAddr:
					// Fall through
				case duckVM_gclist_cons_type_objectObject:
					object2 = *(object1.value.list->car.data);
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
			}
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_car8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			if (object1.value.list == dl_null) {
				object2.type = duckLisp_object_type_list;
				object2.value.list = dl_null;
			}
			else {
				switch (object1.value.list->type) {
				case duckVM_gclist_cons_type_addrAddr:
					// Fall through
				case duckVM_gclist_cons_type_addrObject:
					object2.type = duckLisp_object_type_list;
					object2.value.list = object1.value.list->car.addr;
					break;
				case duckVM_gclist_cons_type_objectAddr:
					// Fall through
				case duckVM_gclist_cons_type_objectObject:
					object2 = *(object1.value.list->car.data);
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
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

			if (object2.type == duckLisp_object_type_list) {
				if (object1.type == duckLisp_object_type_list) {
					if (object1.value.list == dl_null) object2.value.list->car.addr = dl_null;
					else object2.value.list->car.addr = object1.value.list;
					switch (object2.value.list->type) {
					case duckVM_gclist_cons_type_objectAddr:
						object2.value.list->type = duckVM_gclist_cons_type_addrAddr;
						break;
					case duckVM_gclist_cons_type_objectObject:
						object2.value.list->type = duckVM_gclist_cons_type_addrObject;
						break;
						/* Already correct. */
					case duckVM_gclist_cons_type_addrAddr:
						break;
					case duckVM_gclist_cons_type_addrObject:
						break;
					default:
						e = dl_error_invalidValue;
						goto l_cleanup;
					}
				}
				else {
					e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
					if (e) break;
					object2.value.list->car.data = objectPtr1;
					switch (object2.value.list->type) {
					case duckVM_gclist_cons_type_addrAddr:
						object2.value.list->type = duckVM_gclist_cons_type_objectAddr;
						break;
					case duckVM_gclist_cons_type_addrObject:
						object2.value.list->type = duckVM_gclist_cons_type_objectObject;
						break;
						/* Already correct. */
					case duckVM_gclist_cons_type_objectAddr:
						break;
					case duckVM_gclist_cons_type_objectObject:
						break;
					default:
						e = dl_error_invalidValue;
						goto l_cleanup;
					}
				}
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

			if (object2.type == duckLisp_object_type_list) {
				if (object1.type == duckLisp_object_type_list) {
					if (object1.value.list == dl_null) object2.value.list->cdr.addr = dl_null;
					else object2.value.list->cdr.addr = object1.value.list;
					switch (object2.value.list->type) {
					case duckVM_gclist_cons_type_addrObject:
						object2.value.list->type = duckVM_gclist_cons_type_addrAddr;
						break;
					case duckVM_gclist_cons_type_objectObject:
						object2.value.list->type = duckVM_gclist_cons_type_objectAddr;
						break;
						/* Already correct. */
					case duckVM_gclist_cons_type_addrAddr:
						break;
					case duckVM_gclist_cons_type_objectAddr:
						break;
					default:
						e = dl_error_invalidValue;
						goto l_cleanup;
					}
				}
				else {
					e = duckVM_gclist_pushObject(duckVM, &objectPtr1, object1);
					if (e) break;
					object2.value.list->cdr.data = objectPtr1;
					switch (object2.value.list->type) {
					case duckVM_gclist_cons_type_addrAddr:
						object2.value.list->type = duckVM_gclist_cons_type_addrObject;
						break;
					case duckVM_gclist_cons_type_objectAddr:
						object2.value.list->type = duckVM_gclist_cons_type_objectObject;
						break;
						/* Already correct. */
					case duckVM_gclist_cons_type_addrObject:
						break;
					case duckVM_gclist_cons_type_objectObject:
						break;
					default:
						e = dl_error_invalidValue;
						goto l_cleanup;
					}
				}
			}
			else e = dl_error_invalidValue;
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_nullp32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object2.type = duckLisp_object_type_bool;
			object2.value.boolean = (object1.value.list == dl_null);
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_nullp16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object2.type = duckLisp_object_type_bool;
			object2.value.boolean = (object1.value.list == dl_null);
			e = stack_push(duckVM, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_nullp8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
				object2.type = duckLisp_object_type_bool;
			if (object1.type != duckLisp_object_type_list) {
				object2.value.boolean = dl_false;
			}
			else {
				object2.value.boolean = (object1.value.list == dl_null);
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

