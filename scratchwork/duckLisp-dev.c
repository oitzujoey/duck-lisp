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
#include "../DuckLib/core.h"
#include "../duckLisp.h"
#include "../duckVM.h"
#include "../DuckLib/sort.h"


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

dl_error_t duckLispDev_callback_print(duckVM_t *duckVM);


dl_error_t duckLispDev_callback_printCons(duckVM_t *duckVM, duckLisp_object_t *cons) {
	dl_error_t e = dl_error_ok;

	if (cons == dl_null) {
		printf("nil");
	}
	else {
		if (cons->value.cons.car == dl_null) {
			printf("nil");
		}
		else if (cons->value.cons.car->type == duckLisp_object_type_cons) {
			printf("(");
			e = duckLispDev_callback_printCons(duckVM, cons->value.cons.car);
			if (e) goto l_cleanup;
			printf(")");
		}
		else {
			e = duckVM_push(duckVM, cons->value.cons.car);
			if (e) goto l_cleanup;
			e = duckLispDev_callback_print(duckVM);
			if (e) goto l_cleanup;
			e = duckVM_pop(duckVM, dl_null);
			if (e) goto l_cleanup;
		}
		
		
		if ((cons->value.cons.cdr == dl_null)
		    || (cons->value.cons.cdr->type == duckLisp_object_type_cons)) {
			if (cons->value.cons.cdr != dl_null) {
				printf(" ");
				e = duckLispDev_callback_printCons(duckVM, cons->value.cons.cdr);
				if (e) goto l_cleanup;
			}
		}
		else {
			printf(" . ");
			e = duckVM_push(duckVM, cons->value.cons.cdr);
			if (e) goto l_cleanup;
			e = duckLispDev_callback_print(duckVM);
			if (e) goto l_cleanup;
			e = duckVM_pop(duckVM, dl_null);
			if (e) goto l_cleanup;
		}
	}
		
 l_cleanup:
	
	return e;
}

dl_error_t duckLispDev_callback_print(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	
	duckLisp_object_t object;
	
	// e = duckVM_getArg(duckVM, &string, 0);
	e = duckVM_pop(duckVM, &object);
	if (e) goto l_cleanup;
	
	switch (object.type) {
	case duckLisp_object_type_symbol:
		for (dl_size_t i = 0; i < object.value.symbol.value_length; i++) {
			putchar(object.value.symbol.value[i]);
		}
		printf("→%llu", object.value.symbol.id);
		break;
	case duckLisp_object_type_string:
		for (dl_size_t i = 0; i < object.value.string.value_length; i++) {
			putchar(object.value.string.value[i]);
		}
		break;
	case duckLisp_object_type_integer:
		printf("%lli", object.value.integer);
		break;
	case duckLisp_object_type_float:
		printf("%f\n", object.value.floatingPoint);
		break;
	case duckLisp_object_type_bool:
		printf("%s", object.value.boolean ? "true" : "false");
		break;
	case duckLisp_object_type_list:
		if (object.value.list == dl_null) {
			printf("nil");
		}
		else {
			/* printf("(%i: ", object.value.list->type); */
			printf("(");

			if (object.value.list->value.cons.car == dl_null) {
				printf("(nil)");
			}
			else if (object.value.list->value.cons.car->type == duckLisp_object_type_cons) {
				printf("(");
				e = duckLispDev_callback_printCons(duckVM, object.value.list->value.cons.car);
				if (e) goto l_cleanup;
				printf(")");
			}
			else {
				e = duckVM_push(duckVM, object.value.list->value.cons.car);
				if (e) goto l_cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto l_cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto l_cleanup;
			}

			if (object.value.list->value.cons.cdr == dl_null) {
			}
			else if (object.value.list->value.cons.cdr->type == duckLisp_object_type_cons) {
				if (object.value.list->value.cons.cdr != dl_null) {
					printf(" ");
					e = duckLispDev_callback_printCons(duckVM, object.value.list->value.cons.cdr);
					if (e) goto l_cleanup;
				}
			}
			else {
				printf(" . ");
				e = duckVM_push(duckVM, object.value.list->value.cons.cdr);
				if (e) goto l_cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto l_cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto l_cleanup;
			}

			printf(")");
		}
		break;
	case duckLisp_object_type_closure:
		printf("(closure %lli", object.value.closure.name);
		DL_DOTIMES(k, object.value.closure.upvalue_array->value.upvalue_array.length) {
			duckLisp_object_t *uv = object.value.closure.upvalue_array->value.upvalue_array.upvalues[k];
			putchar(' ');
			if (uv == dl_null) {
				putchar('$');
				continue;
			}
			if (uv->value.upvalue.type == duckVM_upvalue_type_stack_index) {
				duckLisp_object_t object = DL_ARRAY_GETADDRESS(duckVM->stack,
				                                               duckLisp_object_t,
				                                               uv->value.upvalue.value.stack_index);
				e = duckVM_push(duckVM, &object);
				if (e) goto l_cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto l_cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto l_cleanup;
			}
			else if (uv->value.upvalue.type == duckVM_upvalue_type_heap_object) {
				e = duckVM_push(duckVM, uv->value.upvalue.value.heap_object);
				if (e) goto l_cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto l_cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto l_cleanup;
			}
			else {
				while (uv->value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
					uv = uv->value.upvalue.value.heap_upvalue;
				}
				if (uv->value.upvalue.type == duckVM_upvalue_type_stack_index) {
					e = duckVM_push(duckVM,
					                &DL_ARRAY_GETADDRESS(duckVM->stack,
					                                     duckLisp_object_t,
					                                     uv->value.upvalue.value.stack_index));
					if (e) goto l_cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto l_cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto l_cleanup;
				}
				else if (uv->value.upvalue.type == duckVM_upvalue_type_heap_object) {
					e = duckVM_push(duckVM, uv->value.upvalue.value.heap_object);
					if (e) goto l_cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto l_cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto l_cleanup;
				}
			}
		}
		printf(")");
		break;
	case duckLisp_object_type_vector:
		printf("[");
		if (object.value.vector.internal_vector != dl_null) {
			for (dl_ptrdiff_t k = object.value.vector.offset;
			     (dl_size_t) k < object.value.vector.internal_vector->value.internal_vector.length;
			     k++) {
				duckLisp_object_t *value = object.value.vector.internal_vector->value.internal_vector.values[k];
				if (k != object.value.vector.offset) putchar(' ');
				e = duckVM_push(duckVM, value);
				if (e) goto l_cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto l_cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto l_cleanup;
			}
		}
		printf("]");
		break;
	case duckLisp_object_type_type:
		printf("::%llu", object.value.type);
		break;
	default:
		printf("print: Unsupported type. [%u]\n", object.type);
	}

	e = duckVM_push(duckVM, &object);
	if (e) {
		goto l_cleanup;
	}

	l_cleanup:

	fflush(stdout);

	return e;
}

dl_error_t duckLispDev_callback_printStack(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	duckLisp_object_t tempObject;

	DL_DOTIMES(i, duckVM->stack.elements_length) {
		dl_error_t e = dl_array_get(&duckVM->stack, &tempObject, i);
		if (e) goto cleanup;
		printf("%lli: ", i);
		switch (tempObject.type) {
		case duckLisp_object_type_bool:
			puts(tempObject.value.boolean ? "true" : "false");
			break;
		case duckLisp_object_type_integer:
			printf("%lli\n", tempObject.value.integer);
			break;
		case duckLisp_object_type_float:
			printf("%f\n", tempObject.value.floatingPoint);
			break;
		case duckLisp_object_type_function:
			if (tempObject.value.function.bytecode != dl_null) {
				printf("bytecode<%llu>\n", (unsigned long long) tempObject.value.function.bytecode);
			}
			else {
			printf("callback<%llu>\n", (unsigned long long) tempObject.value.function.callback);
			}
			break;
		case duckLisp_object_type_symbol:
			for (dl_size_t i = 0; i < tempObject.value.symbol.value_length; i++) {
				putchar(tempObject.value.symbol.value[i]);
			}
			printf("→%llu", tempObject.value.symbol.id);
			putchar('\n');
			break;
		case duckLisp_object_type_string:
			putchar('"');
			for (dl_ptrdiff_t k = 0; (dl_size_t) k < tempObject.value.string.value_length; k++) {
				switch (tempObject.value.string.value[k]) {
				case '\n':
					putchar('\\');
					putchar('n');
					break;
				default:
					putchar(tempObject.value.string.value[k]);
				}
			}
			putchar('"');
			putchar('\n');
			break;
		case duckLisp_object_type_list:
			if (tempObject.value.list == dl_null) {
				printf("nil");
			}
			else {
				/* printf("(%i: ", tempObject.value.list->type); */
				printf("(");

				if (tempObject.value.list->value.cons.car == dl_null) {

				}
				else if (tempObject.value.list->value.cons.car->type == duckLisp_object_type_cons) {
					printf("(");
					e = duckLispDev_callback_printCons(duckVM, tempObject.value.list->value.cons.car);
					if (e) goto cleanup;
					printf(")");
				}
				else {
					e = duckVM_push(duckVM, tempObject.value.list->value.cons.car);
					if (e) goto cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto cleanup;
				}

				if (tempObject.value.list->value.cons.cdr == dl_null) {

				}
				else if (tempObject.value.list->value.cons.cdr->type == duckLisp_object_type_cons) {
					if (tempObject.value.list->value.cons.cdr != dl_null) {
						printf(" ");
						e = duckLispDev_callback_printCons(duckVM, tempObject.value.list->value.cons.cdr);
						if (e) goto cleanup;
					}
				}
				else {
					printf(" . ");
					e = duckVM_push(duckVM, tempObject.value.list->value.cons.cdr);
					if (e) goto cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto cleanup;
				}

				printf(")");
			}
			putchar('\n');
			break;
		case duckLisp_object_type_closure:
			printf("(closure %lli", tempObject.value.closure.name);
			DL_DOTIMES(k, tempObject.value.closure.upvalue_array->value.upvalue_array.length) {
				duckLisp_object_t *uv = tempObject.value.closure.upvalue_array->value.upvalue_array.upvalues[k];
				putchar(' ');
				if (uv == dl_null) {
					putchar('$');
					continue;
				}
				if (uv->value.upvalue.type == duckVM_upvalue_type_stack_index) {
					duckLisp_object_t object = DL_ARRAY_GETADDRESS(duckVM->stack,
					                                               duckLisp_object_t,
					                                               uv->value.upvalue.value.stack_index);
					e = duckVM_push(duckVM, &object);
					if (e) goto cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto cleanup;
				}
				else if (uv->value.upvalue.type == duckVM_upvalue_type_heap_object) {
					e = duckVM_push(duckVM, uv->value.upvalue.value.heap_object);
					if (e) goto cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto cleanup;
				}
				else {
					while (uv->value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
						uv = uv->value.upvalue.value.heap_upvalue;
					}
					if (uv->value.upvalue.type == duckVM_upvalue_type_stack_index) {
						e = duckVM_push(duckVM,
						                &DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, uv->value.upvalue.value.stack_index));
						if (e) goto cleanup;
						e = duckLispDev_callback_print(duckVM);
						if (e) goto cleanup;
						e = duckVM_pop(duckVM, dl_null);
						if (e) goto cleanup;
					}
					else if (uv->value.upvalue.type == duckVM_upvalue_type_heap_object) {
						e = duckVM_push(duckVM, uv->value.upvalue.value.heap_object);
						if (e) goto cleanup;
						e = duckLispDev_callback_print(duckVM);
						if (e) goto cleanup;
						e = duckVM_pop(duckVM, dl_null);
						if (e) goto cleanup;
					}
				}
			}
			puts(")");
			break;
		case duckLisp_object_type_vector:
		printf("[");
		if (tempObject.value.vector.internal_vector != dl_null) {
			for (dl_ptrdiff_t k = tempObject.value.vector.offset;
			     (dl_size_t) k < tempObject.value.vector.internal_vector->value.internal_vector.length;
			     k++) {
				duckLisp_object_t *value = tempObject.value.vector.internal_vector->value.internal_vector.values[k];
				if (k != tempObject.value.vector.offset) putchar(' ');
				e = duckVM_push(duckVM, value);
				if (e) goto cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto cleanup;
			}
		}
		printf("]\n");
			break;
		case duckLisp_object_type_type:
			printf("::%llu\n", tempObject.value.type);
			break;
		default:
			printf("Bad object type %u.\n", tempObject.type);
		}
	}

	e = duckVM_pushNil(duckVM);

	cleanup:

	return e;
}

dl_error_t duckLispDev_callback_printUpvalueStack(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	duckLisp_object_t *tempObjectPointer = dl_null;
	duckLisp_object_t tempObject;

	printf("Call stack depth: %llu    Upvalue array call stack depth: %llu\n", duckVM->call_stack.elements_length, duckVM->upvalue_array_call_stack.elements_length);

	DL_DOTIMES(i, duckVM->upvalue_stack.elements_length) {
		dl_error_t e = dl_array_get(&duckVM->upvalue_stack, &tempObjectPointer, i);
		if (e) goto l_cleanup;
		if (tempObjectPointer == dl_null) {
			continue;
		}
		printf("%lli: ", i);
		tempObject = *tempObjectPointer;
		if (tempObject.type == duckLisp_object_type_none) continue;
		switch (tempObject.type) {
		case duckLisp_object_type_upvalue:
			if (tempObject.value.upvalue.type == duckVM_upvalue_type_stack_index) {
				duckLisp_object_t object = DL_ARRAY_GETADDRESS(duckVM->stack,
				                                               duckLisp_object_t,
				                                               tempObject.value.upvalue.value.stack_index);
				e = duckVM_push(duckVM, &object);
				if (e) goto l_cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto l_cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto l_cleanup;
			}
			else if (tempObject.value.upvalue.type == duckVM_upvalue_type_heap_object) {
				e = duckVM_push(duckVM, tempObject.value.upvalue.value.heap_object);
				if (e) goto l_cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto l_cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto l_cleanup;
			}
			else {
				while (tempObject.value.upvalue.type == duckVM_upvalue_type_heap_upvalue) {
					tempObject = *tempObject.value.upvalue.value.heap_upvalue;
				}
				if (tempObject.value.upvalue.type == duckVM_upvalue_type_stack_index) {
					e = duckVM_push(duckVM,
					                &DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, tempObject.value.upvalue.value.stack_index));
					if (e) goto l_cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto l_cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto l_cleanup;
				}
				else if (tempObject.value.upvalue.type == duckVM_upvalue_type_heap_object) {
					e = duckVM_push(duckVM, tempObject.value.upvalue.value.heap_object);
					if (e) goto l_cleanup;
					e = duckLispDev_callback_print(duckVM);
					if (e) goto l_cleanup;
					e = duckVM_pop(duckVM, dl_null);
					if (e) goto l_cleanup;
				}
			}
			putchar('\n');
			break;
		default:
			printf("Bad object type %u.\n", tempObject.type);
		}
	}

	e = duckVM_pushNil(duckVM);

	l_cleanup:

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

int duckLispDev_callback_quicksort_hoare_less(const void *l, const void *r, const void *context) {
	(void) context;
	duckLisp_object_t **left_object_ptr = (duckLisp_object_t **) l;
	duckLisp_object_t **right_object_ptr = (duckLisp_object_t **) r;
	const dl_ptrdiff_t left = (*left_object_ptr)->value.integer;
	const dl_ptrdiff_t right = (*right_object_ptr)->value.integer;
	return left - right;
}

/* This is very unsafe.
   (defun list list-length) */
dl_error_t duckLispDev_callback_quicksort_hoare(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_object_t list = DL_ARRAY_GETADDRESS(duckVM->stack, duckLisp_object_t, duckVM->stack.elements_length - 2);
	duckLisp_object_t list_length = DL_ARRAY_GETADDRESS(duckVM->stack,
	                                                    duckLisp_object_t,
	                                                    duckVM->stack.elements_length - 1);
	duckLisp_object_t **array = dl_null;
	dl_size_t array_length = list_length.value.integer;

	e = DL_MALLOC(duckVM->memoryAllocation, &array, array_length, duckLisp_object_t *);
	if (e) goto cleanup;

	DL_DOTIMES(i, array_length) {
		array[i] = list.value.list->value.cons.car;
		if (list.value.list != dl_null) {
			if ((list.value.list->value.cons.cdr == dl_null)
			    || (list.value.list->value.cons.cdr->type == duckLisp_object_type_cons)) {
				list.value.list = list.value.list->value.cons.cdr;
			}
			else {
				list = *list.value.list->value.cons.cdr;
			}
		}
	}

	/**/ quicksort_hoare(array,
	                     array_length,
	                     sizeof(duckLisp_object_t *),
	                     0,
	                     array_length - 1,
	                     duckLispDev_callback_quicksort_hoare_less,
	                     dl_null);

	duckLisp_object_t *cons = dl_null;
	DL_DOTIMES(i, array_length) {
		duckLisp_object_t tempCons;
		tempCons.value.cons.car = array[i];
		tempCons.value.cons.cdr = cons;
		tempCons.type = duckLisp_object_type_cons;
		e = duckVM_gclist_pushObject(duckVM, &cons, tempCons);
		if (e) goto cleanupMalloc;
	}

	list.value.list = cons;
	e = duckVM_pop(duckVM, dl_null);
	if (e) goto cleanup;
	e = duckVM_pop(duckVM, dl_null);
	if (e) goto cleanup;
	e = duckVM_push(duckVM, &list);
	if (e) goto cleanupMalloc;
	
 cleanupMalloc:
	eError = DL_FREE(duckVM->memoryAllocation, &array);
	if (!e) e = eError;
 cleanup:

	return e;
}

dl_error_t print_errors(dl_array_t *errors, dl_array_t *sourceCode){
	dl_error_t e = dl_error_ok;
	dl_bool_t firstLoop = dl_true;
	while (errors->elements_length > 0) {
		if (!firstLoop) putchar('\n');
		firstLoop = dl_false;

		duckLisp_error_t error;  /* Compile errors */
		e = dl_array_popElement(errors, (void *) &error);
		if (e) break;

		printf(COLOR_RED);
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < error.message_length; i++) {
			putchar(error.message[i]);
		}
		printf(COLOR_NORMAL);
		putchar('\n');

		if (error.index == -1) {
			continue;
		}

		if (sourceCode) {
			printf(COLOR_CYAN);
			for (dl_ptrdiff_t i = 0; (dl_size_t) i < sourceCode->elements_length; i++) {
				if (DL_ARRAY_GETADDRESS(*sourceCode, char, i) == '\n')
					putchar(' ');
				else
					putchar(DL_ARRAY_GETADDRESS(*sourceCode, char, i));
			}

			puts(COLOR_RED);
			for (dl_ptrdiff_t i = /* duckLisp->source.elements_length - sourceCode.elements_length */0; i < error.index; i++) {
				putchar(' ');
			}
			puts("^");

			puts(COLOR_NORMAL);
		}
	}
	return e;
}

dl_error_t duckLispDev_generator_include(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_ast_string_t fileName;
	char *cFileName = NULL;
	FILE *sourceFile = NULL;
	int tempInt = 0;
	char tempChar = '\0';
	dl_array_t sourceCode; /* dl_array_t:char */
	/**/ dl_array_init(&sourceCode, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	duckLisp_cst_compoundExpression_t cst;
	/**/ cst_compoundExpression_init(&cst);
	duckLisp_ast_compoundExpression_t ast;
	/**/ ast_compoundExpression_init(&ast);

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

	printf(COLOR_YELLOW);
	printf("(include ");
	DL_DOTIMES(i, fileName.value_length) {
		putchar(fileName.value[i]);
	}
	printf(")\n");
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

	/* printf(COLOR_YELLOW); */
	/* /\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* printf("include: Pre parse memory usage: %llu/%llu (%llu%%)\n", tempDlSize, duckLisp->memoryAllocation->size, 100*tempDlSize/duckLisp->memoryAllocation->size); */
	/* puts(COLOR_NORMAL); */

	e = duckLisp_cst_append(duckLisp, sourceCode.elements, sourceCode.elements_length, &cst, 0, dl_true);
	if (e) goto cFileName_cleanup;

	e = duckLisp_ast_append(duckLisp, sourceCode.elements, &ast, &cst, 0, dl_true);
	if (e) goto cFileName_cleanup;

	e = cst_compoundExpression_quit(duckLisp, &cst);
	if (e) goto cFileName_cleanup;

	/* printf(COLOR_YELLOW); */
	/* printf("Included AST: "); */
	/* e = ast_print_compoundExpression(*duckLisp, ast); */
	/* putchar('\n'); */
	/* if (e) goto l_cFileName_cleanup; */
	/* puts(COLOR_NORMAL); */

	/* printf(COLOR_YELLOW); */
	/* /\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* printf("include: Pre compile memory usage: %llu/%llu (%llu%%)\n", tempDlSize, duckLisp->memoryAllocation->size, 100*tempDlSize/duckLisp->memoryAllocation->size); */
	/* puts(COLOR_NORMAL); */

	e = duckLisp_generator_noscope(duckLisp, compileState, assembly, &ast.value.expression);
	if (e) goto cFileName_cleanup;

	e = ast_compoundExpression_quit(duckLisp, &ast);
	if (e) goto cFileName_cleanup;

	/* printf(COLOR_YELLOW); */
	/* /\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* printf("include: Post compile memory usage: %llu/%llu (%llu%%)\n", tempDlSize, duckLisp->memoryAllocation->size, 100*tempDlSize/duckLisp->memoryAllocation->size); */
	/* puts(COLOR_NORMAL); */

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
         duckLisp_object_t *return_value,
         const char *source,
         const dl_size_t source_length) {
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
	                                &bytecode,
	                                &bytecode_length,
	                                &DL_ARRAY_GETADDRESS(sourceCode, char, 0),
	                                sourceCode.elements_length);

	e = dl_memory_checkHealth(*duckLisp->memoryAllocation);
	if (e) {
		printf(COLOR_RED "Memory health check failed. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
	}

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

	e = print_errors(&duckLisp->errors, &sourceCode);
	if (e) goto cleanup;
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
		char *disassembly = duckLisp_disassemble(duckLisp->memoryAllocation, bytecode, bytecode_length);
		printf("%s", disassembly);
		e = dl_free(duckLisp->memoryAllocation, (void **) &disassembly);
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
	e = print_errors(&duckVM->errors, dl_null);
	if (e) goto cleanup;
	if (runtimeError) {
		printf(COLOR_RED "\nVM returned error. (%s)\n" COLOR_NORMAL, dl_errorString[runtimeError]);
		e = runtimeError;
		goto cleanupBytecode;
	}

 cleanupBytecode:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &bytecode);
	if (!e) e = eError;

	/* puts(COLOR_CYAN "}" COLOR_NORMAL); */

 cleanup:

	eError = dl_array_quit(&sourceCode);
	if (!e) e = eError;

	return e;
}

int evalFile(duckLisp_t *duckLisp, duckVM_t *duckVM, duckLisp_object_t *return_value, const char *filename) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	char tempChar;
	int tempInt;

	dl_array_t sourceCode;

	/**/ dl_array_init(&sourceCode, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Fetch script. */

	FILE *sourceFile = sourceFile = fopen(filename, "r");
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

	e = eval(duckLisp, duckVM, return_value, sourceCode.elements, sourceCode.elements_length);

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
	const size_t duckVMMaxObjects = 60000;

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
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, duckLisp_compileState_t*, dl_array_t*, duckLisp_ast_expression_t*);
	} generators[] = {
		{DL_STR("include"), duckLispDev_generator_include},
		{dl_null, 0,        dl_null}
	};

	// All user-defined callbacks go here.
	struct {
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
	} callbacks[] = {
		{DL_STR("print"),           duckLispDev_callback_print},
		{DL_STR("print-stack"),     duckLispDev_callback_printStack},
		{DL_STR("garbage-collect"), duckLispDev_callback_garbageCollect},
		{DL_STR("disassemble"),     duckLispDev_callback_toggleAssembly},
		{DL_STR("quicksort-hoare"), duckLispDev_callback_quicksort_hoare},
		{DL_STR("print-uv-stack"),  duckLispDev_callback_printUpvalueStack},
		{dl_null, 0,                dl_null}
	};

	/* Initialization. */

	if ((argc != 1) && (argc != 2)) {
		printf(COLOR_YELLOW
		       "Usage:\n"
		       "./duckLisp-dev filename                                               Run script from file\n"
		       "./duckLisp-dev \"(func0 arg0 arg1 ...) (func1 arg0 arg1 ...) ...\"      Run script from string\n"
		       "./duckLisp-dev                                                        Start REPL\n"
		       COLOR_NORMAL);
		goto l_cleanup;
	}

	tempMemory_size = duckLispMemory_size;
	duckLispMemory = malloc(tempMemory_size);
	if (duckLispMemory == NULL) {
		e = dl_error_outOfMemory;
		printf(COLOR_RED "Out of memory.\n" COLOR_NORMAL);
		goto l_cleanup;
	}
	d.duckLispMemory = dl_true;

	e = dl_memory_init(&duckLispMemoryAllocation, duckLispMemory, tempMemory_size, dl_memoryFit_best);
	if (e) {
		goto l_cleanup;
	}
	duckLisp.memoryAllocation = &duckLispMemoryAllocation;

	e = duckLisp_init(&duckLisp);
	if (e) {
		printf(COLOR_RED "Could not initialize DuckLisp. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		goto l_cleanup;
	}
	d.duckLisp_init = dl_true;

	/* Create generators. */

	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		e = duckLisp_addGenerator(&duckLisp, generators[i].callback, generators[i].name, generators[i].name_length);
		if (e) {
			printf(COLOR_RED "Could not register generator. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		}
	}

	/* Add C functions. */

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		e = duckLisp_linkCFunction(&duckLisp, callbacks[i].callback, callbacks[i].name, callbacks[i].name_length);
		if (e) {
			printf(COLOR_RED "Could not create function. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
			goto l_cleanup;
		}
	}

	tempMemory_size = duckVMMemory_size;
	duckVMMemory = malloc(tempMemory_size);
	if (duckVMMemory == NULL) {
		e = dl_error_outOfMemory;
		printf("Out of memory.\n");
		goto l_cleanup;
	}

	e = dl_memory_init(&duckVMMemoryAllocation, duckVMMemory, tempMemory_size, dl_memoryFit_best);
	if (e) {
		goto l_cleanup;
	}
	duckVM.memoryAllocation = &duckVMMemoryAllocation;

	d.duckVMMemory = dl_true;

	/* Execute. */
	e = duckVM_init(&duckVM, duckVMMaxObjects);
	if (e) {
		printf("Could not initialize VM. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	d.duckVM_init = dl_true;

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		e = duckVM_linkCFunction(&duckVM,
		                         duckLisp_symbol_nameToValue(&duckLisp, callbacks[i].name, callbacks[i].name_length),
		                         callbacks[i].callback);
		if (e) {
			printf("Could not link callback into VM. (%s)\n", dl_errorString[e]);
			goto l_cleanup;
		}
	}

	if (argc == 2) {
		FILE *sourceFile = fopen(argv[1], "r");
		if (sourceFile == NULL) {
			e = eval(&duckLisp, &duckVM, dl_null, argv[1], strlen(argv[1]));
		}
		else {
			if (fclose(sourceFile) == 0) e = evalFile(&duckLisp, &duckVM, dl_null, argv[1]);
		}
	}
	/* REPL */
	else if (argc == 1) {
		char *line = NULL;
		size_t buffer_length = 0;
		ssize_t length = 0;
		printf("(disassemble)  Toggle disassembly of forms.\n");
		while (1) {
			duckLisp_object_t return_value;
			if (duckVM.stack.elements_length > 0) {
				printf(COLOR_YELLOW
				       "A runtime error has occured. Use (print-stack) to inspect the stack, or press\n"
				       "RET to pop a stack frame."
				       COLOR_NORMAL);
				printf("\n%llu", duckVM.stack.elements_length);
			}
			printf("> ");
			if ((length = getline(&line, &buffer_length, stdin)) < 0) break;
			e = eval(&duckLisp, &duckVM, &return_value, line, length);
			free(line); line = NULL;
			e = duckVM_push(&duckVM, &return_value);
			e = duckLispDev_callback_print(&duckVM);
			e = duckVM_pop(&duckVM, dl_null);

			puts(COLOR_CYAN);
			/**/ dl_memory_usage(&tempDlSize, *duckLisp.memoryAllocation);
			printf("Compiler memory usage: %llu/%llu (%llu%%)\n",
			       tempDlSize,
			       duckLisp.memoryAllocation->size,
			       100*tempDlSize/duckLisp.memoryAllocation->size);
			/**/ dl_memory_usage(&tempDlSize, *duckVM.memoryAllocation);
			printf("VM memory usage: %llu/%llu (%llu%%)\n",
			       tempDlSize,
			       duckVM.memoryAllocation->size,
			       100*tempDlSize/duckVM.memoryAllocation->size);
			puts(COLOR_NORMAL);

		}
	}

	l_cleanup:

	puts(COLOR_CYAN);
	if (d.duckVM_init) {
		/**/ dl_memory_usage(&tempDlSize, *duckVM.memoryAllocation);
		printf("(duckVM) Current memory use: %llu/%llu (%llu%%)\n",
			   tempDlSize,
			   duckVM.memoryAllocation->size,
			   100*tempDlSize/duckVM.memoryAllocation->size);
		printf("(duckVM) Max memory used:    %llu/%llu (%llu%%)\n",
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
		printf("(duckLisp) Current memory use: %llu/%llu (%llu%%)\n",
			   tempDlSize,
			   duckLisp.memoryAllocation->size,
			   100*tempDlSize/duckLisp.memoryAllocation->size);
		printf("(duckLisp) Max memory used:    %llu/%llu (%llu%%)\n",
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
