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


dl_error_t duckLispDev_callback_printCons(duckVM_t *duckVM, duckVM_object_t *cons) {
	dl_error_t e = dl_error_ok;

	if (cons == dl_null) {
		printf("nil");
	}
	else {
		if (cons->value.cons.car == dl_null) {
			printf("nil");
		}
		else if (cons->value.cons.car->type == duckVM_object_type_cons) {
			printf("(");
			e = duckLispDev_callback_printCons(duckVM, cons->value.cons.car);
			if (e) goto cleanup;
			printf(")");
		}
		else {
			e = duckVM_push(duckVM, cons->value.cons.car);
			if (e) goto cleanup;
			e = duckLispDev_callback_print(duckVM);
			if (e) goto cleanup;
			e = duckVM_pop(duckVM, dl_null);
			if (e) goto cleanup;
		}


		if ((cons->value.cons.cdr == dl_null)
		    || (cons->value.cons.cdr->type == duckVM_object_type_cons)) {
			if (cons->value.cons.cdr != dl_null) {
				printf(" ");
				e = duckLispDev_callback_printCons(duckVM, cons->value.cons.cdr);
				if (e) goto cleanup;
			}
		}
		else {
			printf(" . ");
			e = duckVM_push(duckVM, cons->value.cons.cdr);
			if (e) goto cleanup;
			e = duckLispDev_callback_print(duckVM);
			if (e) goto cleanup;
			e = duckVM_pop(duckVM, dl_null);
			if (e) goto cleanup;
		}
	}

 cleanup:

	return e;
}

dl_error_t duckLispDev_callback_print(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;

	duckVM_object_t object;

	// e = duckVM_getArg(duckVM, &string, 0);
	e = duckVM_pop(duckVM, &object);
	if (e) goto cleanup;

	switch (object.type) {
	case duckVM_object_type_symbol:
		for (dl_size_t i = 0; i < object.value.symbol.internalString->value.internalString.value_length; i++) {
			putchar(object.value.symbol.internalString->value.internalString.value[i]);
		}
		printf("→%lu", object.value.symbol.id);
		break;
	case duckVM_object_type_string:
		if (object.value.string.internalString) {
			for (dl_size_t i = object.value.string.offset; i < object.value.string.length; i++) {
				putchar(object.value.string.internalString->value.internalString.value[i]);
			}
		}
		break;
	case duckVM_object_type_integer:
		printf("%li", object.value.integer);
		break;
	case duckVM_object_type_float:
		printf("%f\n", object.value.floatingPoint);
		break;
	case duckVM_object_type_bool:
		printf("%s", object.value.boolean ? "true" : "false");
		break;
	case duckVM_object_type_list:
		if (object.value.list == dl_null) {
			printf("nil");
		}
		else {
			/* printf("(%i: ", object.value.list->type); */
			printf("(");

			if (object.value.list->value.cons.car == dl_null) {
				printf("(nil)");
			}
			else if (object.value.list->value.cons.car->type == duckVM_object_type_cons) {
				printf("(");
				e = duckLispDev_callback_printCons(duckVM, object.value.list->value.cons.car);
				if (e) goto cleanup;
				printf(")");
			}
			else {
				e = duckVM_push(duckVM, object.value.list->value.cons.car);
				if (e) goto cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto cleanup;
			}

			if (object.value.list->value.cons.cdr == dl_null) {
			}
			else if (object.value.list->value.cons.cdr->type == duckVM_object_type_cons) {
				if (object.value.list->value.cons.cdr != dl_null) {
					printf(" ");
					e = duckLispDev_callback_printCons(duckVM, object.value.list->value.cons.cdr);
					if (e) goto cleanup;
				}
			}
			else {
				printf(" . ");
				e = duckVM_push(duckVM, object.value.list->value.cons.cdr);
				if (e) goto cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto cleanup;
			}

			printf(")");
		}
		break;
	case duckVM_object_type_closure:
		printf("(closure %li", object.value.closure.name);
		DL_DOTIMES(k, object.value.closure.upvalue_array->value.upvalue_array.length) {
			duckVM_object_t *uv = object.value.closure.upvalue_array->value.upvalue_array.upvalues[k];
			putchar(' ');
			if (uv == dl_null) {
				putchar('$');
				continue;
			}
			if (uv->value.upvalue.type == duckVM_upvalue_type_stack_index) {
				duckVM_object_t object = DL_ARRAY_GETADDRESS(duckVM->stack,
				                                               duckVM_object_t,
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
					                &DL_ARRAY_GETADDRESS(duckVM->stack,
					                                     duckVM_object_t,
					                                     uv->value.upvalue.value.stack_index));
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
		printf(")");
		break;
	case duckVM_object_type_vector:
		printf("[");
		if (object.value.vector.internal_vector != dl_null) {
			for (dl_ptrdiff_t k = object.value.vector.offset;
			     (dl_size_t) k < object.value.vector.internal_vector->value.internal_vector.length;
			     k++) {
				duckVM_object_t *value = object.value.vector.internal_vector->value.internal_vector.values[k];
				if (k != object.value.vector.offset) putchar(' ');
				e = duckVM_push(duckVM, value);
				if (e) goto cleanup;
				e = duckLispDev_callback_print(duckVM);
				if (e) goto cleanup;
				e = duckVM_pop(duckVM, dl_null);
				if (e) goto cleanup;
			}
		}
		printf("]");
		break;
	case duckVM_object_type_type:
		printf("<%lu>", object.value.type);
		break;
	case duckVM_object_type_composite:
		printf("(make-instance <%lu> ", object.value.composite->value.internalComposite.type);
		e = duckVM_push(duckVM, object.value.composite->value.internalComposite.value);
		if (e) goto cleanup;
		e = duckLispDev_callback_print(duckVM);
		if (e) goto cleanup;
		e = duckVM_pop(duckVM, dl_null);
		if (e) goto cleanup;
		printf(" ");
		e = duckVM_push(duckVM, object.value.composite->value.internalComposite.function);
		if (e) goto cleanup;
		e = duckLispDev_callback_print(duckVM);
		if (e) goto cleanup;
		e = duckVM_pop(duckVM, dl_null);
		if (e) goto cleanup;
		printf(")");
		break;
	default:
		printf("print: Unsupported type. [%u]\n", object.type);
	}

	e = duckVM_push(duckVM, &object);
	if (e) {
		goto cleanup;
	}

 cleanup:

	fflush(stdout);

	return e;
}

void printErrors(dl_array_t errors) {
	printf(COLOR_YELLOW);
	DL_DOTIMES(i, errors.elements_length) {
		duckLisp_error_t error;
		dl_array_popElement(&errors, &error);
		DL_DOTIMES(j, error.message_length) {
			putchar(error.message[j]);
		}
		putchar('\n');
	}
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
	duckVM_object_t return_value;

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

	e = duckVM_execute(&duckVM, &return_value, bytecode, bytecode_length);
	if (e) {
		puts(COLOR_YELLOW "Execution failed" COLOR_NORMAL);

		printErrors(duckVM.errors);

		goto cleanup;
	}

	if (return_value.type == duckVM_object_type_bool) {
		if (return_value.value.boolean) {
			printf(COLOR_GREEN "PASS" COLOR_NORMAL " %s\n" , fileBaseName);
		}
		else {
			e = dl_error_invalidValue;
			puts(COLOR_YELLOW "Test returned \"fail\"" COLOR_NORMAL);
		}
	}
	else {
		e = dl_error_invalidValue;
		printf(COLOR_YELLOW "Test didn't return a boolean. type: %i\n" COLOR_NORMAL, return_value.type);
	}

 cleanup:

	if (e) {
		puts("disassembly {");
		unsigned char *string = dl_null;
		dl_size_t length = 0;
		duckLisp_disassemble(&string, &length, &ma, bytecode, bytecode_length);
		printf("%s", string);
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
