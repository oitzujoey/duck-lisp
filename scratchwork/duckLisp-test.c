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
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include "../duckLisp.h"
#include "../duckVM.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
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

dl_bool_t g_disassemble = dl_false;

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
		if (nil || (type == duckVM_object_type_cons)) {
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
			else if (type == duckVM_object_type_cons) {
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
			putchar(' ');
			e = duckVM_pushElement(duckVM, k);
			if (e) break;
			/* stack: closure closure[k] */
			e = duckLispDev_callback_print(duckVM);
			if (e) goto cleanup;
			/* stack: closure closure[k] */
			e = duckVM_pop(duckVM);
			if (e) goto cleanup;
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

void printErrors(dl_array_t errors) {
	printf(COLOR_YELLOW);
	DL_DOTIMES(i, errors.elements_length) {
		dl_uint8_t error;
		dl_array_popElement(&errors, &error);
		putchar(error);
	}
	putchar('\n');
	printf(COLOR_NORMAL);
}

dl_error_t runTest(const unsigned char *fileBaseName, dl_uint8_t *text, size_t text_length) {
	dl_error_t e = dl_error_ok;

	const size_t duckLispMemory_size = 1024 * 1024;
	const size_t duckVMMaxObjects = 1024;

	void *memory = NULL;
	dl_memoryAllocation_t ma;
	duckLisp_t duckLisp = {0};
	unsigned char *bytecode = NULL;
	dl_size_t bytecode_length;
	duckVM_t duckVM = {0};
	duckVM_object_type_t objectType;

	memory = malloc(duckLispMemory_size);
	if (memory == NULL) {
		e = dl_error_outOfMemory;
		perror("Test malloc failed");
		goto cleanup;
	}

	e = dl_memory_init(&ma, memory, duckLispMemory_size, dl_memoryFit_best);
	if (e) {
		puts(COLOR_YELLOW "Memory allocation initialization failed" COLOR_NORMAL);
		goto cleanup;
	}

	e = duckLisp_init(&duckLisp,
	                  &ma,
	                  duckVMMaxObjects
#ifdef USE_PARENTHESIS_INFERENCE
	                  ,
	                  0
#endif /* USE_PARENTHESIS_INFERENCE */
	                  );
	if (e) {
		puts(COLOR_YELLOW "Compiler initialization failed" COLOR_NORMAL);
		goto cleanup;
	}

	e = duckLisp_loadString(&duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
	                        dl_false,
#endif /* USE_PARENTHESIS_INFERENCE */
	                        &bytecode,
	                        &bytecode_length,
	                        text,
	                        text_length - 1,
	                        fileBaseName,
	                        strlen((const char *) fileBaseName));
	if (e) {
		puts(COLOR_YELLOW "Compilation failed" COLOR_NORMAL);

		printErrors(duckLisp.errors);

		goto cleanup;
	}

	e = duckVM_init(&duckVM, &ma, duckVMMaxObjects);
	if (e) {
		puts(COLOR_YELLOW "VM initialization failed" COLOR_NORMAL);
		goto cleanup;
	}

	e = duckVM_execute(&duckVM, bytecode, bytecode_length);
	if (e) {
		puts(COLOR_YELLOW "Execution failed" COLOR_NORMAL);

		printErrors(duckVM.errors);

		goto cleanup;
	}

	e = duckVM_typeOf(&duckVM, &objectType);
	if (e) goto cleanup;
	if (objectType == duckVM_object_type_bool) {
		dl_bool_t returnedBoolean;
		e = duckVM_copyBoolean(&duckVM, &returnedBoolean);
		if (e) goto cleanup;
		if (returnedBoolean) {
			printf(COLOR_GREEN "PASS" COLOR_NORMAL " %s\n" , fileBaseName);
		}
		else {
			e = dl_error_invalidValue;
			puts(COLOR_YELLOW "Test returned \"fail\"" COLOR_NORMAL);
		}
	}
	else {
		e = dl_error_invalidValue;
		printf(COLOR_YELLOW "Test didn't return a boolean. type: %i\n" COLOR_NORMAL, objectType);
	}
	/* Pop return value. */
	e = duckVM_pop(&duckVM);
	if (e) goto cleanup;

 cleanup:

	if (e) {
		puts("disassembly {");
		dl_array_t string;
		duckLisp_disassemble(&string, &ma, bytecode, bytecode_length);
		printf("%s", (char *) string.elements);
		puts("}");
		printf(COLOR_RED "FAIL" COLOR_NORMAL " %s\n", fileBaseName);
	}

	(void) duckVM_quit(&duckVM);
	(void) duckLisp_quit(&duckLisp);
	(void) dl_memory_quit(&ma);
	(void) free(memory);

	return e;
}

int main(int argc, char *argv[]) {
	dl_error_t e = dl_error_ok;

	const char *directoryName = NULL;
	size_t directoryName_length = 0;
	DIR *directory = NULL;

	if (argc != 2) {
		printf("Usage: %s <tests directory>\n", argv[0]);
		e = dl_error_invalidValue;
		goto cleanup;
	}

	directoryName = argv[1];
	directoryName_length = strlen(directoryName);
	directory = opendir(directoryName);
	if (directory == NULL) {
		e = dl_error_invalidValue;
		perror("Could not open directory");
		goto cleanup;
	}

	while (true) {
		struct dirent *dirent = readdir(directory);
		if (dirent == NULL) break;
		if (dirent->d_type != DT_REG) continue;

		const unsigned char *fileBaseName = (unsigned char *) dirent->d_name;

		char *extension = strrchr((const char *) fileBaseName, '.');
		if (extension == NULL) continue;
		extension++;
		if (0 != strcmp(extension, "dl")) continue;

		// +1 for '/'. +1 for '\0'.
		const size_t path_length = directoryName_length + 1 + strlen((const char *) fileBaseName) + 1;
		char *path = malloc(sizeof(char) * path_length);
		if (path == NULL) {
			// This might be recoverable, but fail anyway.
			e = dl_error_outOfMemory;
			perror("Malloc failed");
			goto cleanup;
		}
		sprintf(path, "%s/%s", directoryName, fileBaseName);

		{
			FILE *file = fopen(path, "r");
			if (file == NULL) {
				// This might be recoverable.
				perror("Could not open file");
				goto testCleanup;
			}

			size_t text_memory_length = 2*1024;
			unsigned char *text = malloc(text_memory_length * sizeof(unsigned char));
			size_t text_length = 0;
			if (text == NULL) {
				e = dl_error_outOfMemory;
				perror("Malloc failed");
				goto testCleanup;
			}

			bool end = false;
			while (true) {
				int c = fgetc(file);
				if (c == EOF) {
					end = true;
					c = '\0';
				}
				if (text_length >= text_memory_length) {
					text_memory_length *= 2;
					text = realloc(text, text_memory_length * sizeof(unsigned char));
				}
				text[text_length++] = c;
				if (end) break;
			}

			e = runTest(fileBaseName, text, text_length);
			if (e == dl_error_outOfMemory) {
				goto testCleanup;
			}
			e = dl_error_ok;

		testCleanup:
			if (text != NULL) free(text);
			if (file != NULL) fclose(file);

			if (e) goto cleanup;
		}
	}

 cleanup:

	if (directory != NULL) (void) closedir(directory);

	return e;
}
