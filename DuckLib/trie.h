
#ifndef DUCKLIB_TRIE_H
#define DUCKLIB_TRIE_H

#include "core.h"
#include "memory.h"
#include "array.h"

typedef struct dl_trie_node_s {
	struct {
		dl_uint8_t **nodes_name;
		dl_size_t *nodes_name_lengths;
		struct dl_trie_node_s *nodes;
		dl_size_t nodes_length;
	} value;
	dl_ptrdiff_t index;
} dl_trie_node_t;

typedef struct {
	dl_memoryAllocation_t *memoryAllocation;
	dl_trie_node_t trie;
} dl_trie_t;

void DECLSPEC dl_trie_init(dl_trie_t *trie, dl_memoryAllocation_t *memoryAllocation, dl_ptrdiff_t nullIndex);
dl_error_t DECLSPEC dl_trie_quit(dl_trie_t *trie);

dl_error_t dl_trie_prettyPrint(dl_array_t *string_array, dl_trie_t trie);
dl_error_t dl_trie_node_prettyPrint(dl_array_t *string_array, dl_trie_node_t trie_node);

dl_error_t DECLSPEC dl_trie_insert(dl_trie_t *trie,
                                   const dl_uint8_t *key,
                                   const dl_size_t key_length,
                                   const dl_ptrdiff_t index);
void DECLSPEC dl_trie_find(dl_trie_t trie, dl_ptrdiff_t *index, const dl_uint8_t *key, const dl_size_t key_length);

#endif /* DUCKLIB_TRIE_H */
