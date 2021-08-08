
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
	dl_ptrdiff_t script_handle = -1;
	duckLisp_error_t error;
	const char source[] = "((string s \"Hello, world!\") (print s))";
	
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
	
	e = duckLisp_loadString(&duckLisp, &script_handle, DL_STR(source));
	if (e) {
		printf("Error loading string. (%s)\n", dl_errorString[e]);
		
		while (dl_true) {
			e = array_popElement(&duckLisp.errors, (void *) &error);
			if (e) {
				break;
			}
			
			for (dl_ptrdiff_t i = 0; i < error.message_length; i++) {
				putchar(error.message[i]);
			}
			putchar('\n');
			
			printf("%s\n", source);
			for (dl_ptrdiff_t i = 0; i < error.index; i++) {
				putchar(' ');
			}
			puts("^");
		}
		
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
