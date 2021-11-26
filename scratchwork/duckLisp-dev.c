
#include <stdlib.h>
#include <stdio.h>
#include "../DuckLib/core.h"
#include "../duckLisp.h"
#include "../duckVM.h"

dl_error_t duckLispDev_callback_printString(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	
	duckLisp_object_t string;
	
	// e = duckVM_getArg(duckVM, &string, 0);
	e = duckVM_pop(duckVM, &string);
	if (e) {
		goto l_cleanup;
	}
	
	if (string.type != duckLisp_object_type_string) {
		// e = duckLisp_error_pushRuntime(duckVM, DL_STR("Argument should be a string."));
		e = e ? e : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	printf("VM: ");
	for (dl_size_t i = 0; i < string.value.string.value_length; i++) {
		putchar(string.value.string.value[i]);
	}
	
	// e = duckVM_pushReturn(duckVM, string);
	
	l_cleanup:
	
	return e;
}

dl_error_t duckLispDev_callback_printStack(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	
	duckLisp_object_t tempObject;
	
	DL_ARRAY_FOREACH(tempObject, duckVM->stack, {
		goto l_cleanup;
	}, {
		printf("%lli: ", dl_array_i);
		switch (tempObject.type) {
		case duckLisp_object_type_bool:
			puts(tempObject.value.boolean ? "true" : "false");
			break;
		case duckLisp_object_type_integer:
			printf("%llu\n", tempObject.value.integer);
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
		case duckLisp_object_type_string:
			putchar('"');
			for (dl_ptrdiff_t k = 0; k < tempObject.value.string.value_length; k++) {
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
		default:
			printf("Bad object type %u.\n", tempObject.type);
		}
	})
	putchar('\n');
	
	l_cleanup:
	
	return e;
}

/*
dl_error_t duckLisp_generator_functionCall(duckLisp_t *duckLisp) {
	dl_error_t e = dl_error_ok;
	
	// 
	// Insert arg1 into this scope's name trie.
	// Set object to type string.
	// Set object to value of arg2.
	
	l_cleanup:
	
	return e;
}
*/

dl_error_t duckLispDev_generator_createString(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t string_index = -1;
	dl_array_t *assemblyFragment = dl_null;
	
	/* Check arguments for call and type errors. */
	
	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}
	
	if (expression->compoundExpressions[1].type != ast_compoundExpression_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}
	
	if ((expression->compoundExpressions[2].type != ast_compoundExpression_type_constant) &&
	    (expression->compoundExpressions[2].value.constant.type != ast_constant_type_string)) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 2 of function \""));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR("\" should be a string."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}
	
	// Create the string variable.
	e = duckLisp_emit_pushString(duckLisp, assembly, &identifier_index, expression->compoundExpressions[2].value.constant.value.string.value,
	                                 expression->compoundExpressions[2].value.constant.value.string.value_length);
	if (e) {
		goto l_cleanup;
	}
	
	// Insert arg1 into this scope's name trie.
	e = duckLisp_scope_addObject(duckLisp, expression->compoundExpressions[1].value.constant.value.string.value,
	                             expression->compoundExpressions[1].value.constant.value.string.value_length);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}
	
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
	
	duckLisp_t duckLisp;
	void *duckLispMemory = dl_null;
	void *duckVMMemory = dl_null;
	size_t tempMemory_size;
	duckLisp_error_t error;
	const char source0[] =
	"(\n"
	"  (string t \"Hello, world!\n\n\")\n"
	"  (\n"
	"    (string s \"in-scope\n\")\n"
	"    (print-string s)\n"
	"    (print-string t))\n"
	"  (print-stack))\n";
	// const char source0[] = "(print-string (print-string (string s \"Hello, world!\")))";
	// const char source0[] = "((string s7 \"7\") (print-string s7) ((string s3 \"3\") (print-string s3) ((string s1 \"1\") (print-string s1)) ((string s2 \"2\") (print-string s2))) ((string s6 \"6\") (print-string s6) ((string s4 \"4\") (print-string s4)) ((string s5 \"5\") (print-string s5))))";
	dl_ptrdiff_t printString_index = -1;
	duckVM_t duckVM;
	unsigned char *bytecode = dl_null;
	dl_size_t bytecode_length = 0;
	duckLisp_object_t tempObject;
	
	// All user-defined generators go here.
	struct {
		const char *name;
		const dl_size_t name_length;
		const dl_error_t (*callback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*);
	} generators[] = {
		{DL_STR("string"),      duckLispDev_generator_createString},
		{dl_null, 0,            dl_null}
	};
	
	// All user-defined callbacks go here.
	struct {
		dl_ptrdiff_t index;
		const char *name;
		const dl_size_t name_length;
		const dl_error_t (*callback)(duckVM_t *);
	} callbacks[] = {
		{0, DL_STR("print-string"), duckLispDev_callback_printString},
		{0, DL_STR("print-stack"),  duckLispDev_callback_printStack},
		{0, dl_null, 0,             dl_null}
	};
	
	/* Initialization. */
	
	tempMemory_size = 1024*1024;
	duckLispMemory = malloc(tempMemory_size);
	if (duckLispMemory == NULL) {
		e = dl_error_outOfMemory;
		printf("Out of memory.\n");
		goto l_cleanup;
	}
	d.duckLispMemory = dl_true;
	
	e = duckLisp_init(&duckLisp, duckLispMemory, tempMemory_size);
	if (e) {
		printf("Could not initialize DuckLisp. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	d.duckLisp_init = dl_true;
	
	/* Create generators. */
	
	// e = duckLisp_pushGenerator(&duckLisp, DL_STR("string"), duckLispDev_generator_createString);
	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		e = duckLisp_addGenerator(&duckLisp, generators[i].callback, generators[i].name, generators[i].name_length);
		if (e) {
			printf("Could not register generator. (%s)\n", dl_errorString[e]);
		}
	}
	
	/* Add C functions. */
	
	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		e = duckLisp_linkCFunction(&duckLisp, &callbacks[i].index, callbacks[i].name, callbacks[i].name_length);
		if (e) {
			printf("Could not create function. (%s)\n", dl_errorString[e]);
			goto l_cleanup;
		}
	}
	
	/* Compile functions. */
	
	e = duckLisp_loadString(&duckLisp, &bytecode, &bytecode_length, DL_STR(source0));
	if (e) {
		printf("Error loading string. (%s)\n", dl_errorString[e]);
		
		goto l_cleanup;
	}
	
	e = dl_memory_checkHealth(duckLisp.memoryAllocation);
	if (e) {
		printf("Memory health check failed. (%s)\n", dl_errorString[e]);
	}
	
	while (dl_true) {
		e = dl_array_popElement(&duckLisp.errors, (void *) &error);
		if (e) {
			break;
		}
		
		// for (dl_ptrdiff_t i = 0; i < duckLisp.source.elements_length; i++) {
		// 	putchar(((char *) duckLisp.source.elements)[i]);
		// }
		// putchar('\n');
		
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < error.message_length; i++) {
			putchar(error.message[i]);
		}
		putchar('\n');
		
		if (error.index == -1) {
			continue;
		}
		
		printf("%s\n", source0);
		for (dl_ptrdiff_t i = duckLisp.source.elements_length - sizeof(source0); i < error.index; i++) {
			putchar(' ');
		}
		puts("^");
	}


	// Print bytecode in hex.
	for (dl_ptrdiff_t i = 0; i < bytecode_length; i++) {
		unsigned char byte = bytecode[i];
		putchar(dl_nybbleToHexChar(byte >> 4));
		putchar(dl_nybbleToHexChar(byte));
	}
	putchar('\n');
	putchar('\n');
	
	// Note: The memSize/eleSize trick only works with the "fit" strategy.
	for (dl_ptrdiff_t i = 0; i < duckLisp.scope_stack.elements_memorySize / duckLisp.scope_stack.element_size; i++) {
		printf("Scope %lli: locals\n", i);
		/**/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp.scope_stack.elements)[i].locals_trie);
	}
	putchar('\n');
	for (dl_ptrdiff_t i = 0; i < duckLisp.scope_stack.elements_memorySize / duckLisp.scope_stack.element_size; i++) {
		printf("Scope %lli: statics\n", i);
		/**/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp.scope_stack.elements)[i].statics_trie);
	}
	putchar('\n');
	for (dl_ptrdiff_t i = 0; i < duckLisp.scope_stack.elements_memorySize / duckLisp.scope_stack.element_size; i++) {
		printf("Scope %lli: generators\n", i);
		/**/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp.scope_stack.elements)[i].generators_trie);
	}
	putchar('\n');
	for (dl_ptrdiff_t i = 0; i < duckLisp.scope_stack.elements_memorySize / duckLisp.scope_stack.element_size; i++) {
		printf("Scope %lli: functions (1: callback  2: script  3: generator)\n", i);
		/**/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp.scope_stack.elements)[i].functions_trie);
	}
	putchar('\n');
	
	// /**/ duckLisp_call(&duckLisp, "hello-world");
	
	tempMemory_size = 1024*1024;
	duckVMMemory = malloc(tempMemory_size);
	if (duckVMMemory == NULL) {
		e = dl_error_outOfMemory;
		printf("Out of memory.\n");
		goto l_cleanup;
	}
	d.duckVMMemory = dl_true;
	
	/* Execute. */
	e = duckVM_init(&duckVM, duckVMMemory, tempMemory_size);
	if (e) {
		printf("Could not initialize VM. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	d.duckVM_init = dl_true;

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		e = duckVM_linkCFunction(&duckVM, callbacks[i].index, callbacks[i].callback);
		if (e) {
			printf("Could not link callback into VM. (%s)\n", dl_errorString[e]);
			goto l_cleanup;
		}
	}
	
	putchar('\n');
	
	e = duckVM_execute(&duckVM, bytecode);
	if (e) {
		goto l_cleanup;
	}
	putchar('\n');

	// e = duckVM_call(&duckVM, helloWorld_index);
	// if (e) {
	// 	printf("Function call failed. (%s)\n", dl_errorString[e]);
	// 	goto l_cleanup;
	// }
	
	l_cleanup:
	
	if (d.duckVM_init) {
		puts("");
		printf("(duckVM) Memory in use:   %llu\n", duckVM.memoryAllocation.used);
		printf("(duckVM) Max memory used: %llu\n", duckVM.memoryAllocation.max_used);
		// dl_memory_printMemoryAllocation(duckLisp.memoryAllocation);
		// Clear 1 MiB of memory. Is surprisingly fast. I'm mainly did this to see how simple it was.
		/**/ dl_memclear(duckVM.memoryAllocation.memory, duckVM.memoryAllocation.size * sizeof(char));
		/**/ duckVM_quit(&duckVM);
	}
	if (d.duckVMMemory) {
		puts("Freeing VM memory.");
		/**/ free(duckVMMemory); duckVMMemory = NULL;
	}
	
	if (d.duckLisp_init) {
		puts("");
		printf("(duckLisp) Memory in use:   %llu\n", duckLisp.memoryAllocation.used);
		printf("(duckLisp) Max memory used: %llu\n", duckLisp.memoryAllocation.max_used);
		// dl_memory_printMemoryAllocation(duckLisp.memoryAllocation);
		// Clear 1 MiB of memory. Is surprisingly fast. I'm mainly did this to see how simple it was.
		/**/ dl_memclear(duckLisp.memoryAllocation.memory, duckLisp.memoryAllocation.size * sizeof(char));
		/**/ duckLisp_quit(&duckLisp);
	}
	if (d.duckLispMemory) {
		puts("Freeing compiler memory.");
		/**/ free(duckLispMemory); duckLispMemory = NULL;
	}
	
	return e;
}
