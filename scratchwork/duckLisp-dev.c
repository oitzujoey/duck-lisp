/*
MIT License

Copyright (c) 2021 Joseph Herguth

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

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "../duckLisp.h"
#include "../duckVM.h"
#include "../DuckLib/sort.h"
#include "../parser.h"
#include "DuckLib/string.h"


#define COLOR_NORMAL    "\x1B[0m"
#define COLOR_BLACK     "\x1B[30m"
#define COLOR_RED       "\x1B[31m"
#define COLOR_GREEN     "\x1B[32m"
#define COLOR_YELLOW    "\x1B[33m"
#define COLOR_BLUE      "\x1B[34m"
#define COLOR_MAGENTA   "\x1B[35m"
#define COLOR_CYAN      "\x1B[36m"
#define COLOR_WHITE     "\x1B[37m"

#define B_COLOR_BLACK     "\x1B[40m"
#define B_COLOR_RED       "\x1B[41m"
#define B_COLOR_GREEN     "\x1B[42m"
#define B_COLOR_YELLOW    "\x1B[43m"
#define B_COLOR_BLUE      "\x1B[44m"
#define B_COLOR_MAGENTA   "\x1B[45m"
#define B_COLOR_CYAN      "\x1B[46m"
#define B_COLOR_WHITE     "\x1B[47m"

dl_bool_t g_disassemble = dl_false;
#ifdef USE_PARENTHESIS_INFERENCE
dl_bool_t g_hanabi = dl_true;
#endif /* USE_PARENTHESIS_INFERENCE */

typedef enum {
	duckLispDev_user_type_none,
	duckLispDev_user_type_file,
} duckLispDev_user_type_t;

typedef struct {
	union {
		FILE *file;
		duckVM_object_t *heapObject;
	} _;
	duckLispDev_user_type_t type;
} duckLispDev_user_t;

dl_error_t duckLispDev_callback_print(duckVM_t *duckVM);


/* This function begins with a cons object on the top of the stack. */
dl_error_t duckLispDev_callback_printCons(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	/* stack: cons */
	dl_bool_t nil;
	duckVM_object_type_t type;

	e = duckVM_isNil(duckVM, &nil);
	if (e) goto cleanup;
	if (nil) {
		/* stack: () */
		printf("nil");
	}
	else {
		/* stack: (car . cdr) */
		e = duckVM_pushFirst(duckVM);
		if (e) goto cleanup;
		/* stack: (car . cdr) car */
		e = duckVM_isNil(duckVM, &nil);
		if (e) goto cleanup;
		e = duckVM_typeOf(duckVM, &type);
		if (e) goto cleanup;
		if (nil) {
			/* stack: (() . cdr) () */
			printf("nil");
		}
		else if (type == duckVM_object_type_cons) {
			/* stack: ((caar . cdar) . cdr) (caar . cdar) */
			printf("(");
			e = duckLispDev_callback_printCons(duckVM);
			if (e) goto cleanup;
			/* stack: ((caar . cdar) . cdr) (caar . cdar) */
			printf(")");
		}
		else {
			/* stack: (car . cdr) car */
			e = duckLispDev_callback_print(duckVM);
			if (e) goto cleanup;
			/* stack: (car . cdr) car */
		}
		/* stack: (car . cdr) car */
		e = duckVM_pop(duckVM);
		if (e) goto cleanup;
		/* stack: (car . cdr) */


		/* stack: (car . cdr) */
		e = duckVM_pushRest(duckVM);
		if (e) goto cleanup;
		/* stack: (car . cdr) cdr */
		e = duckVM_isNil(duckVM, &nil);
		if (e) goto cleanup;
		e = duckVM_typeOf(duckVM, &type);
		if (e) goto cleanup;
		if (nil || (type == duckVM_object_type_list)) {
			/* stack: (car . cdr) cdr */
			if (!nil) {
				/* stack: (car . (cadr . cddr)) (cadr . cddr) */
				printf(" ");
				e = duckLispDev_callback_printCons(duckVM);
				if (e) goto cleanup;
				/* stack: (car . (cadr . cddr)) (cadr . cddr) */
			}
			/* stack: (car . cdr) cdr */
		}
		else {
			/* stack: (car . cdr) cdr */
			printf(" . ");
			e = duckLispDev_callback_print(duckVM);
			if (e) goto cleanup;
			/* stack: (car . cdr) cdr */
		}
		/* stack: (car . cdr) cdr */
		e = duckVM_pop(duckVM);
		if (e) goto cleanup;
		/* stack: (car . cdr) */
	}
	/* stack: cons */

 cleanup: return e;
}

dl_error_t duckLispDev_callback_print(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	/* stack: object */
	duckVM_object_type_t type;

	e = duckVM_typeOf(duckVM, &type);
	if (e) goto cleanup;

	switch (type) {
	case duckVM_object_type_symbol: {
		/* stack: symbol */
		dl_size_t id = 0;
		dl_uint8_t *string = dl_null;
		dl_size_t length = 0;
		e = duckVM_copySymbolName(duckVM, &string, &length);
		if (e) break;
		e = duckVM_copySymbolId(duckVM, &id);
		if (e) break;
		for (dl_size_t i = 0; i < length; i++) {
			putchar(string[i]);
		}
		e = DL_FREE(duckVM->memoryAllocation, &string);
		if (e) break;
		printf("→%lu", id);
	}
		break;
	case duckVM_object_type_string: {
		/* stack: string */
		dl_uint8_t *string = dl_null;
		dl_size_t length = 0;
		e = duckVM_copyString(duckVM, &string, &length);
		if (e) goto stringCleanup;
		for (dl_size_t i = 0; i < length; i++) {
			putchar(string[i]);
		}
		stringCleanup:
		e = DL_FREE(duckVM->memoryAllocation, &string);
		if (e) break;
	}
		break;
	case duckVM_object_type_integer: {
		/* stack: integer */
		dl_ptrdiff_t integer;
		e = duckVM_copySignedInteger(duckVM, &integer);
		if (e) break;
		printf("%li", integer);
		break;
	}
	case duckVM_object_type_float: {
		/* stack: float */
		double floatingPoint;
		e = duckVM_copyFloat(duckVM, &floatingPoint);
		if (e) break;
		printf("%f", floatingPoint);
		break;
	}
	case duckVM_object_type_bool: {
		/* stack: boolean */
		dl_bool_t boolean;
		e = duckVM_copyBoolean(duckVM, &boolean);
		if (e) break;
		printf("%s", boolean ? "true" : "false");
		break;
	}
	case duckVM_object_type_list: {
		/* stack: list */
		dl_bool_t nil;
		e = duckVM_isNil(duckVM, &nil);
		if (e) break;
		if (nil) {
			/* stack: () */
			printf("nil");
		}
		else {
			/* stack: (car cdr) */
			duckVM_object_type_t type;

			printf("(");

			e = duckVM_pushCar(duckVM);
			if (e) goto cleanup;
			/* stack: (car cdr) car */
			e = duckVM_isNil(duckVM, &nil);
			if (e) goto cleanup;
			e = duckVM_typeOf(duckVM, &type);
			if (e) goto cleanup;
			if (nil) {
				/* stack: (() cdr) () */
				printf("(nil)");
			}
			else if (type == duckVM_object_type_cons) {
				/* stack: ((caar cdar) cdr) (caar cdar) */
				printf("(");
				e = duckLispDev_callback_printCons(duckVM);
				if (e) goto cleanup;
				/* stack: ((caar cdar) cdr) (caar cdar) */
				printf(")");
			}
			else {
				/* stack: (car cdr) car */
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				/* stack: (car cdr) car */
			}
			/* stack: (car cdr) car */
			e = duckVM_pop(duckVM);
			if (e) goto cleanup;
			/* stack: (car cdr) */

			e = duckVM_pushCdr(duckVM);
			if (e) goto cleanup;
			/* stack: (car cdr) cdr */
			e = duckVM_isNil(duckVM, &nil);
			if (e) goto cleanup;
			e = duckVM_typeOf(duckVM, &type);
			if (e) goto cleanup;
			if (nil) {
				/* stack: (car ()) () */
			}
			else if (type == duckVM_object_type_list) {
				/* stack: (car cdr) cdr */
				if (!nil) {
					/* stack: (car (cadr cddr)) (cadr cddr) */
					printf(" ");
					e = duckLispDev_callback_printCons(duckVM);
					if (e) goto cleanup;
					/* stack: (car (cadr cddr)) (cadr cddr) */
				}
				/* stack: (car cdr) cdr */
			}
			else {
				/* stack: (car cdr) cdr */
				printf(" . ");
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				/* stack: (car cdr) cdr */
			}
			/* stack: (car cdr) cdr */
			e = duckVM_pop(duckVM);
			if (e) goto cleanup;
			/* stack: (car cdr) */

			printf(")");
		}
		/* stack: list */
	}
		break;
	case duckVM_object_type_closure: {
		/* stack: closure */
		dl_ptrdiff_t name;
		dl_uint8_t arity;
		dl_bool_t variadic;
		dl_size_t length;
		e = duckVM_copyClosureName(duckVM, &name);
		if (e) break;
		e = duckVM_copyClosureArity(duckVM, &arity);
		if (e) break;
		e = duckVM_copyClosureIsVariadic(duckVM, &variadic);
		if (e) break;
		e = duckVM_length(duckVM, &length);
		if (e) break;
		printf("(closure $%li #%u%s", name, arity, variadic ? "?" : "");
		if (e) break;
		DL_DOTIMES(k, length) {
			/* stack: closure */
			duckVM_object_type_t type;
			e = duckVM_typeOf(duckVM, &type);
			if (e) break;
			putchar(' ');
			if (type == duckVM_object_type_closure) {
				printf("#C");
			}
			else {
				e = duckVM_pushElement(duckVM, k);
				if (e) break;
				/* stack: closure closure[k] */
				e = duckLispDev_callback_print(duckVM);
				if (e) break;
				/* stack: closure closure[k] */
				e = duckVM_pop(duckVM);
				if (e) break;
			}
			/* stack: closure */
		}
		printf(")");
	}
		break;
	case duckVM_object_type_vector: {
		/* stack: vector */
		dl_size_t length;
		dl_bool_t empty;
		e = duckVM_length(duckVM, &length);
		if (e) goto cleanup;
		printf("[");
		e = duckVM_isEmpty(duckVM, &empty);
		if (e) goto cleanup;
		if (!empty) {
			/* stack: vector */
			DL_DOTIMES(k, length) {
				/* stack: vector */
				if (k != 0) putchar(' ');
				e = duckVM_pushElement(duckVM, k);
				if (e) goto cleanup;
				/* stack: vector vector[k] */
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				/* stack: vector vector[k] */
				e = duckVM_pop(duckVM);
				if (e) goto cleanup;
				/* stack: vector */
			}
			/* stack: vector */
		}
		/* stack: vector */
		printf("]");
	}
		break;
	case duckVM_object_type_type: {
		/* stack: type */
		dl_size_t type;
		e = duckVM_copyType(duckVM, &type);
		printf("::%lu", type);
		break;
	}
	case duckVM_object_type_composite: {
		/* stack: composite */
		dl_size_t type;
		e = duckVM_copyCompositeType(duckVM, &type);
		if (e) goto cleanup;
		printf("(make-instance ::%lu ", type);
		e = duckVM_pushCompositeValue(duckVM);
		if (e) goto cleanup;
		/* stack: composite value */
		e = duckLispDev_callback_print(duckVM);
		if (e) goto cleanup;
		/* stack: composite value */
		e = duckVM_pop(duckVM);
		if (e) goto cleanup;
		/* stack: composite */
		printf(" ");
		e = duckVM_pushCompositeFunction(duckVM);
		if (e) goto cleanup;
		/* stack: composite function */
		e = duckLispDev_callback_print(duckVM);
		if (e) goto cleanup;
		/* stack: composite function */
		e = duckVM_pop(duckVM);
		if (e) goto cleanup;
		/* stack: composite */
		printf(")");
		break;
	}
	default: {
		/* stack: object */
		duckVM_object_type_t type;
		e = duckVM_typeOf(duckVM, &type);
		if (e) break;
		printf("print: Unsupported type. [%u]\n", type);
	}
	}
	if (e) goto cleanup;

 cleanup:
	fflush(stdout);
	return e;
}

dl_error_t duckLispDev_callback_printStack(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	const dl_size_t stack_length = duckVM->stack.elements_length;
	DL_DOTIMES(i, stack_length) {
		/* stack: _ */
		e = duckVM_push(duckVM, i);
		if (e) goto cleanup;
		/* stack: object */
		printf("%li: ", i);
		e = duckLispDev_callback_print(duckVM);
		if (e) goto cleanup;
		putchar('\n');
		/* stack: object */
		e = duckVM_pop(duckVM);
		if (e) goto cleanup;
		/* stack: _ */
	}

	e = duckVM_pushNil(duckVM);
	if (e) goto cleanup;
	/* stack: () */

	cleanup:

	return e;
}

dl_error_t duckLispDev_callback_printUpvalueStack(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	duckVM_object_t *tempObjectPointer = dl_null;
	duckVM_object_t tempObject;

	printf("Call stack depth: %lu    Upvalue array call stack depth: %lu\n",
	       duckVM->call_stack.elements_length,
	       duckVM->upvalue_array_call_stack.elements_length);

	DL_DOTIMES(i, duckVM->upvalue_stack.elements_length) {
		dl_error_t e = dl_array_get(&duckVM->upvalue_stack, &tempObjectPointer, i);
		if (e) goto cleanup;
		if (tempObjectPointer == dl_null) {
			continue;
		}
		printf("%li: ", i);
		tempObject = *tempObjectPointer;
		if (tempObject.type == duckVM_object_type_none) continue;
		switch (tempObject.type) {
		case duckVM_object_type_upvalue:
			if (tempObject.value.upvalue.type == duckVM_upvalue_type_stack_index) {
				duckVM_object_t object = DL_ARRAY_GETADDRESS(duckVM->stack,
				                                               duckVM_object_t,
				                                               tempObject.value.upvalue.value.stack_index);
				e = duckVM_object_push(duckVM, &object);
				if (e) goto cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				e = duckVM_pop(duckVM);
				if (e) goto cleanup;
			}
			else if (tempObject.value.upvalue.type == duckVM_upvalue_type_heap_object) {
				e = duckVM_object_push(duckVM, tempObject.value.upvalue.value.heap_object);
				if (e) goto cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				e = duckVM_pop(duckVM);
				if (e) goto cleanup;
			}
			else {
				while (tempObject.value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
					tempObject = *tempObject.value.upvalue.value.heap_upvalue;
				}
				if (tempObject.value.upvalue.type == duckVM_upvalue_type_stack_index) {
					e = duckVM_object_push(duckVM,
					                       &DL_ARRAY_GETADDRESS(duckVM->stack,
					                                            duckVM_object_t,
					                                            tempObject.value.upvalue.value.stack_index));
					if (e) goto cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto cleanup;
					e = duckVM_pop(duckVM);
					if (e) goto cleanup;
				}
				else if (tempObject.value.upvalue.type == duckVM_upvalue_type_heap_object) {
					e = duckVM_object_push(duckVM, tempObject.value.upvalue.value.heap_object);
					if (e) goto cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto cleanup;
					e = duckVM_pop(duckVM);
					if (e) goto cleanup;
				}
			}
			putchar('\n');
			break;
		default:
			printf("Bad object type %u.\n", tempObject.type);
		}
	}

	e = duckVM_pushNil(duckVM);

	cleanup:

	return e;
}

dl_error_t duckLispDev_callback_garbageCollect(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	e = duckVM_garbageCollect(duckVM);
	if (e) goto cleanup;
	e = duckVM_pushNil(duckVM);

 cleanup:

	return e;
}

dl_error_t duckLispDev_callback_toggleAssembly(duckVM_t *duckVM) {
	g_disassemble = !g_disassemble;
	return duckVM_pushNil(duckVM);
}

#ifdef USE_PARENTHESIS_INFERENCE
dl_error_t duckLispDev_callback_toggleHanabi(duckVM_t *duckVM) {
	g_hanabi = !g_hanabi;
	return duckVM_pushNil(duckVM);
}
#endif /* USE_PARENTHESIS_INFERENCE */

int duckLispDev_callback_quicksort_hoare_less(const void *l, const void *r, const void *context) {
	(void) context;
	duckVM_object_t **left_object_ptr = (duckVM_object_t **) l;
	duckVM_object_t **right_object_ptr = (duckVM_object_t **) r;
	const dl_ptrdiff_t left = (*left_object_ptr)->value.integer;
	const dl_ptrdiff_t right = (*right_object_ptr)->value.integer;
	return left - right;
}

/* This is very unsafe.
   (defun list list-length) */
dl_error_t duckLispDev_callback_quicksort_hoare(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_object_t list = DL_ARRAY_GETADDRESS(duckVM->stack, duckVM_object_t, duckVM->stack.elements_length - 2);
	duckVM_object_t list_length = DL_ARRAY_GETADDRESS(duckVM->stack,
	                                                    duckVM_object_t,
	                                                    duckVM->stack.elements_length - 1);
	duckVM_object_t **array = dl_null;
	dl_size_t array_length = list_length.value.integer;

	e = DL_MALLOC(duckVM->memoryAllocation, &array, array_length, duckVM_object_t *);
	if (e) goto cleanup;

	DL_DOTIMES(i, array_length) {
		array[i] = list.value.list->value.cons.car;
		if (list.value.list != dl_null) {
			if ((list.value.list->value.cons.cdr == dl_null)
			    || (list.value.list->value.cons.cdr->type == duckVM_object_type_cons)) {
				list.value.list = list.value.list->value.cons.cdr;
			}
			else {
				list = *list.value.list->value.cons.cdr;
			}
		}
	}

	/**/ quicksort_hoare(array,
	                     array_length,
	                     sizeof(duckVM_object_t *),
	                     0,
	                     array_length - 1,
	                     duckLispDev_callback_quicksort_hoare_less,
	                     dl_null);

	duckVM_object_t *cons = dl_null;
	DL_DOTIMES(i, array_length) {
		duckVM_object_t tempCons;
		tempCons.value.cons.car = array[i];
		tempCons.value.cons.cdr = cons;
		tempCons.type = duckVM_object_type_cons;
		e = duckVM_allocateHeapObject(duckVM, &cons, tempCons);
		if (e) goto cleanupMalloc;
	}

	list.value.list = cons;
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;
	e = duckVM_object_push(duckVM, &list);
	if (e) goto cleanupMalloc;

 cleanupMalloc:
	eError = DL_FREE(duckVM->memoryAllocation, &array);
	if (!e) e = eError;
 cleanup:

	return e;
}

dl_error_t duckLispDev_destructor_openFile(duckVM_gclist_t *gclist, struct duckVM_object_s *object) {
	dl_error_t e = dl_error_ok;

	(void) gclist;

	puts("DESTRUCT");

	duckLispDev_user_t *fileUser = object->value.user.data;
	if (fileUser == NULL) return e;

	if (fclose(fileUser->_.file)) {
		e = dl_error_invalidValue;
	}
	(void) free(fileUser);
	fileUser = dl_null;

	return e;
}

static dl_error_t duckLispDev_marker_openFile(duckVM_gclist_t *gclist,
                                              dl_array_t *dispatch,
                                              struct duckVM_object_s *object) {
	(void) gclist;
	puts("MARK");
	if (object->value.user.data) {
		duckVM_object_t *internalObject = object->value.user.data;
		// Mark the internal object.
		return dl_array_pushElement(dispatch, &internalObject);
	}
	return dl_error_ok;
}

dl_error_t duckLispDev_callback_openFile(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_object_t fileName_object;
	duckVM_object_t mode_object;
	char *fileName = NULL;
	char *mode = NULL;
	duckVM_object_t internal;
	duckVM_object_t ret;

	e = duckVM_object_pop(duckVM, &mode_object);
	if (e) goto cleanup;
	if (mode_object.type != duckVM_object_type_string) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->open-file: Second argument (mode) must be a string."));
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckVM_object_pop(duckVM, &fileName_object);
	if (e) goto cleanup;
	if (fileName_object.type != duckVM_object_type_string) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->open-file: First argument (file-name) must be a string."));
		if (eError) e = eError;
		goto cleanup;
	}

	mode = malloc(sizeof(char) * (mode_object.value.string.internalString->value.internalString.value_length + 1));
	if (mode == NULL) {
		e = dl_error_outOfMemory;
		goto cleanup;
	}
	memcpy(mode,
	       mode_object.value.string.internalString->value.internalString.value,
	       mode_object.value.string.internalString->value.internalString.value_length);
	mode[mode_object.value.string.internalString->value.internalString.value_length] = '\0';

	fileName = malloc(sizeof(char)
	                  * (fileName_object.value.string.internalString->value.internalString.value_length + 1));
	if (fileName == NULL) {
		e = dl_error_outOfMemory;
		goto cleanup;
	}
	memcpy(fileName,
	       fileName_object.value.string.internalString->value.internalString.value,
	       fileName_object.value.string.internalString->value.internalString.value_length);
	fileName[fileName_object.value.string.internalString->value.internalString.value_length] = '\0';

	FILE *file = fopen(fileName, mode);
	if (file == NULL) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->open-file: `fopen` failed."));
		if (eError) e = eError;
		goto cleanup;
	}
	internal = duckVM_object_makeUser(malloc(sizeof(duckLispDev_user_t)), dl_null, duckLispDev_destructor_openFile);
	if (internal.value.user.data == NULL) {
		e = dl_error_outOfMemory;
		goto cleanup;
	}
	duckLispDev_user_t *internalUser = internal.value.user.data;
	internalUser->type = duckLispDev_user_type_file;
	internalUser->_.file = file;
	duckVM_object_t *internalPointer = dl_null;
	e = duckVM_allocateHeapObject(duckVM, &internalPointer, internal);
	if (e) goto cleanup;

	ret = duckVM_object_makeUser(internalPointer, duckLispDev_marker_openFile, dl_null);
	e = duckVM_object_push(duckVM, &ret);
	if (e) goto cleanup;

 cleanup:
	if (fileName) /**/ free(fileName);
	if (mode) /**/ free(mode);
	return e;
}

dl_error_t duckLispDev_callback_closeFile(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_object_t file_object;
	duckVM_object_t ret;

	e = duckVM_object_pop(duckVM, &file_object);
	if (e) goto cleanup;

	if (file_object.type != duckVM_object_type_user) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->close-file: Argument (file) must be a file."));
		if (eError) e = eError;
		goto cleanup;
	}

	duckVM_object_t *internal = file_object.value.user.data;
	duckLispDev_user_t *fileUser = dl_null;
	if (internal != dl_null) {
		fileUser = internal->value.user.data;
	}

	if ((internal == dl_null)
	    || (fileUser == dl_null)
	    || (fileUser->type != duckLispDev_user_type_file)) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->close-file: Argument (file) must be a file."));
		if (eError) e = eError;
		goto cleanup;
	}

	ret.type = duckVM_object_type_integer;
	ret.value.integer = fclose(fileUser->_.file);
	fileUser->_.file = NULL;
	/**/ free(fileUser);
	internal->value.user.data = NULL;

	e = duckVM_object_push(duckVM, &ret);
	if (e) goto cleanup;

 cleanup:
	return e;
}

dl_error_t duckLispDev_callback_fgetc(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_object_t file_object;
	duckVM_object_t ret;

	e = duckVM_object_pop(duckVM, &file_object);
	if (e) goto cleanup;

	if (file_object.type != duckVM_object_type_user) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->fgetc: Argument (file) must be a file."));
		if (eError) e = eError;
		goto cleanup;
	}

	duckVM_object_t *internal = file_object.value.user.data;
	duckLispDev_user_t *fileUser = dl_null;
	if (internal != dl_null) {
		fileUser = internal->value.user.data;
	}

	if ((internal == dl_null)
	    || (fileUser == dl_null)
	    || (fileUser->type != duckLispDev_user_type_file)) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->fgetc: Argument (file) must be a file."));
		if (eError) e = eError;
		goto cleanup;
	}

	ret.type = duckVM_object_type_integer;
	ret.value.integer = fgetc(fileUser->_.file);

	e = duckVM_object_push(duckVM, &ret);
	if (e) goto cleanup;

 cleanup:
	return e;
}

dl_error_t duckLispDev_callback_writeFile(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_object_t file_object;
	duckVM_object_t ret;
	duckVM_object_t string_object;

	e = duckVM_object_pop(duckVM, &string_object);
	if (e) goto cleanup;
	if (string_object.type != duckVM_object_type_string) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->fwrite: Second argument must be a string."));
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckVM_object_pop(duckVM, &file_object);
	if (e) goto cleanup;

	if (file_object.type != duckVM_object_type_user) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->fwrite: First argument (file) must be a file."));
		if (eError) e = eError;
		goto cleanup;
	}

	duckVM_object_t *internal = file_object.value.user.data;
	duckLispDev_user_t *fileUser = dl_null;
	if (internal != dl_null) {
		fileUser = internal->value.user.data;
	}

	if ((internal == dl_null)
	    || (fileUser == dl_null)
	    || (fileUser->type != duckLispDev_user_type_file)) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM,
		                                  DL_STR("duckVM_execute->callback->fgetc: Argument (file) must be a file."));
		if (eError) e = eError;
		goto cleanup;
	}

	ret.type = duckVM_object_type_integer;
	ret.value.integer = fwrite((&string_object.value.string.internalString->value.internalString.value
	                            [string_object.value.string.offset]),
	                           sizeof(dl_uint8_t),
	                           string_object.value.string.length,
	                           fileUser->_.file);

	e = duckVM_object_push(duckVM, &ret);
	if (e) goto cleanup;

 cleanup:
	return e;
}

/* `#include`, but for duck-lisp. The top level is assumed to be inferred. Assuming the top level *is* inferred, then it
   can include normal duck-lisp files or inferred duck-lisp files. After that, there are two rules for inclusion:
   inferred files may include normal duck-lisp files or inferred duck-lisp files, and normal duck-lisp files may only
   include normal duck-lisp files. */
dl_error_t duckLispDev_action_include(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_ast_expression_t *expression = &compoundExpression->value.expression;
#ifdef USE_PARENTHESIS_INFERENCE
	typedef enum {
		filetype_dl,
		filetype_hanabi
	} filetype_t;
	filetype_t filetype = filetype_dl;

	duckLisp_ast_string_t hanabiExtension = {DL_STR(".hna")};
#endif /* USE_PARENTHESIS_INFERENCE */
	duckLisp_ast_string_t fileName;
	char *cFileName = NULL;
	FILE *sourceFile = NULL;
	int tempInt = 0;
	char tempChar = '\0';
	dl_array_t sourceCode; /* dl_array_t:char */
	/**/ dl_array_init(&sourceCode, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	duckLisp_ast_compoundExpression_t ast;
	/**/ duckLisp_ast_compoundExpression_init(&ast);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2, dl_false);
	if (e) goto cleanup;

	e = duckLisp_checkTypeAndReportError(duckLisp,
	                                     expression->compoundExpressions[0].value.identifier,
	                                     expression->compoundExpressions[1],
	                                     duckLisp_ast_type_string);
	if (e) goto cleanup;

	fileName.value = expression->compoundExpressions[1].value.string.value;
	fileName.value_length = expression->compoundExpressions[1].value.string.value_length;

#ifdef USE_PARENTHESIS_INFERENCE
	if (fileName.value_length >= hanabiExtension.value_length) {
		dl_bool_t result;
		(void) dl_string_compare_partial(&result,
		                                 (fileName.value
		                                  + fileName.value_length
		                                  - hanabiExtension.value_length),
		                                 hanabiExtension.value,
		                                 hanabiExtension.value_length);
		if (result) filetype = filetype_hanabi;
	}
#endif /* USE_PARENTHESIS_INFERENCE */

	printf(COLOR_YELLOW);
	printf("(include \"");
	DL_DOTIMES(i, fileName.value_length) {
		putchar(fileName.value[i]);
	}
	printf("\")\n");
	printf(COLOR_NORMAL);

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &cFileName, (fileName.value_length + 1) * sizeof(char));
	if (e) goto cleanup;
	/**/ dl_memcopy_noOverlap(cFileName, fileName.value, fileName.value_length);
	cFileName[fileName.value_length] = '\0';

	/* Fetch script. */

	e = dl_array_pushElements(&sourceCode, DL_STR("("));
	if (e) goto cFileName_cleanup;

	sourceFile = fopen(cFileName, "r");
	if (sourceFile == NULL) {
		printf(COLOR_RED "Could not open file \"%s\".\n" COLOR_NORMAL, cFileName);
		e = dl_error_nullPointer;
		goto cFileName_cleanup;
	}
	while ((tempInt = fgetc(sourceFile)) != EOF) {
		tempChar = tempInt & 0xFF;
		e = dl_array_pushElement(&sourceCode, &tempChar);
		if (e) {
			fclose(sourceFile);
			goto cFileName_cleanup;
		}
	}
	fclose(sourceFile);

	tempChar = ')';
	e = dl_array_pushElement(&sourceCode, &tempChar);
	if (e) goto cFileName_cleanup;

	/* Parse script. */

	{
		dl_ptrdiff_t index = 0;
		e = duckLisp_parse_compoundExpression(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
		                                      dl_false,
#endif /* USE_PARENTHESIS_INFERENCE */
		                                      fileName.value,
		                                      fileName.value_length,
		                                      sourceCode.elements,
		                                      sourceCode.elements_length,
		                                      &ast,
		                                      &index,
		                                      dl_true);
		if (e) goto cFileName_cleanup;
	}

	/* printf(COLOR_YELLOW); */
	/* printf("Included AST: "); */
	/* e = ast_print_compoundExpression(*duckLisp, ast); */
	/* putchar('\n'); */
	/* if (e) goto l_cFileName_cleanup; */
	/* puts(COLOR_NORMAL); */

	// Free the original expression.
	{
		duckLisp_ast_compoundExpression_t ce;
		ce.type = duckLisp_ast_type_expression;
		ce.value.expression = *expression;
		e = duckLisp_ast_compoundExpression_quit(duckLisp->memoryAllocation, &ce);
		if (e) goto cFileName_cleanup;
	}

	// Replace the expression with `__noscope` and then the body of the just read file.

	e = DL_MALLOC(duckLisp->memoryAllocation,
	              &expression->compoundExpressions,
	              ast.value.expression.compoundExpressions_length + 1,
	              duckLisp_ast_compoundExpression_t);
	if (e) goto cFileName_cleanup;
	expression->compoundExpressions_length = ast.value.expression.compoundExpressions_length + 1;

	{
		duckLisp_ast_identifier_t identifier;
		identifier.value_length = sizeof("__noscope") - 1;
		e = DL_MALLOC(duckLisp->memoryAllocation, &identifier.value, identifier.value_length, char);
		if (e) goto cFileName_cleanup;
		// Yes, this is dirty macro abuse.
		(void) dl_memcopy_noOverlap(identifier.value, DL_STR("__noscope") * sizeof(char));
		expression->compoundExpressions[0].type = duckLisp_ast_type_identifier;
		expression->compoundExpressions[0].value.identifier = identifier;
	}

	(void) dl_memcopy_noOverlap(&expression->compoundExpressions[1],
	                            ast.value.expression.compoundExpressions,
	                            (ast.value.expression.compoundExpressions_length
	                             * sizeof(duckLisp_ast_compoundExpression_t)));

	e = DL_FREE(duckLisp->memoryAllocation, &ast.value.expression.compoundExpressions);
	if (e) goto cFileName_cleanup;

#ifdef USE_PARENTHESIS_INFERENCE
	if (filetype != filetype_hanabi) compoundExpression->type = duckLisp_ast_type_literalExpression;
#endif /* USE_PARENTHESIS_INFERENCE */


 cFileName_cleanup:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &cFileName);
	if (eError) e = eError;

 cleanup:
	/**/ dl_array_quit(&sourceCode);

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

int eval(duckLisp_t *duckLisp,
         duckVM_t *duckVM,
         duckVM_object_t *return_value,
#ifdef USE_PARENTHESIS_INFERENCE
         const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
         const dl_uint8_t *source,
         const dl_size_t source_length,
         const dl_uint8_t *fileName,
         const dl_size_t fileName_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	char tempChar;
	/* dl_size_t tempDlSize; */

	dl_array_t sourceCode;

	/**/ dl_array_init(&sourceCode, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Fetch script. */

	// Provide implicit progn.
	e = dl_array_pushElements(&sourceCode, DL_STR("(() "));
	if (e) goto cleanup;

	e = dl_array_pushElements(&sourceCode, source, source_length);
	if (e) goto cleanup;

	tempChar = ')';
	e = dl_array_pushElement(&sourceCode, &tempChar);
	if (e) goto cleanup;

	/* Compile functions. */

	/* printf(COLOR_CYAN); */
	/* /\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* printf("Compiler memory usage: %llu/%llu (%llu%%)\n", tempDlSize, duckLisp->memoryAllocation->size, 100*tempDlSize/duckLisp->memoryAllocation->size); */
	/* puts(COLOR_NORMAL); */

	dl_error_t loadError;
	dl_error_t runtimeError;
	unsigned char *bytecode = dl_null;
	dl_size_t bytecode_length = 0;
	loadError = duckLisp_loadString(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
	                                parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
	                                &bytecode,
	                                &bytecode_length,
	                                &DL_ARRAY_GETADDRESS(sourceCode, dl_uint8_t, 0),
	                                sourceCode.elements_length,
	                                fileName,
	                                fileName_length);

	/* e = dl_memory_checkHealth(*duckLisp->memoryAllocation); */
	/* if (e) { */
	/* 	printf(COLOR_RED "Memory health check failed. (%s)\n" COLOR_NORMAL, dl_errorString[e]); */
	/* } */

	printf(COLOR_CYAN);

	/* /\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* printf("Compiler memory usage: %llu/%llu (%llu%%)\n\n", tempDlSize, duckLisp->memoryAllocation->size, 100*tempDlSize/duckLisp->memoryAllocation->size); */

	/* // Note: The memSize/eleSize trick only works with the "fit" strategy. */
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckLisp->scope_stack.elements_memorySize / duckLisp->scope_stack.element_size; i++) { */
	/* 	printf("Scope %lli: locals\n", i); */
	/* 	/\**\/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp->scope_stack.elements)[i].locals_trie); */
	/* } */
	/* putchar('\n'); */
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckLisp->scope_stack.elements_memorySize / duckLisp->scope_stack.element_size; i++) { */
	/* 	printf("Scope %lli: statics\n", i); */
	/* 	/\**\/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp->scope_stack.elements)[i].statics_trie); */
	/* } */
	/* putchar('\n'); */
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckLisp->scope_stack.elements_memorySize / duckLisp->scope_stack.element_size; i++) { */
	/* 	printf("Scope %lli: generators\n", i); */
	/* 	/\**\/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp->scope_stack.elements)[i].generators_trie); */
	/* } */
	/* putchar('\n'); */
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckLisp->scope_stack.elements_memorySize / duckLisp->scope_stack.element_size; i++) { */
	/* 	printf("Scope %lli: functions (1: callback  2: script  3: generator)\n", i); */
	/* 	/\**\/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp->scope_stack.elements)[i].functions_trie); */
	/* } */
	/* putchar('\n'); */
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckLisp->scope_stack.elements_memorySize / duckLisp->scope_stack.element_size; i++) { */
	/* 	printf("Scope %lli: labels\n", i); */
	/* 	/\**\/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp->scope_stack.elements)[i].labels_trie); */
	/* } */

	printf(COLOR_NORMAL);

	if (loadError) {
		printf(COLOR_RED "Error loading string. (%s)\n" COLOR_NORMAL, dl_errorString[loadError]);
	}

#ifdef USE_PARENTHESIS_INFERENCE
	if (loadError) {
		DL_DOTIMES(i, duckLisp->inferrerLog.elements_length) {
			putchar(DL_ARRAY_GETADDRESS(duckLisp->inferrerLog, dl_uint8_t, i));
		}
	}
#endif /* USE_PARENTHESIS_INFERENCE */

	{
		dl_uint8_t *errorString = duckLisp->errors.elements;
		dl_size_t errorString_length = duckLisp->errors.elements_length;
		printf(COLOR_RED);
		DL_DOTIMES(i, errorString_length) {
			putchar(errorString[i]);
		}
		printf(COLOR_NORMAL);
		(void) free(errorString);
		if (e) goto cleanup;
	}
	/* printf(COLOR_CYAN); */
	/* /\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* printf("Compiler memory usage: %llu/%llu (%llu%%)\n", tempDlSize, duckLisp->memoryAllocation->size, 100*tempDlSize/duckLisp->memoryAllocation->size); */
	/* puts(COLOR_NORMAL); */

	if (loadError) {
		/* putchar('\n'); */
		printf(COLOR_RED "Failed to compile source.\n" COLOR_NORMAL);
		e = loadError;
		goto cleanup;
	}

	if (g_disassemble) {
		dl_uint8_t *disassembly = NULL;
		dl_size_t length = 0;
		duckLisp_disassemble(&disassembly, &length, duckLisp->memoryAllocation, bytecode, bytecode_length);
		printf("%s", disassembly);
		e = dl_free(duckLisp->memoryAllocation, (void **) &disassembly);
		if (e) goto cleanup;
		putchar('\n');
	}

	/* /\* Print bytecode in hex. *\/ */
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < bytecode_length; i++) { */
	/* 	unsigned char byte = bytecode[i]; */
	/* 	putchar(dl_nybbleToHexChar(byte >> 4)); */
	/* 	putchar(dl_nybbleToHexChar(byte)); */
	/* } */
	/* putchar('\n'); */

	/* putchar('\n'); */
	/* puts(COLOR_CYAN "VM: {" COLOR_NORMAL); */

	runtimeError = duckVM_execute(duckVM, return_value, bytecode, bytecode_length);
	{
		dl_uint8_t *errorString = duckVM->errors.elements;
		dl_size_t errorString_length = duckVM->errors.elements_length;
		printf(COLOR_RED);
		DL_DOTIMES(i, errorString_length) {
			putchar(errorString[i]);
		}
		printf(COLOR_NORMAL);
		(void) free(errorString);
		if (e) goto cleanup;
	}
	if (e) goto cleanup;
	if (runtimeError) {
		printf(COLOR_RED "\nVM returned error. (%s)\n" COLOR_NORMAL, dl_errorString[runtimeError]);
		e = runtimeError;
		goto cleanupBytecode;
	}

#ifdef USE_DATALOGGING
	puts("");
	printf("instructions generated: %lu\n", duckLisp->datalog.total_instructions_generated);
	printf("bytes generated: %lu\n", duckLisp->datalog.total_bytes_generated);
	if (duckLisp->datalog.total_instructions_generated > 0) {
		printf("average bytes per instruction: %3.2f\n",
		       ((double) duckLisp->datalog.total_bytes_generated
		        / (double) duckLisp->datalog.total_instructions_generated));
	}
	printf("push-pop optimization -- instructions removed: %lu -- percent improvement: %3.2f%%\n",
	       duckLisp->datalog.pushpop_instructions_removed,
	       (100.0
	        * ((double) duckLisp->datalog.pushpop_instructions_removed
	           / (double) duckLisp->datalog.total_instructions_generated)));
	printf("jump size optimization -- bytes removed: %lu -- percent improvement: %3.2f%%\n",
	       duckLisp->datalog.jumpsize_bytes_removed,
	       (100.0
	        * (double) duckLisp->datalog.jumpsize_bytes_removed / (double) duckLisp->datalog.total_bytes_generated));
#endif /* USE_DATALOGGING */

 cleanupBytecode:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &bytecode);
	if (!e) e = eError;

	/* puts(COLOR_CYAN "}" COLOR_NORMAL); */

 cleanup:

	eError = dl_array_quit(&sourceCode);
	if (!e) e = eError;

	return e;
}

int evalFile(duckLisp_t *duckLisp, duckVM_t *duckVM, duckVM_object_t *return_value, const dl_uint8_t *filename) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

#ifdef USE_PARENTHESIS_INFERENCE
	typedef enum {
		filetype_dl,
		filetype_hanabi
	} filetype_t;
	filetype_t filetype = filetype_dl;
#endif /* USE_PARENTHESIS_INFERENCE */

	char tempChar;
	int tempInt;
#ifdef USE_PARENTHESIS_INFERENCE
	duckLisp_ast_string_t hanabiExtension = {DL_STR(".hna")};
#endif /* USE_PARENTHESIS_INFERENCE */
	size_t filename_length = strlen((const char *) filename);

	dl_array_t sourceCode;
	/**/ dl_array_init(&sourceCode, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

#ifdef USE_PARENTHESIS_INFERENCE
	if (filename_length >= hanabiExtension.value_length) {
		dl_bool_t result;
		(void) dl_string_compare_partial(&result,
		                                 (filename + filename_length - hanabiExtension.value_length),
		                                 hanabiExtension.value,
		                                 hanabiExtension.value_length);
		if (result) filetype = filetype_hanabi;
	}
#endif /* USE_PARENTHESIS_INFERENCE */

	/* Fetch script. */

	FILE *sourceFile = sourceFile = fopen((const char *) filename, "r");
	if (sourceFile != NULL) {
		while ((tempInt = fgetc(sourceFile)) != EOF) {
			tempChar = tempInt & 0xFF;
			e = dl_array_pushElement(&sourceCode, &tempChar);
			if (e) {
				fclose(sourceFile);
				goto cleanup;
			}
		}
		fclose(sourceFile);
	}
	else {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	e = eval(duckLisp,
	         duckVM,
	         return_value,
#ifdef USE_PARENTHESIS_INFERENCE
	         filetype == filetype_hanabi,
#endif /* USE_PARENTHESIS_INFERENCE */
	         sourceCode.elements,
	         sourceCode.elements_length,
	         filename,
	         filename_length);

 cleanup:

	eError = dl_array_quit(&sourceCode);
	if (!e) e = eError;

	return e;
}

int main(int argc, char *argv[]) {
	dl_error_t e = dl_error_ok;
	struct {
		dl_bool_t duckLispMemory;
		dl_bool_t duckLisp_init;
		dl_bool_t duckVMMemory;
		dl_bool_t duckVM_init;
	} d = {0};

	const size_t duckLispMemory_size = 10 * 1024 * 1024;
	const size_t duckVMMemory_size = 1000 * 64 * 1024;
	const size_t duckVMMaxObjects = 1000000;
	const size_t duckComptimeVMMaxObjects = duckVMMaxObjects;
#ifdef USE_PARENTHESIS_INFERENCE
	const size_t duckInferenceVMMaxObjects = duckComptimeVMMaxObjects;
#endif /* USE_PARENTHESIS_INFERENCE */

	duckLisp_t duckLisp;
	void *duckLispMemory = dl_null;
	dl_memoryAllocation_t duckLispMemoryAllocation;
	void *duckVMMemory = dl_null;
	dl_memoryAllocation_t duckVMMemoryAllocation;
	size_t tempMemory_size;
	dl_size_t tempDlSize;

	duckVM_t duckVM;

	// All user-defined generators go here.
	struct {
		const unsigned char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, duckLisp_ast_compoundExpression_t*);
	} parser_actions[] = {
		{DL_STR("include"), duckLispDev_action_include},
		{dl_null, 0,        dl_null}
	};

	// All user-defined generators go here.
	struct {
		dl_uint8_t *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, duckLisp_compileState_t*, dl_array_t*, duckLisp_ast_expression_t*);
		dl_uint8_t *type;
		const dl_size_t type_length;
		dl_uint8_t *declarationScript;
		const dl_size_t declarationScript_length;
	} generators[] = {
		{dl_null, 0, dl_null, dl_null, 0, dl_null, 0}
	};

	// All user-defined callbacks go here.
	struct {
		dl_uint8_t *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
		dl_uint8_t *typeString;
		const dl_size_t typeString_length;
	} callbacks[] = {
		{DL_STR("print"),           duckLispDev_callback_print,             DL_STR("(I)")},
		{DL_STR("print-stack"),     duckLispDev_callback_printStack,        DL_STR("()")},
		{DL_STR("garbage-collect"), duckLispDev_callback_garbageCollect,    DL_STR("()")},
		{DL_STR("disassemble"),     duckLispDev_callback_toggleAssembly,    DL_STR("()")},
#ifdef USE_PARENTHESIS_INFERENCE
		{DL_STR("inference"),       duckLispDev_callback_toggleHanabi,      DL_STR("()")},
#endif /* USE_PARENTHESIS_INFERENCE */
		{DL_STR("quicksort-hoare"), duckLispDev_callback_quicksort_hoare,   DL_STR("(I I)")},
		{DL_STR("print-uv-stack"),  duckLispDev_callback_printUpvalueStack, DL_STR("()")},
		{DL_STR("open-file"),       duckLispDev_callback_openFile,          DL_STR("(I I)")},
		{DL_STR("close-file"),      duckLispDev_callback_closeFile,         DL_STR("(I)")},
		{DL_STR("fgetc"),           duckLispDev_callback_fgetc,             DL_STR("(I)")},
		{DL_STR("fwrite"),          duckLispDev_callback_writeFile,         DL_STR("(I I)")},
		{dl_null, 0,                dl_null,                                dl_null, 0}
	};

	/* Initialization. */

	if ((argc != 1) && (argc != 2)) {
		printf(COLOR_YELLOW
		       "Usage:\n"
		       "./duckLisp-dev filename                                               Run script from file\n"
		       "./duckLisp-dev \"(func0 arg0 arg1 ...) (func1 arg0 arg1 ...) ...\"      Run script from string\n"
		       "./duckLisp-dev                                                        Start REPL\n"
		       COLOR_NORMAL);
		goto cleanup;
	}

	tempMemory_size = duckLispMemory_size;
	duckLispMemory = malloc(tempMemory_size);
	if (duckLispMemory == NULL) {
		e = dl_error_outOfMemory;
		printf(COLOR_RED "Out of memory.\n" COLOR_NORMAL);
		goto cleanup;
	}
	d.duckLispMemory = dl_true;

	e = dl_memory_init(&duckLispMemoryAllocation, duckLispMemory, tempMemory_size, dl_memoryFit_best);
	if (e) {
		goto cleanup;
	}

	e = duckLisp_init(&duckLisp,
	                  &duckLispMemoryAllocation,
	                  duckComptimeVMMaxObjects
#ifdef USE_PARENTHESIS_INFERENCE
	                  ,
	                  duckInferenceVMMaxObjects
#endif /* USE_PARENTHESIS_INFERENCE */
	                  );
	if (e) {
		printf(COLOR_RED "Could not initialize DuckLisp. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		{
			dl_uint8_t *errorString = duckLisp.errors.elements;
			dl_size_t errorString_length = duckLisp.errors.elements_length;
			dl_error_t eError = dl_error_ok;
			if (eError) e = eError;
			printf(COLOR_RED);
			DL_DOTIMES(i, errorString_length) {
				putchar(errorString[i]);
			}
			printf(COLOR_NORMAL);
			(void) free(errorString);
			if (e) goto cleanup;
		}
		goto cleanup;
	}
	d.duckLisp_init = dl_true;

	/* Add actions to reader. */

	for (dl_ptrdiff_t i = 0; parser_actions[i].name != dl_null; i++) {
		e = duckLisp_addParserAction(&duckLisp,
		                             parser_actions[i].callback,
		                             parser_actions[i].name,
		                             parser_actions[i].name_length);
		if (e) {
			printf(COLOR_RED "Could not register reader action. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		}
	}

	/* Create generators. */

	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		e = duckLisp_addGenerator(&duckLisp,
		                          generators[i].callback,
		                          generators[i].name,
		                          generators[i].name_length
#ifdef USE_PARENTHESIS_INFERENCE
		                          ,
		                          generators[i].type,
		                          generators[i].type_length,
		                          generators[i].declarationScript,
		                          generators[i].declarationScript_length
#endif /* USE_PARENTHESIS_INFERENCE */
		                          );
		if (e) {
			printf(COLOR_RED "Could not register generator. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		}
	}

	/* Add C functions. */

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		e = duckLisp_linkCFunction(&duckLisp,
		                           callbacks[i].callback,
		                           callbacks[i].name,
		                           callbacks[i].name_length
#ifdef USE_PARENTHESIS_INFERENCE
		                           ,
		                           callbacks[i].typeString,
		                           callbacks[i].typeString_length
#endif /* USE_PARENTHESIS_INFERENCE */
		                           );
		if (e) {
			printf(COLOR_RED "Could not create function. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
			goto cleanup;
		}
	}

	tempMemory_size = duckVMMemory_size;
	duckVMMemory = malloc(tempMemory_size);
	if (duckVMMemory == NULL) {
		e = dl_error_outOfMemory;
		printf("Out of memory.\n");
		goto cleanup;
	}

	e = dl_memory_init(&duckVMMemoryAllocation, duckVMMemory, tempMemory_size, dl_memoryFit_best);
	if (e) {
		goto cleanup;
	}

	d.duckVMMemory = dl_true;

	/* Execute. */
	e = duckVM_init(&duckVM, &duckVMMemoryAllocation, duckVMMaxObjects);
	if (e) {
		printf("Could not initialize VM. (%s)\n", dl_errorString[e]);
		goto cleanup;
	}
	d.duckVM_init = dl_true;

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		e = duckVM_linkCFunction(&duckVM,
		                         duckLisp_symbol_nameToValue(&duckLisp, callbacks[i].name, callbacks[i].name_length),
		                         callbacks[i].callback);
		if (e) {
			printf("Could not link callback into VM. (%s)\n", dl_errorString[e]);
			goto cleanup;
		}
	}

	if (argc == 2) {
		FILE *sourceFile = fopen(argv[1], "r");
		if (sourceFile == NULL) {
			e = eval(&duckLisp,
			         &duckVM,
			         dl_null,
#ifdef USE_PARENTHESIS_INFERENCE
			         dl_true,
#endif /* USE_PARENTHESIS_INFERENCE */
			         (unsigned char *) argv[1],
			         strlen(argv[1]),
			         DL_STR("<ARGV>"));
		}
		else {
			if (fclose(sourceFile) == 0) e = evalFile(&duckLisp, &duckVM, dl_null, (const unsigned char *) argv[1]);
		}
	}
	/* REPL */
	else if (argc == 1) {
		dl_uint8_t *line = NULL;
		size_t buffer_length = 0;
		ssize_t length = 0;
		printf("HELP:\n");
		printf("    (print <value>)                         Display the value of an object.\n");
		printf("    (print-stack)                           Print the data stack of the current environment.\n");
		printf("    (garbage-collect)                       Manually run garbage collection.\n");
		printf("    (disassemble)               %s  Toggle disassembly of forms.\n",
		       g_disassemble ? "[enabled] " : "[disabled]");
#ifdef USE_PARENTHESIS_INFERENCE
		printf("    (inference)                 %s  Toggle parenthesis inference.\n",
		       g_hanabi ? " [enabled]" : "[disabled]");
#endif /* USE_PARENTHESIS_INFERENCE */
		printf("    (quicksort-hoare <list> <list-length>)  Sort a numeric list in descending order.\n");
		printf("    (print-uv-stack)                        Print the upvalue stack in the current environment.\n");
		printf("    (open-file <name> <mode>)               Call `fopen'.\n");
		printf("    (close-file <file>)                     Call `fclose'.\n");
		printf("    (fgetc <file>)                          Call `fgetc'.\n");
		printf("    (fwrite <file> <string>)                Call `fwrite'.\n");
		while (1) {
			duckVM_object_t return_value;
			if (duckVM.stack.elements_length > 0) {
				printf(COLOR_YELLOW
				       "A runtime error has occured. Use (print-stack) to inspect the stack, or press\n"
				       "RET to pop a stack frame."
				       COLOR_NORMAL);
				printf("\n%lu", duckVM.stack.elements_length);
			}
			printf("> ");
			if ((length = getline((char **) &line, &buffer_length, stdin)) < 0) break;
			e = eval(&duckLisp,
			         &duckVM,
			         &return_value,
#ifdef USE_PARENTHESIS_INFERENCE
			         g_hanabi,
#endif /* USE_PARENTHESIS_INFERENCE */
			         line,
			         length,
			         DL_STR("<REPL>"));
			free(line); line = NULL;
			e = duckVM_object_push(&duckVM, &return_value);
			if (e) break;
			e = duckLispDev_callback_print(&duckVM);
			if (e) break;
			e = duckVM_pop(&duckVM);
			if (e) break;

			puts(COLOR_CYAN);
			/**/ dl_memory_usage(&tempDlSize, *duckLisp.memoryAllocation);
			printf("Compiler memory usage: %lu/%lu (%lu%%)\n",
			       tempDlSize,
			       duckLisp.memoryAllocation->size,
			       100*tempDlSize/duckLisp.memoryAllocation->size);
			/**/ dl_memory_usage(&tempDlSize, *duckVM.memoryAllocation);
			printf("VM memory usage: %lu/%lu (%lu%%)\n",
			       tempDlSize,
			       duckVM.memoryAllocation->size,
			       100*tempDlSize/duckVM.memoryAllocation->size);
			puts(COLOR_NORMAL);

		}
	}

 cleanup:

	puts(COLOR_CYAN);
	if (d.duckVM_init) {
		/**/ dl_memory_usage(&tempDlSize, *duckVM.memoryAllocation);
		printf("(duckVM) Current memory use: %lu/%lu (%lu%%)\n",
			   tempDlSize,
			   duckVM.memoryAllocation->size,
			   100*tempDlSize/duckVM.memoryAllocation->size);
		printf("(duckVM) Max memory used:    %lu/%lu (%lu%%)\n",
			   duckVM.memoryAllocation->max_used,
			   duckVM.memoryAllocation->size,
			   100*duckVM.memoryAllocation->max_used/duckVM.memoryAllocation->size);
		/**/ duckVM_quit(&duckVM);
		/**/ dl_memory_quit(duckVM.memoryAllocation);
	}
	if (d.duckVMMemory) {
		puts("Freeing VM memory.");
		/**/ free(duckVMMemory); duckVMMemory = NULL;
	}

	if (d.duckLisp_init) {
		puts("");
		/**/ dl_memory_usage(&tempDlSize, *duckLisp.memoryAllocation);
		printf("(duckLisp) Current memory use: %lu/%lu (%lu%%)\n",
			   tempDlSize,
			   duckLisp.memoryAllocation->size,
			   100*tempDlSize/duckLisp.memoryAllocation->size);
		printf("(duckLisp) Max memory used:    %lu/%lu (%lu%%)\n",
			   duckLisp.memoryAllocation->max_used,
			   duckLisp.memoryAllocation->size,
			   100*duckLisp.memoryAllocation->max_used/duckLisp.memoryAllocation->size);

		/**/ duckLisp_quit(&duckLisp);
		/**/ dl_memory_quit(duckLisp.memoryAllocation);
	}
	if (d.duckLispMemory) {
		puts("Freeing compiler memory.");
		/**/ free(duckLispMemory); duckLispMemory = NULL;
	}
	printf(COLOR_NORMAL);

	return e;
}
