#ifndef DUCKVM_H
#define DUCKVM_H

#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/array.h"

typedef struct duckVM_gclist_s {
	struct duckLisp_object_s *objects;
	struct duckLisp_object_s **freeObjects;
	dl_bool_t *objectInUse;
	dl_size_t objects_length;
	dl_size_t freeObjects_length;
	dl_array_strategy_t strategy;
	dl_memoryAllocation_t *memoryAllocation;
} duckVM_gclist_t;

typedef enum {
	duckVM_upvalue_type_stack_index,
	duckVM_upvalue_type_heap_object,
	duckVM_upvalue_type_heap_upvalue
} duckVM_upvalue_type_t;

typedef struct {
	dl_memoryAllocation_t *memoryAllocation;
	dl_array_t errors;  // Runtime errors.
	dl_array_t stack;  // duckLisp_object_t For data.
	dl_array_t call_stack;  // unsigned char *
	dl_array_t upvalue_stack;  // duckVM_upvalue_t *
	dl_array_t upvalue_array_call_stack;  // duckVM_upvalue_t **
	dl_array_t upvalue_array_length_call_stack;  // dl_size_t
	dl_array_t statics;  // Stack for static variables. These never get deallocated.
	duckVM_gclist_t gclist;
} duckVM_t;

/* When adding types, always add to the end of the section. */
typedef enum {
  duckLisp_object_type_none,

  /* These types are user visible types. */
  duckLisp_object_type_bool,
  duckLisp_object_type_integer,
  duckLisp_object_type_float,
  duckLisp_object_type_string,
  duckLisp_object_type_list,
  duckLisp_object_type_symbol,
  duckLisp_object_type_function,
  duckLisp_object_type_closure,
  duckLisp_object_type_vector,

  /* These types should never appear on the stack. */
  duckLisp_object_type_cons,
  duckLisp_object_type_upvalue,
  duckLisp_object_type_upvalueArray,
  duckLisp_object_type_internalVector,
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
		struct {
			// duckLisp_ast_compoundExpression_t tree;
			unsigned char *bytecode;
			dl_error_t (*callback)(duckVM_t *);
		} function;
		struct {
			dl_ptrdiff_t name;
			struct duckLisp_object_s *upvalue_array;
			dl_uint8_t arity;
			dl_bool_t variadic;
		} closure;
		struct duckLisp_object_s *list;
		struct {
			struct duckLisp_object_s *car;
			struct duckLisp_object_s *cdr;
		} cons;
		struct {
			duckVM_upvalue_type_t type;
			union {
				dl_ptrdiff_t stack_index;
				struct duckLisp_object_s *heap_object;
				struct duckLisp_object_s *heap_upvalue;
			} value;
		} upvalue;
		struct {
			struct duckLisp_object_s **upvalues;
			dl_size_t length;
			dl_bool_t initialized;
		} upvalue_array;
		struct {
			struct duckLisp_object_s **values;
			dl_size_t length;
			dl_bool_t initialized;
		} internal_vector;
		struct {
			struct duckLisp_object_s *internal_vector;
			dl_ptrdiff_t offset;
		} vector;
	} value;
	duckLisp_object_type_t type;
	dl_bool_t inUse;
} duckLisp_object_t;

dl_error_t duckVM_init(duckVM_t *duckVM, dl_size_t maxObjects);
void duckVM_quit(duckVM_t *duckVM);
dl_error_t duckVM_execute(duckVM_t *duckVM, duckLisp_object_t *return_value, dl_uint8_t *bytecode);
dl_error_t duckVM_callLocal(duckVM_t *duckVM, duckLisp_object_t *return_value, dl_ptrdiff_t function_index);
dl_error_t duckVM_linkCFunction(duckVM_t *duckVM, dl_ptrdiff_t callback_index, dl_error_t (*callback)(duckVM_t *));

/* Functions for C callbacks */
dl_error_t duckVM_gclist_pushObject(duckVM_t *duckVM, duckLisp_object_t **objectOut, duckLisp_object_t objectIn);
dl_error_t duckVM_garbageCollect(duckVM_t *duckVM);
/* void duckVM_getArgLength(duckVM_t *duckVM, dl_size_t *length); */
/* dl_error_t duckVM_getArg(duckVM_t *duckVM, duckLisp_object_t *object, dl_ptrdiff_t index); */
dl_error_t duckVM_pop(duckVM_t *duckVM, duckLisp_object_t *object);
dl_error_t duckVM_push(duckVM_t *duckVM, duckLisp_object_t *object);
dl_error_t duckVM_pushNil(duckVM_t *duckVM);
/* dl_error_t duckVM_pushReturn(duckVM_t *duckVM, duckLisp_object_t object); */

#endif /* DUCKVM_H */
