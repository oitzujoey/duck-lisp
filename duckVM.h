
#ifndef DUCKVM_H
#define DUCKVM_H

#include "DuckLib/core.h"
#include "duckLisp.h"

typedef struct {
	dl_memoryAllocation_t memoryAllocation;
	dl_array_t errors;                          // Runtime errors.
	dl_array_t stack;                           // It's a stack.
	dl_ptrdiff_t frame_pointer;    // Points to the current stack frame.
	dl_array_t statics;                         // Stack for static variables. These never get deallocated.
} duckVM_t;

typedef enum {
	duckLisp_object_type_none,
	duckLisp_object_type_bool,
	duckLisp_object_type_integer,
	duckLisp_object_type_float,
	duckLisp_object_type_string,
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
		struct{
			// duckLisp_ast_compoundExpression_t tree;
			unsigned char *bytecode;
			dl_error_t (*callback)(duckVM_t *);
		} function;
	} value;
	duckLisp_object_type_t type;
} duckLisp_object_t;

dl_error_t duckVM_init(duckVM_t *duckVM, void *memory, dl_size_t size);
void duckVM_quit(duckVM_t *duckVM);
dl_error_t duckVM_execute(duckVM_t *duckVM, unsigned char *bytecode);
dl_error_t duckVM_callLocal(duckVM_t *duckVM, dl_ptrdiff_t function_index);
// void duckVM_loadBytecode(duckVM_t *duckVM, unsigned char *bytecode, dl_size_t bytecode_length);
dl_error_t duckVM_linkCFunction(duckVM_t *duckVM, dl_ptrdiff_t callback_index, dl_error_t (*callback)(duckVM_t *));

/* Functions for C callbacks */
void duckVM_getArgLength(duckVM_t *duckVM, dl_size_t *length);
dl_error_t duckVM_getArg(duckVM_t *duckVM, duckLisp_object_t *object, dl_ptrdiff_t index);
dl_error_t duckVM_pop(duckVM_t *duckVM, duckLisp_object_t *object);
dl_error_t duckVM_push(duckVM_t *duckVM, duckLisp_object_t *object);
dl_error_t duckVM_pushReturn(duckVM_t *duckVM, duckLisp_object_t object);

#endif /* DUCKVM_H */
