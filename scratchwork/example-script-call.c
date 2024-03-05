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
 (defun div-mod (n d)\
   var quotient (/ n d)\
   cons quotient\
        - n (* quotient d))\
 global div-mod #div-mod)";
	size_t source_length = sizeof(source)/sizeof(*source) - 1;

	uint8_t *bytecode = NULL;
	size_t bytecode_length = 0;
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

	// Execute the bytecode to define the global function.
	dl_error_t runtimeError = duckVM_execute(&duckVM,
	                                         bytecode,
	                                         bytecode_length);
	if (runtimeError) {
		e = print_errors("VM execution failed.", &duckVM.errors);
		goto cleanup;
	}
	// We don't care about the result.
	e = duckVM_pop(&duckVM);
	if (e) goto cleanup;

	// Push the function onto the stack.
	dl_ptrdiff_t key = duckLisp_symbol_nameToValue(&duckLisp,
	                                               DL_STR("div-mod"));
	e = duckVM_pushGlobal(&duckVM, key);
	if (e) {
		printf("Could not find global function \"div-mod\".\n");
		goto cleanup;
	}
	// stack: div-mod
	// Push the arguments in the order you would normally pass them in a
	// duck-lisp script.
	e = duckVM_pushInteger(&duckVM);
	if (e) goto cleanup;
	// stack: div-mod 0
	e = duckVM_setInteger(&duckVM, 661);
	if (e) goto cleanup;
	// stack: div-mod 661
	e = duckVM_pushInteger(&duckVM);
	if (e) goto cleanup;
	// stack: div-mod 661 0
	e = duckVM_setInteger(&duckVM, 491);
	if (e) goto cleanup;
	// stack: div-mod 661 491
	// Call the function, specifying that we pushed 2 arguments.
	// `stackIndex` = -3, because the function is in the third position on the
	// stack, counting from the top. This could have also been written
	// `-args_length-1`.
	runtimeError = duckVM_call(&duckVM, -3, 2);
	if (runtimeError) {
		e = print_errors("VM call failed.", &duckVM.errors);
		goto cleanup;
	}
	// stack: return-value
	// Fetch the return values. Since it's a cons, we have to extract the
	// integers from its CAR and CDR and then pop everything off.
	dl_bool_t isCons;
	e = duckVM_isCons(&duckVM, &isCons);
	if (e) goto cleanup;
	if (!isCons) {
		printf("Returned object is not a cons.\n");
		e = dl_error_invalidValue;
		goto cleanup;
	}
	// stack: (cons car cdr)

	e = duckVM_pushCar(&duckVM);
	if (e) goto cleanup;
	// stack: (cons car cdr) car
	dl_bool_t isInteger;
	e = duckVM_isInteger(&duckVM, &isInteger);
	if (e) goto cleanup;
	if (!isInteger) {
		printf("Car of returned object is not an integer.\n");
		e = dl_error_invalidValue;
		goto cleanup;
	}
	// stack: (cons quotient cdr) quotient
	dl_ptrdiff_t quotient;
	e = duckVM_copySignedInteger(&duckVM, &quotient);
	if (e) goto cleanup;
	e = duckVM_pop(&duckVM);
	if (e) goto cleanup;
	// stack: (cons quotient cdr)

	e = duckVM_pushCdr(&duckVM);
	if (e) goto cleanup;
	// stack: (cons quotient cdr) cdr
	e = duckVM_isInteger(&duckVM, &isInteger);
	if (e) goto cleanup;
	if (!isInteger) {
		printf("Cdr of returned object is not an integer.\n");
		e = dl_error_invalidValue;
		goto cleanup;
	}
	// stack: (cons quotient remainder) remainder
	dl_ptrdiff_t remainder;
	e = duckVM_copySignedInteger(&duckVM, &remainder);
	if (e) goto cleanup;
	e = duckVM_popSeveral(&duckVM, 2);
	if (e) goto cleanup;
	// stack: EMPTY

	// Adjust format string for your platform. The result should be 36.
	printf("VM: %li . %li\n", quotient, remainder);

 cleanup:
	(void) duckVM_quit(&duckVM);
	(void) duckLisp_quit(&duckLisp);

	// These two lines are only required if using the DuckLib allocator.
	(void) dl_memory_quit(&duckLispMemoryAllocation);
	(void) free(duckLisp_memory);

	return e != 0;
}
