
#include <stdlib.h>
#include <stdio.h>
#include "../DuckLib/core.h"
#include "../duckLisp.h"

int main(int argc, char *argv[]) {
	dl_error_t e = dl_error_ok;
	struct {
		dl_bool_t duckLispMemory;
		dl_bool_t duckLisp_init;
	} d = {0};
	
	duckLisp_t duckLisp;
	void *duckLispMemory = dl_null;
	size_t tempMemory_size;
	duckLisp_error_t error;
	dl_ptrdiff_t scriptHandles[3];
	const char source0[] = "((string s \"Hello, world!\") (print s))";
	const char source1[] = "((int i -5) (bool b true) (bool b false) (print i))";
	const char source2[] = "((float f 1.4e656) (float f0 -1.4e656) (float f1 .4e6) (float f2 1.4e-656) (float f3 -.4e6) (float f3 -10.e-2) (echo 'float) (print f))";
	
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
	
	e = duckLisp_loadString(&duckLisp, &scriptHandles[0], DL_STR(source0));
	if (e) {
		printf("Error loading string. (%s)\n", dl_errorString[e]);
		
		while (dl_true) {
			e = array_popElement(&duckLisp.errors, (void *) &error);
			if (e) {
				break;
			}
			
			for (dl_ptrdiff_t i = 0; i < duckLisp.source.elements_length; i++) {
				putchar(((char *) duckLisp.source.elements)[i]);
			}
			putchar('\n');
			
			for (dl_ptrdiff_t i = 0; i < error.message_length; i++) {
				putchar(error.message[i]);
			}
			putchar('\n');
			
			printf("%s\n", source0);
			for (dl_ptrdiff_t i = duckLisp.source.elements_length - sizeof(source0); i < error.index; i++) {
				putchar(' ');
			}
			puts("^");
		}
		
		goto l_cleanup;
	}
	
	e = dl_memory_checkHealth(duckLisp.memoryAllocation);
	if (e) {
		printf("Memory health check failed. (%s)\n", dl_errorString[e]);
	}
	
	e = duckLisp_cst_print(&duckLisp);
	if (e) {
		printf("Error printing DuckLisp CST. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	
	e = duckLisp_ast_print(&duckLisp);
	if (e) {
		printf("Error printing DuckLisp AST. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	
	e = duckLisp_loadString(&duckLisp, &scriptHandles[1], DL_STR(source1));
	if (e) {
		printf("Error loading string. (%s)\n", dl_errorString[e]);
		
		while (dl_true) {
			e = array_popElement(&duckLisp.errors, (void *) &error);
			if (e) {
				break;
			}
			
			for (dl_ptrdiff_t i = 0; i < duckLisp.source.elements_length; i++) {
				putchar(((char *) duckLisp.source.elements)[i]);
			}
			putchar('\n');
			
			for (dl_ptrdiff_t i = 0; i < error.message_length; i++) {
				putchar(error.message[i]);
			}
			putchar('\n');
			
			printf("%s\n", source1);
			for (dl_ptrdiff_t i = duckLisp.source.elements_length - sizeof(source1); i < error.index; i++) {
				putchar(' ');
			}
			puts("^");
		}
		
		goto l_cleanup;
	}
	
	e = dl_memory_checkHealth(duckLisp.memoryAllocation);
	if (e) {
		printf("Memory health check failed. (%s)\n", dl_errorString[e]);
	}
	
	e = duckLisp_cst_print(&duckLisp);
	if (e) {
		printf("Error printing DuckLisp CST. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	
	e = duckLisp_ast_print(&duckLisp);
	if (e) {
		printf("Error printing DuckLisp AST. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	
	e = duckLisp_loadString(&duckLisp, &scriptHandles[2], DL_STR(source2));
	if (e) {
		printf("Error loading string. (%s)\n", dl_errorString[e]);
		
		while (dl_true) {
			e = array_popElement(&duckLisp.errors, (void *) &error);
			if (e) {
				break;
			}
			
			for (dl_ptrdiff_t i = 0; i < duckLisp.source.elements_length; i++) {
				putchar(((char *) duckLisp.source.elements)[i]);
			}
			putchar('\n');
			
			for (dl_ptrdiff_t i = 0; i < error.message_length; i++) {
				putchar(error.message[i]);
			}
			putchar('\n');
			
			printf("%s\n", source2);
			for (dl_ptrdiff_t i = duckLisp.source.elements_length - sizeof(source2); i < error.index; i++) {
				putchar(' ');
			}
			puts("^");
		}
		
		goto l_cleanup;
	}
	
	e = dl_memory_checkHealth(duckLisp.memoryAllocation);
	if (e) {
		printf("Memory health check failed. (%s)\n", dl_errorString[e]);
	}
	
	e = duckLisp_cst_print(&duckLisp);
	if (e) {
		printf("Error printing DuckLisp CST. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	
	e = duckLisp_ast_print(&duckLisp);
	if (e) {
		printf("Error printing DuckLisp AST. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	
	// /**/ duckLisp_run(&duckLisp, &script_handle);
	
	l_cleanup:
	
	if (d.duckLisp_init) {
		duckLisp_quit(&duckLisp);
	}
	if (d.duckLispMemory) {
		/**/ free(duckLispMemory); duckLispMemory = NULL;
	}
	
	return e;
}
