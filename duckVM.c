
#include "duckVM.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "duckLisp.h"
#include <stdio.h>

dl_error_t duckVM_gclist_init(duckVM_gclist_t *gclist, dl_memoryAllocation_t *memoryAllocation, dl_size_t maxConses, dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	gclist->memoryAllocation = memoryAllocation;


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
				  (void **) &gclist->consInUse,
				  maxConses * sizeof(dl_bool_t));
	if (e) goto cleanup;

	e = dl_malloc(gclist->memoryAllocation,
				  (void **) &gclist->objectInUse,
				  maxObjects * sizeof(dl_bool_t));
	if (e) goto cleanup;


	/**/ dl_memclear(gclist->conses, gclist->conses_length * sizeof(duckVM_gclist_cons_t));
	/**/ dl_memclear(gclist->objects, gclist->objects_length * sizeof(duckLisp_object_t));


	for (dl_ptrdiff_t i = 0; (dl_size_t) i < maxConses; i++) {
		gclist->freeConses[i] = &gclist->conses[i];
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < maxObjects; i++) {
		gclist->freeObjects[i] = &gclist->objects[i];
	}

 cleanup:

	return e;
}

dl_error_t duckVM_gclist_quit(duckVM_gclist_t *gclist) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	e = dl_free(gclist->memoryAllocation, (void **) &gclist->freeConses);
	gclist->freeConses_length = 0;
	
	e = dl_free(gclist->memoryAllocation, (void **) &gclist->freeObjects);
	gclist->freeObjects_length = 0;


	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->conses);
	e = eError ? eError : e;
	gclist->conses_length = 0;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->objects);
	e = eError ? eError : e;
	gclist->objects_length = 0;


	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->consInUse);
	e = eError ? eError : e;

	eError = dl_free(gclist->memoryAllocation, (void **) &gclist->objectInUse);
	e = eError ? eError : e;
	
	return e;
}

void duckVM_gclist_markObject(duckVM_gclist_t *gclist, duckLisp_object_t *object) {
	if (object) {
		gclist->objectInUse[(dl_ptrdiff_t) (object - gclist->objects)] = dl_true;
	}
}

dl_error_t duckVM_gclist_markCons(duckVM_gclist_t *gclist, duckVM_gclist_cons_t *cons) {
	dl_error_t e = dl_error_ok;

	if (cons) {
		gclist->consInUse[(dl_ptrdiff_t) (cons - gclist->conses)] = dl_true;
		if ((cons->type == duckVM_gclist_cons_type_addrAddr) ||
			(cons->type == duckVM_gclist_cons_type_addrObject)) {
			e = duckVM_gclist_markCons(gclist, cons->car.addr);
			if (e) goto cleanup;
		}
		else {
			duckVM_gclist_markObject(gclist, cons->car.data);
		}
		if ((cons->type == duckVM_gclist_cons_type_addrAddr) ||
			(cons->type == duckVM_gclist_cons_type_objectAddr)) {
			e = duckVM_gclist_markCons(gclist, cons->cdr.addr);
			if (e) goto cleanup;
		}
		else {
			duckVM_gclist_markObject(gclist, cons->cdr.data);
		}
	}

 cleanup:

	return e;
}

dl_error_t duckVM_gclist_garbageCollect(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	/* Clear the in use flags. */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.conses_length; i++) {
		duckVM->gclist.consInUse[i] = dl_false;
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->gclist.objects_length; i++) {
		duckVM->gclist.objectInUse[i] = dl_false;
	}

	/* Mark the cells in use. */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckVM->stack.elements_length; i++) {
		duckLisp_object_t *object = &DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, i);
		if (object->type == duckLisp_object_type_list) {
			e = duckVM_gclist_markCons(&duckVM->gclist, object->value.list);
			if (e) goto cleanup;
		}
	}

	/* Free cells if not marked. */

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


dl_error_t duckVM_init(duckVM_t *duckVM, dl_size_t maxConses, dl_size_t maxObjects) {
	dl_error_t e = dl_error_ok;

	/**/ dl_array_init(&duckVM->errors, duckVM->memoryAllocation, sizeof(duckLisp_error_t), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->stack, duckVM->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->call_stack, duckVM->memoryAllocation, sizeof(unsigned char *), dl_array_strategy_fit);
	/**/ dl_array_init(&duckVM->statics, duckVM->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_fit);
	e = duckVM_gclist_init(&duckVM->gclist, duckVM->memoryAllocation, maxConses, maxObjects);
	if (e) goto l_cleanup;

	l_cleanup:

	return e;
}

void duckVM_quit(duckVM_t *duckVM) {
	/**/ duckVM_gclist_quit(&duckVM->gclist);
	/**/ dl_memclear(&duckVM->errors, sizeof(dl_array_t));
	/**/ dl_memclear(&duckVM->statics, sizeof(dl_array_t));
	/**/ dl_memclear(&duckVM->stack, sizeof(dl_array_t));
	/**/ dl_memclear(&duckVM->call_stack, sizeof(dl_array_t));
}

dl_error_t duckVM_execute(duckVM_t *duckVM, unsigned char *bytecode) {
	dl_error_t e = dl_error_ok;
	
	dl_uint8_t *ip = bytecode;
	dl_ptrdiff_t ptrdiff1;
	dl_ptrdiff_t ptrdiff2;
	// dl_size_t length1;
	duckLisp_object_t object1 = {0};
	duckLisp_object_t object2 = {0};
	duckLisp_object_t *objectPtr1;
	duckLisp_object_t *objectPtr2;
	/**/ dl_memclear(&object1, sizeof(duckLisp_object_t));
	duckVM_gclist_cons_t cons1;
	dl_bool_t bool1;
	
	do {
		ptrdiff1 = 0;
		/**/ dl_memclear(&object1, sizeof(duckLisp_object_t));
		switch (*(ip++)) {
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;
		case duckLisp_instruction_pushSymbol16:
			object1.value.symbol.id = *(ip++);
			object1.value.symbol.id = *(ip++) + (object1.value.symbol.id << 8);
			object1.value.symbol.value_length = *(ip++);
			object1.value.symbol.value_length = *(ip++) + (object1.value.symbol.value_length << 8);
			object1.value.symbol.value = (char *) ip;
			ip += object1.value.symbol.value_length;
			object1.type = duckLisp_object_type_symbol;
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;
		case duckLisp_instruction_pushSymbol8:
			object1.value.symbol.id = *(ip++);
			object1.value.symbol.value_length = *(ip++);
			object1.value.symbol.value = (char *) ip;
			ip += object1.value.symbol.value_length;
			object1.type = duckLisp_object_type_symbol;
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		case duckLisp_instruction_pushBooleanFalse:
			object1.type = duckLisp_object_type_bool;
			object1.value.integer = dl_false;
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;
		case duckLisp_instruction_pushBooleanTrue:
			object1.type = duckLisp_object_type_bool;
			object1.value.integer = dl_true;
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		case duckLisp_instruction_pushInteger32:
			object1.value.integer = *(ip++);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.type = duckLisp_object_type_integer;
			object1.value.integer = (object1.value.integer > 0x7FFFFFFF) ? -(0x100000000 - object1.value.integer) : object1.value.integer;
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;
		case duckLisp_instruction_pushInteger16:
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.type = duckLisp_object_type_integer;
			object1.value.integer = (object1.value.integer > 0x7FFF) ? -(0x10000 - object1.value.integer) : object1.value.integer;
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;
		case duckLisp_instruction_pushInteger8:
			object1.value.integer = *(ip++) + (object1.value.integer << 8);
			object1.type = duckLisp_object_type_integer;
			object1.value.integer = (object1.value.integer > 0x7F) ? -(0x100 - object1.value.integer) : object1.value.integer;
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			if (e) {
				break;
			}
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_call32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			e = dl_array_pushElement(&duckVM->call_stack, &ip);
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
			e = dl_array_pushElement(&duckVM->call_stack, &ip);
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
			e = dl_array_pushElement(&duckVM->call_stack, &ip);
			if (e) break;
			if (ptrdiff1 & 0x80ULL) {
				ip -= ((~ptrdiff1 + 1) & 0xFFULL);
			}
			else {
				ip += ptrdiff1;
			}
			--ip;
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_acall32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_pushElement(&duckVM->call_stack, &ip);
			if (e) break;
			if (ptrdiff1 & 0x80000000ULL) {
				ip -= ((~((dl_size_t) ptrdiff1) + 1) & 0xFFFFFFFFULL);
			}
			else {
				ip += ptrdiff1;
			}
			--ip;
			break;
		case duckLisp_instruction_acall16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_pushElement(&duckVM->call_stack, &ip);
			if (e) break;
			if (ptrdiff1 & 0x8000ULL) {
				ip -= ((~ptrdiff1 + 1) & 0xFFFFULL);
			}
			else {
				ip += ptrdiff1;
			}
			--ip;
			break;
		case duckLisp_instruction_acall8:
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_integer) {
				e = dl_error_invalidValue;
				break;
			}
			e = dl_array_pushElement(&duckVM->call_stack, &ip);
			if (e) break;
			ip = &bytecode[object1.value.integer];
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff2);
			if (e) break;
			break;

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
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff2);
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
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff2);
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
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff2);
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
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff1);
			if (e) break;
			break;
		case duckLisp_instruction_pop16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff1);
			if (e) break;
			break;
		case duckLisp_instruction_pop8:
			ptrdiff1 = *(ip++);
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff1);
			if (e) break;
			break;
		
		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_move32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			e = dl_array_set(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff2);
			if (e) break;
			break;
		case duckLisp_instruction_move16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			e = dl_array_set(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff2);
			if (e) break;
			break;
		case duckLisp_instruction_move8:
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			e = dl_array_set(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff2);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_mul32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_mul16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_div32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_div16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_add32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_add16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_sub32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_sub16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_greater32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_greater16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_equal32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
					object1.value.boolean = object1.value.boolean == object2.value.boolean;
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_equal16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
					object1.value.boolean = object1.value.boolean == object2.value.boolean;
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;
		case duckLisp_instruction_equal8:
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_less32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_less16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			break;

		// I probably don't need an `if` if I research the standard a bit.
		case duckLisp_instruction_cons32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
				cons1.car.addr = object2.value.list;
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_cons16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff2 = *(ip++);
			ptrdiff2 = *(ip++) + (ptrdiff1 << 8);
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
				cons1.car.addr = object2.value.list;
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
			if (e) break;
			break;
		case duckLisp_instruction_cons8:
			ptrdiff1 = *(ip++);
			ptrdiff2 = *(ip++);
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
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
			if (e) break;
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_nullp8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			if (object1.type != duckLisp_object_type_list) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			object2.type = duckLisp_object_type_bool;
			object2.value.boolean = (object1.value.list == dl_null);
			e = dl_array_pushElement(&duckVM->stack, &object2);
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
			e = dl_array_pushElement(&duckVM->stack, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_typeof16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			object2.type = duckLisp_object_type_integer;
			object2.value.integer = object1.type;
			e = dl_array_pushElement(&duckVM->stack, &object2);
			if (e) break;
			break;
		case duckLisp_instruction_typeof8:
			ptrdiff1 = *(ip++);
			e = dl_array_get(&duckVM->stack, &object1, duckVM->stack.elements_length - ptrdiff1);
			if (e) break;
			object2.type = duckLisp_object_type_integer;
			object2.value.integer = object1.type;
			e = dl_array_pushElement(&duckVM->stack, &object2);
			if (e) break;
			break;

		case duckLisp_instruction_return32:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			if (duckVM->stack.elements_length > 0) {
				e = dl_array_getTop(&duckVM->stack, &object1);
				if (e) break;
			}
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff1);
			if (e) break;
			if (duckVM->stack.elements_length > 0) {
				e = dl_array_set(&duckVM->stack, &object1, duckVM->stack.elements_length - 1);
				if (e) break;
			}
			e = dl_array_popElement(&duckVM->call_stack, &ip);
			if (e == dl_error_bufferUnderflow) {
				e = dl_error_ok;
				goto l_cleanup;
			}
			break;
		case duckLisp_instruction_return16:
			ptrdiff1 = *(ip++);
			ptrdiff1 = *(ip++) + (ptrdiff1 << 8);
			if (duckVM->stack.elements_length > 0) {
				e = dl_array_getTop(&duckVM->stack, &object1);
				if (e) break;
			}
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff1);
			if (e) break;
			if (duckVM->stack.elements_length > 0) {
				e = dl_array_set(&duckVM->stack, &object1, duckVM->stack.elements_length - 1);
				if (e) break;
			}
			e = dl_array_popElement(&duckVM->call_stack, &ip);
			if (e == dl_error_bufferUnderflow) {
				e = dl_error_ok;
				goto l_cleanup;
			}
			break;
		case duckLisp_instruction_return8:
			ptrdiff1 = *(ip++);
			if (duckVM->stack.elements_length > 0) {
				e = dl_array_getTop(&duckVM->stack, &object1);
				if (e) break;
			}
			e = dl_array_popElements(&duckVM->stack, dl_null, ptrdiff1);
			if (e) break;
			if (duckVM->stack.elements_length > 0) {
				e = dl_array_set(&duckVM->stack, &object1, duckVM->stack.elements_length - 1);
				if (e) break;
			}
			e = dl_array_popElement(&duckVM->call_stack, &ip);
			if (e == dl_error_bufferUnderflow) {
				e = dl_error_ok;
				goto l_cleanup;
			}
			break;
		case duckLisp_instruction_return0:
			e = dl_array_popElement(&duckVM->call_stack, &ip);
			if (e == dl_error_bufferUnderflow) {
				e = dl_error_ok;
				goto l_cleanup;
			}
			break;

		case duckLisp_instruction_nil:
			object1.type = duckLisp_object_type_list;
			object1.value.list = dl_null;
			e = dl_array_pushElement(&duckVM->stack, &object1);
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
		printf("ip 0x%lX\n", ip - bytecode);
		printf("*ip 0x%X\n", *ip);
	}
	
	return e;
}

dl_error_t duckVM_callLocal(duckVM_t *duckVM, dl_ptrdiff_t function_index) {
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
		e = duckVM_execute(duckVM, functionObject.value.function.bytecode);
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
	return dl_array_popElement(&duckVM->stack, object);
}

dl_error_t duckVM_push(duckVM_t *duckVM, duckLisp_object_t *object) {
	return dl_array_pushElement(&duckVM->stack, object);
}

/* dl_error_t duckVM_pushReturn(duckVM_t *duckVM, duckLisp_object_t object) { */
/* 	dl_error_t e = dl_error_ok; */
	
/* 	e = dl_array_pushElement(&duckVM->stack, (void *) &object); */
/* 	duckVM->frame_pointer++; */
	
/* 	return e; */
/* } */

