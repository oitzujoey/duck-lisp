
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>
#include "../DuckLib/core.h"
#include "../DuckLib/memory.h"
#include "../DuckLib/trie.h"
#include "../DuckLib/string.h"

int main(int argc, char *argv[]) {
	dl_error_t e = dl_error_ok;
	struct {
		dl_bool_t malloc;
	} d = {0};
	(void) argc;
	(void) argv;
	
	dl_memoryAllocation_t memoryAllocation;
	dl_trie_t trie;
	dl_ptrdiff_t index = 0;
	dl_bool_t equal = dl_false;
	
	const struct {
		char *string;
		dl_size_t length;
	} words[] = {
		{DL_STR("burst")},
		{DL_STR("prey")},
		{DL_STR("package")},
		{DL_STR("size")},
		{DL_STR("shoulder")},
		{DL_STR("toll")},
		{DL_STR("we")},
		{DL_STR("toss")},
		{DL_STR("smell")},
		{DL_STR("give")},
		{DL_STR("demonstration")},
		{DL_STR("calendar")},
		{DL_STR("tool")},
		{DL_STR("to")},
		{DL_STR("examination")},
		{DL_STR("mother")},
		{DL_STR("revolution")},
		{DL_STR("memory")},
		{DL_STR("commerce")},
		{DL_STR("course")},
		{DL_STR("admit")},
		{DL_STR("willpower")},
		{DL_STR("class")},
		{DL_STR("vegetarian")},
		{DL_STR("advance")},
		{DL_STR("personality")},
		{DL_STR("bat")},
		{DL_STR("folk")},
		{DL_STR("back")},
		{DL_STR("moment")},
		{DL_STR("pain")},
		{DL_STR("species")},
		{DL_STR("attachment")},
		{DL_STR("ant")},
		{DL_STR("pit")},
		{DL_STR("disappoint")},
		{DL_STR("pierce")},
		{DL_STR("screen")},
		{DL_STR("me")},
		{DL_STR("volcano")},
		{DL_STR("arrange")},
		{DL_STR("fuss")},
		{DL_STR("tape")},
		{DL_STR("novel")},
		{DL_STR("response")},
		{DL_STR("rainbow")},
		{DL_STR("hair")},
		{DL_STR("battlefield")},
		{DL_STR("")},
		{DL_STR("flat")},
		{NULL, 0}
	};
	
	const size_t memory_size = 1024*1024;
	void *memory = malloc(memory_size);
	if (memory == NULL) {
		puts("Out of memory.");
		goto l_cleanup;
	}
	d.malloc = dl_true;
	
	e = dl_memory_init(&memoryAllocation, (void *) memory, memory_size, dl_memoryFit_best);
	if (e) {
		fprintf(stderr, "Could not initialize memory. (%s)\n", dl_errorString[e]);
		goto l_cleanup;
	}
	
	/**/ dl_trie_init(&trie, &memoryAllocation, -1);
	
	// Populate tree from table.
	for (dl_ptrdiff_t i = 0; words[i].string != NULL; i++) {
		dl_string_compare_partial(&equal, words[i].string, DL_STR("moment"));
		e = dl_trie_insert(&trie, words[i].string, words[i].length, index++);
		if (e) {
			fprintf(stderr, "Could not insert keyword and index into trie. [%lli] (%s)\n", index-1, dl_errorString[e]);
			goto l_cleanup;
		}
	}
	
	// /**/ dl_trie_print(trie);
	/**/ dl_trie_print_compact(trie);
	
	dl_trie_find(trie, &index, DL_STR("hair"));
	printf("index %lli\n", index);
	dl_trie_find(trie, &index, DL_STR(""));
	printf("index %lli\n", index);
	dl_trie_find(trie, &index, DL_STR("me"));
	printf("index %lli\n", index);
	dl_trie_find(trie, &index, DL_STR("mother"));
	printf("index %lli\n", index);
	dl_trie_find(trie, &index, DL_STR("memory"));
	printf("index %lli\n", index);
	dl_trie_find(trie, &index, DL_STR("moment"));
	printf("index %lli\n", index);
	dl_trie_find(trie, &index, DL_STR("f"));
	printf("index %lli\n", index);
	
	l_cleanup:
	
	/**/ dl_memory_quit(&memoryAllocation);
	
	if (d.malloc) {
		free(memory); memory = 0;
	}
	
	return e;
}
