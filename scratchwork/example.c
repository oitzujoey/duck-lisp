#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* The compiler */
#include "DuckLib/core.h"
#include "duckLisp.h"
/* The VM */
#include "duckVM.h"


// Print compilation errors.
dl_error_t print_errors(char *message, dl_array_t *errors){
	puts(message);

	dl_uint8_t *errorString = errors->elements;
	dl_size_t errorString_length = errors->elements_length;

	for (size_t i = 0; i < errorString_length; i++) {
		putchar(errorString[i]);
	}
	putchar('\n');

	return dl_error_invalidValue;
}


dl_error_t callback_helloWorld(duckVM_t *duckVM) {
	puts("Hello, world!");
	return duckVM_pushNil(duckVM);
}

dl_error_t callback_println(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	// stack: object

	dl_uint8_t *string = NULL;
	dl_size_t string_length = 0;
	dl_bool_t isString = dl_false;

	e = duckVM_isString(duckVM, &isString);
	if (e) goto cleanup;
	if (!isString) {
		e = dl_error_invalidValue;
		// Add a helpful error message for the user.
		(eError
		 = duckVM_error_pushRuntime(duckVM,
		                            DL_STR("println: Argument is not a string.")));
		if (eError) e = eError;
		goto cleanup;
	}
	// stack: string

	e = duckVM_copyString(duckVM, &string, &string_length);
	if (e) goto cleanup;
	// Keep the stack balanced; consume all arguments we were passed.
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;
	// stack: EMPTY

	/* The macro below is shorthand for
	   `for (dl_ptrdiff_t i = 0; (dl_size_t) i < string_length; i++)` */
	DL_DOTIMES(i, string_length) {
		putchar(string[i]);
	}
	putchar('\n');

	e = duckVM_pushInteger(duckVM);
	if (e) goto cleanup;
	// stack: 0
	e = duckVM_setInteger(duckVM, string_length);
	if (e) goto cleanup;
	// stack: string_length

 cleanup:
	if (string != NULL) {
		// You can use C's `free` instead if not using DuckLib's allocator.
		eError = dl_free(duckVM->memoryAllocation, (void **) &string);
		if (eError) e = eError;
	}
	return e;
}

dl_error_t callback_divMod(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	// stack: dividend divisor

	// Check type of divisor (second argument).
	{
		dl_bool_t isInteger;
		e = duckVM_isInteger(duckVM, &isInteger);
		if (e) goto cleanup;
		if (!isInteger) {
			e = dl_error_invalidValue;
			(eError
			 = duckVM_error_pushRuntime(duckVM,
			                            DL_STR("div-mod: Divisor is not an integer.")));
			if (eError) e = eError;
			goto cleanup;
		}
	}
	// Fetch divisor.
	dl_ptrdiff_t divisor;
	e = duckVM_copySignedInteger(duckVM, &divisor);
	if (e) goto cleanup;
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;
	// stack: dividend

	// Check type of dividend (first argument).
	{
		dl_bool_t isInteger;
		e = duckVM_isInteger(duckVM, &isInteger);
		if (e) goto cleanup;
		if (!isInteger) {
			e = dl_error_invalidValue;
			(eError
			 = duckVM_error_pushRuntime(duckVM,
			                            DL_STR("div-mod: Dividend is not an integer.")));
			if (eError) e = eError;
			goto cleanup;
		}
	}
	// Fetch divisor.
	dl_ptrdiff_t dividend;
	e = duckVM_copySignedInteger(duckVM, &dividend);
	if (e) goto cleanup;
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;
	// stack: EMPTY

	// Do the interesting operations.
	dl_ptrdiff_t quotient = dividend / divisor;
	dl_ptrdiff_t remainder = dividend % divisor;

	// Push the cons that will bind the quotient and remainder together.
	e = duckVM_pushCons(duckVM);
	if (e) goto cleanup;
	// stack: (cons NIL NIL)

	// Push the quotient.
	e = duckVM_pushInteger(duckVM);
	if (e) goto cleanup;
	// stack: (cons NIL NIL) 0
	e = duckVM_setInteger(duckVM, quotient);
	if (e) goto cleanup;
	// stack: (cons NIL NIL) quotient
	// Assign the quotient to the CAR of the cons.
	// -2 is the index of the cons.
	e = duckVM_setCar(duckVM, -2);
	if (e) goto cleanup;
	// stack: (cons quotient NIL) quotient
	// Pop the quotient, which now has a safe copy in the cons.
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;
	// stack: (cons quotient NIL)

	// Do the same for the remainder.
	e = duckVM_pushInteger(duckVM);
	if (e) goto cleanup;
	// stack: (cons quotient NIL) 0
	e = duckVM_setInteger(duckVM, remainder);
	if (e) goto cleanup;
	// stack: (cons quotient NIL) remainder
	e = duckVM_setCdr(duckVM, -2);
	if (e) goto cleanup;
	// stack: (cons quotient remainder) remainder
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;
	// stack: (cons quotient remainder)

 cleanup: return e;
}


int main(void) {
	// Many functions return DuckLib error codes.
	dl_error_t e = dl_error_ok;

	duckLisp_t duckLisp;
	duckVM_t duckVM;

	// You only need this if using the DuckLib allocator!

	dl_memoryAllocation_t duckLispMemoryAllocation;
	const size_t duckLisp_memory_size = 10000000;
	void *duckLisp_memory = NULL;

	// Allocate a big hunk of memory to use as the heap.
	duckLisp_memory = malloc(duckLisp_memory_size);
	if (duckLisp_memory == NULL) {
		printf("malloc failed.\n");
		// You can use your own error handling mechanism here, but this is my
		// code, so we're using `dl_error_t`. ;-)
		e = dl_error_invalidValue;
		goto cleanup;
	}
	// `dl_memoryFit_best` means to find a chunk of memory that most closely
	// fits the specified size.
	e = dl_memory_init(&duckLispMemoryAllocation,
	                   duckLisp_memory,
	                   duckLisp_memory_size,
	                   dl_memoryFit_best);
	if (e) {
		printf("Failed to initialize memory allocator\n");
		goto cleanup;
	}


	// Pass `NULL` as the second argument if not using DuckLib allocator.
	// Limit the maximum number of objects allocated in the compile-time VM to
	// 100.
	// Limit the maximum number of objects allocated in the inference-time VM to
	// 100. This last parameter only exists if you built with inference enabled.
	e = duckLisp_init(&duckLisp, &duckLispMemoryAllocation, 100, 100);
	if (e) {
		e = print_errors("Failed to initialize duck-lisp compiler.",
		                 &duckLisp.errors);
		goto cleanup;
	}

	// Limit the maximum number of objects allocated in the VM to 10.
	const size_t objectHeap_size = 10;

	// Pass `NULL` as the second argument if not using DuckLib allocator.
	e = duckVM_init(&duckVM, &duckLispMemoryAllocation, objectHeap_size);
	if (e) {
		printf("Failed to initialize duck-lisp VM\n");
		goto cleanup;
	}

	dl_uint8_t source[] = "\
(()\
 var result (div-mod 661 491)\
 + car result\
   cdr result)";
	size_t source_length = sizeof(source)/sizeof(*source) - 1;

	uint8_t *bytecode = NULL;
	size_t bytecode_length = 0;

	struct {
		dl_uint8_t *name;
		dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
		dl_uint8_t *type;
		dl_size_t type_length;
	} callbacks[] = {
		{DL_STR("hello-world"), callback_helloWorld, DL_STR("()")},
		{DL_STR("println"), callback_println, DL_STR("(I)")},
		{DL_STR("div-mod"), callback_divMod, DL_STR("(I I)")},
	};
	const dl_size_t callbacks_length = sizeof(callbacks)/sizeof(*callbacks);

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < callbacks_length; i++) {
		e = duckLisp_linkCFunction(&duckLisp,
		                           callbacks[i].callback,
		                           callbacks[i].name,
		                           callbacks[i].name_length,
		                           // These two arguments are not required if
		                           // parenthesis inference is disabled.
		                           callbacks[i].type,
		                           callbacks[i].type_length);
		if (e) {
			printf("Failed to register C callback \"%s\" with compiler.\n",
			       callbacks[i].name);
			goto cleanup;
		}
	}

	dl_error_t loadError = duckLisp_loadString(&duckLisp,
	                                           // Parenthesis inference is
	                                           // enabled:
	                                           true,
	                                           &bytecode,
	                                           &bytecode_length,
	                                           source,
	                                           source_length,
	                                           DL_STR("<No file>"));
	if (loadError) {
		e = print_errors("Compilation failed.", &duckLisp.errors);
		goto cleanup;
	}

	DL_DOTIMES(i, callbacks_length) {
		ptrdiff_t callback_id = duckLisp_symbol_nameToValue(&duckLisp,
		                                                    callbacks[i].name,
		                                                    (callbacks[i]
		                                                     .name_length));
		if (callback_id == -1) {
			printf("\"%s\" could not be found in the compiler's environment.\n",
			       callbacks[i].name);
			e = dl_error_invalidValue;
			goto cleanup;
		}
		e = duckVM_linkCFunction(&duckVM, callback_id, callbacks[i].callback);
		if (e) {
			printf("Failed to register C callback with VM.\n");
			goto cleanup;
		}
	}

	dl_error_t runtimeError = duckVM_execute(&duckVM,
	                                         bytecode,
	                                         bytecode_length);
	if (runtimeError) {
		e = print_errors("VM execution failed.", &duckVM.errors);
		goto cleanup;
	}

	dl_bool_t isInteger;
	e = duckVM_isInteger(&duckVM, &isInteger);
	if (e) goto cleanup;
	if (!isInteger) {
		printf("Returned object is not an integer.\n");
		e = dl_error_invalidValue;
		goto cleanup;
	}
	dl_ptrdiff_t returnedInteger;
	e = duckVM_copySignedInteger(&duckVM, &returnedInteger);
	if (e) goto cleanup;
	/* Pop return value to balance the stack. */
	e = duckVM_pop(&duckVM);
	if (e) goto cleanup;

	// Adjust format string for your platform. The result should be 36.
	printf("VM: %li\n", returnedInteger);

 cleanup:
	(void) duckVM_quit(&duckVM);
	(void) duckLisp_quit(&duckLisp);

	// These two lines are only required if using the DuckLib allocator.
	(void) dl_memory_quit(&duckLispMemoryAllocation);
	(void) free(duckLisp_memory);

	return e != 0;
}
