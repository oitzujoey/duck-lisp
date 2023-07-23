/*
MIT License

Copyright (c) 2021, 2022, 2023 Joseph Herguth

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
	struct duckVM_object_s *objects;
	struct duckVM_object_s **freeObjects;
	dl_bool_t *objectInUse;
	dl_size_t objects_length;
	dl_size_t freeObjects_length;
	dl_array_strategy_t strategy;
	dl_memoryAllocation_t *memoryAllocation;
} duckVM_gclist_t;

typedef struct {
	dl_uint8_t *ip;
	struct duckVM_object_s *bytecode;
} duckVM_callFrame_t;

typedef struct duckVM_s {
	dl_memoryAllocation_t *memoryAllocation;
	dl_array_t errors;  /* Runtime errors. */
	dl_array_t stack;  /* dl_array_t:duckLisp_object_t For data. */
	dl_array_t call_stack;  /* duckVM_callFrame_t */
	/* I'm lazy and I don't want to bother with correct GC. */
	struct duckVM_object_s *currentBytecode;
	dl_array_t upvalue_stack;  /* duckVM_upvalue_t * */
	dl_array_t upvalue_array_call_stack;  /* duckVM_upvalueArray_t */
	/* Addressed by symbol number. */
	dl_array_t globals;  /* duckVM_object_t * */
	dl_array_t globals_map;  /* dl_ptrdiff_t */
	duckVM_gclist_t gclist;
	dl_size_t nextUserType;
	void *duckLisp;
} duckVM_t;


/* Should never appear on the stack */
typedef struct {
	dl_uint8_t *value;
	dl_size_t value_length;
} duckVM_internalString_t;

typedef struct {
	struct duckVM_object_s *internalString;
	dl_ptrdiff_t offset;
	dl_size_t length;
} duckVM_string_t;

typedef struct {
	dl_size_t id;
	struct duckVM_object_s *internalString;
} duckVM_symbol_t;

typedef struct {
	dl_error_t (*callback)(duckVM_t *);
} duckVM_function_t;

typedef struct {
	/* `name` might not be a good name. It is the index of the function. */
	dl_ptrdiff_t name;
	/* The *entire* bytecode the function is defined in. In most cases the function is a small part of the
	   code. */
	struct duckVM_object_s *bytecode;
	struct duckVM_object_s *upvalue_array;
	dl_uint8_t arity;
	dl_bool_t variadic;
} duckVM_closure_t;

typedef struct duckVM_object_s * duckVM_list_t;

/* Should never appear on the stack */
typedef struct {
	struct duckVM_object_s *car;
	struct duckVM_object_s *cdr;
} duckVM_cons_t;

/* Should never appear on the stack */
typedef struct {
	union {
		dl_ptrdiff_t stack_index;
		struct duckVM_object_s *heap_object;
		struct duckVM_object_s *heap_upvalue;
	} value;
	duckVM_upvalue_type_t type;
} duckVM_upvalue_t;

/* Should never appear on the stack */
typedef struct {
	struct duckVM_object_s **upvalues;
	dl_size_t length;
} duckVM_upvalueArray_t;

/* Should never appear on the stack */
typedef struct {
	struct duckVM_object_s **values;
	dl_size_t length;
	dl_bool_t initialized;
} duckVM_internalVector_t;

typedef struct {
	struct duckVM_object_s *internal_vector;
	dl_ptrdiff_t offset;
} duckVM_vector_t;

/* Should never appear on the stack */
typedef struct {
	dl_uint8_t *bytecode;
	dl_size_t bytecode_length;
} duckVM_bytecode_t;

/* Should never appear on the stack */
typedef struct {
	dl_size_t type;
	struct duckVM_object_s *value;
	struct duckVM_object_s *function;
} duckVM_internalComposite_t;

typedef struct duckVM_object_s * duckVM_composite_t;

typedef struct {
	void *data;
	dl_error_t (*destructor)(duckVM_gclist_t *, struct duckVM_object_s *);
} duckVM_user_t;


typedef enum {
  duckVM_object_type_none,

  /* These types are user visible types. */
  duckVM_object_type_bool,
  duckVM_object_type_integer,
  duckVM_object_type_float,
  duckVM_object_type_string,
  duckVM_object_type_list,
  duckVM_object_type_symbol,
  duckVM_object_type_function,
  duckVM_object_type_closure,
  duckVM_object_type_vector,
  duckVM_object_type_type,
  duckVM_object_type_composite,
  /* User-defined type */
  duckVM_object_type_user,

  /* These types should never appear on the stack. */
  duckVM_object_type_cons,
  duckVM_object_type_upvalue,
  duckVM_object_type_upvalueArray,
  duckVM_object_type_internalVector,
  duckVM_object_type_bytecode,
  duckVM_object_type_internalComposite,
  duckVM_object_type_internalString,

  /* This is... you guessed it... the last entry in the enum. */
  duckVM_object_type_last,
} duckVM_object_type_t;

typedef struct duckVM_object_s {
	union {
		dl_bool_t boolean;
		dl_ptrdiff_t integer;
		double floatingPoint;
		duckVM_internalString_t internalString;
		duckVM_string_t string;
		duckVM_symbol_t symbol;
		duckVM_function_t function;
		duckVM_closure_t closure;
		duckVM_list_t list;
		duckVM_cons_t cons;
		duckVM_upvalue_t upvalue;
		duckVM_upvalueArray_t upvalue_array;
		duckVM_internalVector_t internal_vector;
		duckVM_vector_t vector;
		duckVM_bytecode_t bytecode;
		dl_size_t type;
		duckVM_internalComposite_t internalComposite;
		duckVM_composite_t composite;
		duckVM_user_t user;
	} value;
	duckVM_object_type_t type;
	dl_bool_t inUse;
} duckVM_object_t;

typedef dl_error_t (*duckVM_gclist_destructor_t)(duckVM_gclist_t *, duckVM_object_t *);

typedef enum {
	duckVM_halt_mode_run,
	duckVM_halt_mode_yield,
	duckVM_halt_mode_halt,
} duckVM_halt_mode_t;



/* VM management */

/* Initialize the VM. */
dl_error_t duckVM_init(duckVM_t *duckVM, dl_memoryAllocation_t *memoryAllocation, dl_size_t maxObjects);
/* Destroy the VM. This will free up any external resources that the VM is currently using. */
void duckVM_quit(duckVM_t *duckVM);
/* Execute bytecode. */
dl_error_t duckVM_execute(duckVM_t *duckVM,
                          duckVM_object_t *return_value,
                          dl_uint8_t *bytecode,
                          dl_size_t bytecode_length);
/* Pass a C callback to the VM. `key` can be found by querying the compiler. */
dl_error_t duckVM_linkCFunction(duckVM_t *duckVM, dl_ptrdiff_t key, dl_error_t (*callback)(duckVM_t *));

/* Empty the stack. */
dl_error_t duckVM_popAll(duckVM_t *duckVM);
/* Force garbage collection to run. */
dl_error_t duckVM_garbageCollect(duckVM_t *duckVM);
/* Reset the VM, but retain global variables and the contents of the heap. */
dl_error_t duckVM_softReset(duckVM_t *duckVM);


/* Functions intended for C callbacks */

/* Log a string into the errors array. */
dl_error_t duckVM_error_pushRuntime(duckVM_t *duckVM, const char *message, const dl_size_t message_length);

/* Push an object onto the stack. */
dl_error_t duckVM_push(duckVM_t *duckVM, duckVM_object_t *object);
/* Push a boolean onto the stack. */
dl_error_t duckVM_pushBoolean(duckVM_t *duckVM, const dl_bool_t boolean);
/* Push an integer onto the stack. */
dl_error_t duckVM_pushInteger(duckVM_t *duckVM, const dl_ptrdiff_t integer);
/* Push a double floating point value onto the stack. */
dl_error_t duckVM_pushFloat(duckVM_t *duckVM, const double floatingPoint);
/* Push nil onto the stack. */
dl_error_t duckVM_pushNil(duckVM_t *duckVM);

/* Pop an object off of the stack and into `object`. */
dl_error_t duckVM_pop(duckVM_t *duckVM, duckVM_object_t *object);

/* Copy an object onto the heap. */
dl_error_t duckVM_allocateHeapObject(duckVM_t *duckVM, duckVM_object_t **heapObjectOut, duckVM_object_t objectIn);

/* Return the type of an object. */
duckVM_object_type_t duckVM_typeOf(duckVM_object_t object);

/* Pass an object to these functions to return the requested field. Use them if the save space. Or don't use them. Your
   choice. */
dl_bool_t duckVM_object_getBoolean(duckVM_object_t object);
dl_ptrdiff_t duckVM_object_getInteger(duckVM_object_t object);
double duckVM_object_getFloat(duckVM_object_t object);
duckVM_string_t duckVM_object_getString(duckVM_object_t object);
duckVM_internalString_t duckVM_object_getInternalString(duckVM_object_t object);
duckVM_list_t duckVM_object_getList(duckVM_object_t object);
duckVM_cons_t duckVM_object_getCons(duckVM_object_t object);
duckVM_symbol_t duckVM_object_getSymbol(duckVM_object_t object);
duckVM_function_t duckVM_object_getFunction(duckVM_object_t object);
duckVM_upvalue_t duckVM_object_getUpvalue(duckVM_object_t object);
duckVM_upvalueArray_t duckVM_object_getUpvalueArray(duckVM_object_t object);
duckVM_closure_t duckVM_object_getClosure(duckVM_object_t object);
duckVM_vector_t duckVM_object_getVector(duckVM_object_t object);
duckVM_internalVector_t duckVM_object_getInternalVector(duckVM_object_t object);
duckVM_bytecode_t duckVM_object_getBytecode(duckVM_object_t object);
dl_size_t duckVM_object_getType(duckVM_object_t object);
duckVM_composite_t duckVM_object_getComposite(duckVM_object_t object);
duckVM_internalComposite_t duckVM_object_getInternalComposite(duckVM_object_t object);
duckVM_user_t duckVM_object_getUser(duckVM_object_t object);

/* Create an object of the type specified by the function. Some of these are convenient shortcuts. Others aren't as
   much. */
duckVM_object_t duckVM_object_makeBoolean(dl_bool_t boolean);
duckVM_object_t duckVM_object_makeInteger(dl_ptrdiff_t integer);
duckVM_object_t duckVM_object_makeFloat(double floatingPoint);
duckVM_object_t duckVM_object_makeInternalString(dl_uint8_t *value, dl_size_t length);
duckVM_object_t duckVM_object_makeString(duckVM_object_t *internalString, dl_ptrdiff_t offset, dl_size_t length);
duckVM_object_t duckVM_object_makeSymbol(dl_size_t id, duckVM_object_t *internalString);
duckVM_object_t duckVM_object_makeFunction(dl_error_t (*callback)(duckVM_t *));
duckVM_object_t duckVM_object_makeClosure(dl_ptrdiff_t name,
                                          duckVM_object_t *bytecode,
                                          duckVM_object_t *upvalueArray,
                                          dl_uint8_t arity,
                                          dl_bool_t variadic);
duckVM_object_t duckVM_object_makeList(duckVM_object_t *cons);
duckVM_object_t duckVM_object_makeCons(duckVM_object_t *car, duckVM_object_t *cdr);
/* No `makeUpvalueObject` because C function calls can't handle unions well. */
duckVM_object_t duckVM_object_makeUpvalueArray(duckVM_object_t **upvalues, dl_size_t length);
duckVM_object_t duckVM_object_makeInternalVector(duckVM_object_t **values, dl_size_t length, dl_bool_t initialized);
duckVM_object_t duckVM_object_makeVector(duckVM_object_t *internalVector, dl_ptrdiff_t offset);
duckVM_object_t duckVM_object_makeBytecode(dl_uint8_t *bytecode, dl_size_t length);
duckVM_object_t duckVM_object_makeInternalComposite(dl_size_t compositeType,
                                                    duckVM_object_t *value,
                                                    duckVM_object_t *function);
duckVM_object_t duckVM_object_makeComposite(duckVM_object_t *internalComposite);
duckVM_object_t duckVM_object_makeUser(void *data,
                                       dl_error_t (*destructor)(duckVM_gclist_t *, struct duckVM_object_s *));

/* Data accessors. Some of these directly fetch internal objects from external objects, but in exchange return an error
   value. */

dl_error_t duckVM_string_getInternalString(duckVM_string_t string, duckVM_internalString_t *internalString);
dl_error_t duckVM_string_getElement(duckVM_string_t string, dl_uint8_t *byte, dl_ptrdiff_t index);

dl_error_t duckVM_symbol_getInternalString(duckVM_symbol_t symbol, duckVM_internalString_t *internalString);

dl_error_t duckVM_closure_getBytecode(duckVM_closure_t closure, duckVM_bytecode_t *bytecode);
dl_error_t duckVM_closure_getUpvalueArray(duckVM_closure_t closure, duckVM_upvalueArray_t *upvalueArray);
/* Will follow the chain of upvalues to the end to return the object. */
dl_error_t duckVM_closure_getUpvalue(duckVM_t *duckVM,
                                     duckVM_closure_t closure,
                                     duckVM_object_t *object,
                                     dl_ptrdiff_t index);

dl_error_t duckVM_list_getCons(duckVM_list_t list, duckVM_cons_t *cons);

dl_error_t duckVM_upvalueArray_getUpvalue(duckVM_t *duckVM,
                                          duckVM_upvalueArray_t upvalueArray,
                                          duckVM_object_t *object,
                                          dl_ptrdiff_t index);

dl_error_t duckVM_internalVector_getElement(duckVM_internalVector_t internalVector,
                                            duckVM_object_t **object,
                                            dl_ptrdiff_t index);

dl_error_t duckVM_vector_getInternalVector(duckVM_vector_t vector, duckVM_internalVector_t *internalVector);
dl_error_t duckVM_vector_getLength(duckVM_vector_t vector, dl_size_t *length);
dl_error_t duckVM_vector_getElement(duckVM_vector_t vector,
                                    duckVM_object_t **object,
                                    dl_ptrdiff_t index);

dl_error_t duckVM_bytecode_getElement(duckVM_bytecode_t bytecode, dl_uint8_t *byte, dl_ptrdiff_t index);

dl_error_t duckVM_composite_getInternalComposite(duckVM_composite_t composite,
                                                 duckVM_internalComposite_t *internalComposite);
dl_error_t duckVM_composite_getType(duckVM_composite_t composite, dl_size_t *type);
dl_error_t duckVM_composite_getValueObject(duckVM_composite_t composite, duckVM_object_t **value);
dl_error_t duckVM_composite_getFunctionObject(duckVM_composite_t composite, duckVM_object_t **function);

#endif /* DUCKVM_H */
