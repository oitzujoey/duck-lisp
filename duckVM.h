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

#ifndef DUCKVM_H
#define DUCKVM_H

#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/array.h"

typedef enum {
	duckVM_upvalue_type_stack_index,
	duckVM_upvalue_type_heap_object,
	duckVM_upvalue_type_heap_upvalue
} duckVM_upvalue_type_t;

typedef struct duckVM_gclist_s {
	struct duckLisp_object_s *objects;
	struct duckLisp_object_s **freeObjects;
	dl_bool_t *objectInUse;
	dl_size_t objects_length;
	dl_size_t freeObjects_length;
	dl_array_strategy_t strategy;
	dl_memoryAllocation_t *memoryAllocation;
} duckVM_gclist_t;

typedef struct {
	dl_uint8_t *ip;
	struct duckLisp_object_s *bytecode;
} duckVM_callFrame_t;

typedef struct duckVM_s {
	dl_memoryAllocation_t *memoryAllocation;
	dl_array_t errors;  /* Runtime errors. */
	dl_array_t stack;  /* dl_array_t:duckLisp_object_t For data. */
	dl_array_t call_stack;  /* duckVM_callFrame_t */
	/* I'm lazy and I don't want to bother with correct GC. */
	struct duckLisp_object_s *currentBytecode;
	dl_array_t upvalue_stack;  /* duckVM_upvalue_t * */
	dl_array_t upvalue_array_call_stack;  /* duckVM_upvalue_t ** */
	dl_array_t upvalue_array_length_call_stack;  /* dl_size_t */
	/* Addressed by symbol number. */
	dl_array_t globals;  /* duckVM_object_t * */
	dl_array_t globals_map;  /* dl_ptrdiff_t */
	duckVM_gclist_t gclist;
	dl_size_t nextUserType;
	void *duckLisp;
} duckVM_t;

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
  duckLisp_object_type_type,
  duckLisp_object_type_composite,
  /* User-defined type */
  duckLisp_object_type_user,

  /* These types should never appear on the stack. */
  duckLisp_object_type_cons,
  duckLisp_object_type_upvalue,
  duckLisp_object_type_upvalueArray,
  duckLisp_object_type_internalVector,
  duckLisp_object_type_bytecode,
  duckLisp_object_type_internalComposite,
  duckLisp_object_type_internalString,

  /* This is... you guessed it... the last entry in the enum. */
  duckLisp_object_type_last,
} duckLisp_object_type_t;

typedef struct duckLisp_object_s {
	union {
		dl_bool_t boolean;
		dl_ptrdiff_t integer;
		double floatingPoint;
		struct {
			dl_uint8_t *value;
			dl_size_t value_length;
		} internalString;
		struct {
			struct duckLisp_object_s *internalString;
			dl_ptrdiff_t offset;
			dl_size_t length;
		} string;
		struct {
			dl_size_t id;
			struct duckLisp_object_s *internalString;
		} symbol;
		struct {
			// duckLisp_ast_compoundExpression_t tree;
			unsigned char *bytecode;
			dl_error_t (*callback)(duckVM_t *);
		} function;
		struct {
			/* `name` might not be a good name. It is the index of the function. */
			dl_ptrdiff_t name;
			/* The *entire* bytecode the function is defined in. In most cases the function is a small part of the
			   code. */
			struct duckLisp_object_s *bytecode;
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
		struct {
			dl_uint8_t *bytecode;
			dl_size_t bytecode_length;
		} bytecode;
		dl_size_t type;
		struct {
			dl_size_t type;
			struct duckLisp_object_s *value;
			struct duckLisp_object_s *function;
		} internalComposite;
		struct duckLisp_object_s *composite;
		struct {
			void *data;
			dl_error_t (*destructor)(duckVM_gclist_t *, struct duckLisp_object_s *);
		} user;
	} value;
	duckLisp_object_type_t type;
	dl_bool_t inUse;
} duckLisp_object_t;

typedef dl_error_t (*duckVM_gclist_destructor_t)(duckVM_gclist_t *, duckLisp_object_t *);

typedef enum {
	duckVM_halt_mode_run,
	duckVM_halt_mode_yield,
	duckVM_halt_mode_halt,
} duckVM_halt_mode_t;

dl_error_t duckVM_init(duckVM_t *duckVM, dl_memoryAllocation_t *memoryAllocation, dl_size_t maxObjects);
void duckVM_quit(duckVM_t *duckVM);
dl_error_t duckVM_execute(duckVM_t *duckVM,
                          duckLisp_object_t *return_value,
                          dl_uint8_t *bytecode,
                          dl_size_t bytecode_length);
dl_error_t duckVM_funcall(duckVM_t *duckVM,
                          duckLisp_object_t *return_value,
                          dl_uint8_t *bytecode,
                          duckLisp_object_t *closure);
dl_error_t duckVM_callLocal(duckVM_t *duckVM, duckLisp_object_t *return_value, dl_ptrdiff_t function_index);
dl_error_t duckVM_linkCFunction(duckVM_t *duckVM, dl_ptrdiff_t key, dl_error_t (*callback)(duckVM_t *));

/* Functions for C callbacks */
dl_error_t duckVM_error_pushRuntime(duckVM_t *duckVM, const char *message, const dl_size_t message_length);
dl_error_t duckVM_gclist_pushObject(duckVM_t *duckVM, duckLisp_object_t **objectOut, duckLisp_object_t objectIn);
dl_error_t duckVM_garbageCollect(duckVM_t *duckVM);
/* void duckVM_getArgLength(duckVM_t *duckVM, dl_size_t *length); */
/* dl_error_t duckVM_getArg(duckVM_t *duckVM, duckLisp_object_t *object, dl_ptrdiff_t index); */
dl_error_t duckVM_pop(duckVM_t *duckVM, duckLisp_object_t *object);
dl_error_t duckVM_popAll(duckVM_t *duckVM);
dl_error_t duckVM_push(duckVM_t *duckVM, duckLisp_object_t *object);
dl_error_t duckVM_pushNil(duckVM_t *duckVM);
dl_error_t duckVM_softReset(duckVM_t *duckVM);
/* dl_error_t duckVM_pushReturn(duckVM_t *duckVM, duckLisp_object_t object); */
dl_error_t duckVM_makeGlobal(duckVM_t *duckVM, const dl_ptrdiff_t key, duckLisp_object_t *object);

#endif /* DUCKVM_H */
