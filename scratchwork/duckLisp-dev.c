
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include "../DuckLib/core.h"
#include "../duckLisp.h"
#include "../duckVM.h"
#include "DuckLib/memory.h"


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


dl_error_t duckLispDev_callback_print(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	
	duckLisp_object_t object;
	
	// e = duckVM_getArg(duckVM, &string, 0);
	e = duckVM_pop(duckVM, &object);
	if (e) {
		goto l_cleanup;
	}
	
	switch (object.type) {
	case duckLisp_object_type_string:
		for (dl_size_t i = 0; i < object.value.string.value_length; i++) {
			putchar(object.value.string.value[i]);
		}
		break;
	case duckLisp_object_type_integer:
		printf("%lli", object.value.integer);
		break;
	case duckLisp_object_type_bool:
		printf("%s", object.value.boolean ? "true" : "false");
		break;
	default:
		printf("print: Unsupported type.\n");
	}
	
	e = duckVM_push(duckVM, &object);
	if (e) {
		goto l_cleanup;
	}
	
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
	duckLisp_error_t error; // Compile errors.
	dl_error_t loadError;
	
	FILE *sourceFile = dl_null;
	dl_array_t sourceCode;
	int tempInt;
	char tempChar;
	dl_ptrdiff_t printString_index = -1;
	duckVM_t duckVM;
	unsigned char *bytecode = dl_null;
	dl_size_t bytecode_length = 0;
	duckLisp_object_t tempObject;
	
	// All user-defined generators go here.
	struct {
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*);
	} generators[] = {
		{dl_null, 0,            dl_null}
	};
	
	// All user-defined callbacks go here.
	struct {
		dl_ptrdiff_t index;
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
	} callbacks[] = {
		{0, DL_STR("print"),        duckLispDev_callback_print},
		{0, DL_STR("print-stack"),  duckLispDev_callback_printStack},
		{0, dl_null, 0,             dl_null}
	};
	
	/* Initialization. */
	
	if (argc != 2) {
		printf(COLOR_YELLOW "Requires a filename as an argument.\n" COLOR_NORMAL);
		goto l_cleanup;
	}
	
	tempMemory_size = 1024*1024;
	duckLispMemory = malloc(tempMemory_size);
	if (duckLispMemory == NULL) {
		e = dl_error_outOfMemory;
		printf(COLOR_RED "Out of memory.\n" COLOR_NORMAL);
		goto l_cleanup;
	}
	d.duckLispMemory = dl_true;
	
	e = duckLisp_init(&duckLisp, duckLispMemory, tempMemory_size);
	if (e) {
		printf(COLOR_RED "Could not initialize DuckLisp. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		goto l_cleanup;
	}
	d.duckLisp_init = dl_true;
	
	/**/ dl_array_init(&sourceCode, &duckLisp.memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	/* Create generators. */
	
	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		e = duckLisp_addGenerator(&duckLisp, generators[i].callback, generators[i].name, generators[i].name_length);
		if (e) {
			printf(COLOR_RED "Could not register generator. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		}
	}
	
	/* Add C functions. */
	
	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		e = duckLisp_linkCFunction(&duckLisp, &callbacks[i].index, callbacks[i].name, callbacks[i].name_length);
		if (e) {
			printf(COLOR_RED "Could not create function. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
			goto l_cleanup;
		}
	}
	
	/* Fetch script. */

	// Create implicit progn.
	e = dl_array_pushElements(&sourceCode, DL_STR("((;) "));
	if (e) {
		goto l_cleanup;
	}
	
	sourceFile = fopen(argv[1], "r");
	if (sourceFile == NULL) {
		printf(COLOR_RED "Could not open file \"%s\".\n" COLOR_NORMAL, argv[1]);
		e = dl_error_nullPointer;
		goto l_cleanup;
	}
	while ((tempInt = fgetc(sourceFile)) != EOF) {
		tempChar = tempInt & 0xFF;
		e = dl_array_pushElement(&sourceCode, &tempChar);
		if (e) {
			fclose(sourceFile);
			goto l_cleanup;
		}
	}
	fclose(sourceFile);

	tempChar = ')';
	e = dl_array_pushElement(&sourceCode, &tempChar);
	if (e) {
		goto l_cleanup;
	}
	
	
	/* Compile functions. */
	
	// e = duckLisp_loadString(&duckLisp, &bytecode, &bytecode_length, DL_STR(source0));
	loadError = duckLisp_loadString(&duckLisp, &bytecode, &bytecode_length, &DL_ARRAY_GETADDRESS(sourceCode, char, 0),
	                        sourceCode.elements_length);
	
	e = dl_memory_checkHealth(duckLisp.memoryAllocation);
	if (e) {
		printf(COLOR_RED "Memory health check failed. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
	}

	printf(COLOR_CYAN);
	
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
	for (dl_ptrdiff_t i = 0; i < duckLisp.scope_stack.elements_memorySize / duckLisp.scope_stack.element_size; i++) {
		printf("Scope %lli: labels\n", i);
		/**/ dl_trie_print_compact(((duckLisp_scope_t *) duckLisp.scope_stack.elements)[i].labels_trie);
	}

	printf(COLOR_NORMAL);
	
	if (loadError) {
		printf(COLOR_RED "\nError loading string. (%s)\n" COLOR_NORMAL, dl_errorString[loadError]);
	}

	while (dl_true) {
		putchar('\n');
		
		e = dl_array_popElement(&duckLisp.errors, (void *) &error);
		if (e) {
			break;
		}

		printf(COLOR_RED);
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < error.message_length; i++) {
			putchar(error.message[i]);
		}
		printf(COLOR_NORMAL);
		putchar('\n');
		
		if (error.index == -1) {
			continue;
		}
		
		printf(COLOR_CYAN);
		for (dl_ptrdiff_t i = 0; i < sourceCode.elements_length; i++) {
			if (DL_ARRAY_GETADDRESS(sourceCode, char, i) == '\n')
				putchar(' ');
			else
				putchar(DL_ARRAY_GETADDRESS(sourceCode, char, i));
		}
		
		puts(COLOR_RED);
		for (dl_ptrdiff_t i = duckLisp.source.elements_length - sourceCode.elements_length; i < error.index; i++) {
			putchar(' ');
		}
		puts("^");
		puts(COLOR_NORMAL);
	}

	if (loadError) {
		putchar('\n');
		printf(COLOR_RED "Failed to compile source.\n" COLOR_NORMAL);
		e = loadError;
		goto l_cleanup;
	}

	{
		char *disassembly = duckLisp_disassemble(&duckLisp.memoryAllocation, bytecode, bytecode_length);
		printf("%s", disassembly);
		e = dl_free(&duckLisp.memoryAllocation, (void **) &disassembly);
	}
	putchar('\n');
	
	// Print bytecode in hex.
	for (dl_ptrdiff_t i = 0; i < bytecode_length; i++) {
		unsigned char byte = bytecode[i];
		putchar(dl_nybbleToHexChar(byte >> 4));
		putchar(dl_nybbleToHexChar(byte));
	}
	putchar('\n');
	
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
	puts(COLOR_CYAN "VM: {" COLOR_NORMAL);
	
	e = duckVM_execute(&duckVM, bytecode);
	if (e) {
		printf(COLOR_RED "\nVM returned error. (%s)\n" COLOR_NORMAL, dl_errorString[e]);
		goto l_cleanup;
	}
	
	puts(COLOR_CYAN "}" COLOR_NORMAL);

	// e = duckVM_call(&duckVM, helloWorld_index);
	// if (e) {
	// 	printf("Function call failed. (%s)\n", dl_errorString[e]);
	// 	goto l_cleanup;
	// }
	
	l_cleanup:
	
	puts(COLOR_CYAN);
	if (d.duckVM_init) {
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
	printf(COLOR_NORMAL);
	
	return e;
}
