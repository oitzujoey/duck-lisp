
#include <stdlib.h>
#include "../DuckLib/core.h"
#include "../duckLisp.h"

int main(int argc, char *argv[]) {
	dl_error_t error = dl_error_ok;
	
	duckLisp_t duckLisp;
	void *tempMemory = dl_null;
	size_t tempMemory_size;
	
	tempMemory_size = 1024;
	tempMemory = malloc(tempMemory_size);
	if (tempMemory == NULL) {
		error = dl_error_outOfMemory;
		goto l_cleanup;
	}
	
	error = duckLisp_init(&duckLisp, tempMemory, tempMemory_size);
	
	l_cleanup:
	
	return error;
}
