
#ifndef DUCKVM_H
#define DUCKVM_H

#include "DuckLib/core.h"
#include "duckLisp.h"

typedef struct duckVM_gclist_s {
	struct duckVM_gclist_cons_s *conses;
	struct duckVM_gclist_cons_s **freeConses;
	dl_bool_t *inUse;
	dl_size_t conses_length;
	dl_size_t freeConses_length;
	dl_array_strategy_t strategy;
	dl_memoryAllocation_t *memoryAllocation;
} duckVM_gclist_t;

typedef struct {
	dl_memoryAllocation_t memoryAllocation;
	dl_array_t errors;                          // Runtime errors.
	dl_array_t stack;                           // For data.
	dl_array_t call_stack;
	dl_array_t statics;                         // Stack for static variables. These never get deallocated.
	duckVM_gclist_t gclist;
} duckVM_t;

typedef enum {
	duckLisp_object_type_none,
	duckLisp_object_type_bool,
	duckLisp_object_type_integer,
	duckLisp_object_type_float,
	duckLisp_object_type_string,
	duckLisp_object_type_list,
	duckLisp_object_type_symbol,
	duckLisp_object_type_function
} duckLisp_object_type_t;

typedef struct duckLisp_object_s {
	union {
		dl_bool_t boolean;
		dl_ptrdiff_t integer;
		double floatingPoint;
		struct {
			char *value;
			dl_size_t value_length;
		} string;
		struct {
			dl_size_t id;
			char *value;
			dl_size_t value_length;
		} symbol;
		struct{
			// duckLisp_ast_compoundExpression_t tree;
			unsigned char *bytecode;
			dl_error_t (*callback)(duckVM_t *);
		} function;
		struct duckVM_gclist_cons_s *list;
	} value;
	duckLisp_object_type_t type;
} duckLisp_object_t;

typedef enum {
	duckVM_gclist_cons_type_addrAddr,
	duckVM_gclist_cons_type_addrObject,
	duckVM_gclist_cons_type_objectAddr,
	duckVM_gclist_cons_type_objectObject,
} duckVM_gclist_cons_type_t;

typedef struct duckVM_gclist_cons_s {
	union {
		struct duckVM_gclist_cons_s *addr;
		duckLisp_object_t *data;
	} car;
	union {
		struct duckVM_gclist_cons_s *addr;
		duckLisp_object_t *data;
	} cdr;
	duckVM_gclist_cons_type_t type;
} duckVM_gclist_cons_t;


dl_error_t duckVM_init(duckVM_t *duckVM, void *memory, dl_size_t size, dl_size_t maxConses);
void duckVM_quit(duckVM_t *duckVM);
dl_error_t duckVM_execute(duckVM_t *duckVM, unsigned char *bytecode);
dl_error_t duckVM_callLocal(duckVM_t *duckVM, dl_ptrdiff_t function_index);
// void duckVM_loadBytecode(duckVM_t *duckVM, unsigned char *bytecode, dl_size_t bytecode_length);
dl_error_t duckVM_linkCFunction(duckVM_t *duckVM, dl_ptrdiff_t callback_index, dl_error_t (*callback)(duckVM_t *));

/* Functions for C callbacks */
/* void duckVM_getArgLength(duckVM_t *duckVM, dl_size_t *length); */
/* dl_error_t duckVM_getArg(duckVM_t *duckVM, duckLisp_object_t *object, dl_ptrdiff_t index); */
dl_error_t duckVM_pop(duckVM_t *duckVM, duckLisp_object_t *object);
dl_error_t duckVM_push(duckVM_t *duckVM, duckLisp_object_t *object);
/* dl_error_t duckVM_pushReturn(duckVM_t *duckVM, duckLisp_object_t object); */

#endif /* DUCKVM_H */
