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
	struct duckVM_s *duckVM;
} duckVM_gclist_t;

typedef struct {
	dl_uint8_t *ip;
	struct duckVM_object_s *bytecode;
} duckVM_callFrame_t;

typedef struct duckVM_s {
	dl_memoryAllocation_t *memoryAllocation;
	dl_array_t errors;  /* Runtime errors. */
	dl_array_t stack;  /* dl_array_t:duckVM_object_t For data. */
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
#ifdef USE_PARENTHESIS_INFERENCE
	void *inferrerContext;
#endif /* USE_PARENTHESIS_INFERENCE */
	void *userData;
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
	dl_error_t (*marker)(duckVM_gclist_t *, dl_array_t *, struct duckVM_object_s *);
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
	duckVM_halt_mode_halt,
} duckVM_halt_mode_t;



/* VM management */

/* Initialize the VM. */
dl_error_t duckVM_init(duckVM_t *duckVM, dl_memoryAllocation_t *memoryAllocation, dl_size_t maxObjects);
/* Destroy the VM. This will free up any external resources that the VM is currently using. */
void duckVM_quit(duckVM_t *duckVM);
/* Execute bytecode. */
dl_error_t duckVM_execute(duckVM_t *duckVM, dl_uint8_t *bytecode, dl_size_t bytecode_length);
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
dl_error_t duckVM_error_pushRuntime(duckVM_t *duckVM, const dl_uint8_t *message, const dl_size_t message_length);

/* General operations */
/* Get the length of the stack. */
dl_size_t duckVM_stackLength(duckVM_t *duckVM);
/* Push an existing stack object onto the top of the stack. */
dl_error_t duckVM_push(duckVM_t *duckVM, dl_ptrdiff_t stack_index);
/* Pop an object off of the stack. */
dl_error_t duckVM_pop(duckVM_t *duckVM);
/* Pop the specified number of objects off of the stack. */
dl_error_t duckVM_popSeveral(duckVM_t *duckVM, dl_size_t number_to_pop);
/* Copy a stack object from the top of the stack to another position, overwriting the destination object. */
dl_error_t duckVM_copyFromTop(duckVM_t *duckVM, dl_ptrdiff_t destination_stack_index);
/* Return the type of the object on the top of the stack. */
dl_error_t duckVM_typeOf(duckVM_t *duckVM, duckVM_object_type_t *type);
/* Call the object at the given index as a function. */
dl_error_t duckVM_call(duckVM_t *duckVM, dl_ptrdiff_t stackIndex, dl_uint8_t numberOfArgs);

/* Global variables */

/* Push the specified global variable onto the stack. */
dl_error_t duckVM_pushGlobal(duckVM_t *duckVM, const dl_ptrdiff_t key);
/* Set the specified global variable to the value of the object on top of the stack. */
dl_error_t duckVM_setGlobal(duckVM_t *duckVM, const dl_ptrdiff_t key);

/* Type-specific operations */

/* Booleans */
/* Push a boolean initialized to `false` onto the top of the stack. */
dl_error_t duckVM_pushBoolean(duckVM_t *duckVM);
/* Set the boolean at the top of the stack to the provided value. */
dl_error_t duckVM_setBoolean(duckVM_t *duckVM, dl_bool_t value);
/* Copy a boolean off the top of the stack into the provided variable. */
dl_error_t duckVM_copyBoolean(duckVM_t *duckVM, dl_bool_t *value);

/* Integers */
/* Push an integer initialized to `0` onto the top of the stack. */
dl_error_t duckVM_pushInteger(duckVM_t *duckVM);
/* Set the integer at the top of the stack to the provided value. */
dl_error_t duckVM_setInteger(duckVM_t *duckVM, dl_ptrdiff_t value);
/* Copy an integer off the top of the stack into the provided variable. */
dl_error_t duckVM_copySignedInteger(duckVM_t *duckVM, dl_ptrdiff_t *value);
dl_error_t duckVM_copyUnsignedInteger(duckVM_t *duckVM, dl_size_t *value);

/* Floats */
/* Push a float initialized to `0.0` onto the top of the stack. */
dl_error_t duckVM_pushFloat(duckVM_t *duckVM);
/* Set the float at the top of the stack to the provided value. */
dl_error_t duckVM_setFloat(duckVM_t *duckVM, double value);
/* Copy a float off the top of the stack into the provided variable. */
dl_error_t duckVM_copyFloat(duckVM_t *duckVM, double *value);

/* Strings */
/* Push a string onto the top of the stack. Strings are immutable, which is why there isn't a `duckVM_setString`. */
dl_error_t duckVM_pushString(duckVM_t *duckVM, dl_uint8_t *string, dl_size_t string_length);
/* Copy a string off the top of the stack into the provided variable. The user must free the string using the VM's
   allocator. */
dl_error_t duckVM_copyString(duckVM_t *duckVM, dl_uint8_t **string, dl_size_t *string_length);

/* Symbols */
/* Push a symbol onto the top of the stack. */
dl_error_t duckVM_pushSymbol(duckVM_t *duckVM, dl_size_t id, dl_uint8_t *name, dl_size_t name_length);
/* Push a symbol with an ID but no name onto the top of the stack. */
dl_error_t duckVM_pushCompressedSymbol(duckVM_t *duckVM, dl_size_t id);
/* Push the name string of the symbol on the top of the stack onto the top of the stack. */
dl_error_t duckVM_copySymbolName(duckVM_t *duckVM, dl_uint8_t **name, dl_size_t *name_length);
/* Push the ID of the symbol on the top of the stack onto the top of the stack. */
dl_error_t duckVM_copySymbolId(duckVM_t *duckVM, dl_size_t *id);

/* Types */
/* Create a new unique type and push it onto the top of the stack. */
dl_error_t duckVM_pushNewType(duckVM_t *duckVM);
/* Push the specified type onto the top of the stack. */
dl_error_t duckVM_pushExistingType(duckVM_t *duckVM, dl_size_t type);
/* Copy a type off the top of the stack into the provided variable. */
dl_error_t duckVM_copyType(duckVM_t *duckVM, dl_size_t *type);

/* Composites */
/* Push a composite value with the specified type onto the top of the stack. The value and function slots are set to
   nil. */
dl_error_t duckVM_pushComposite(duckVM_t *duckVM, dl_size_t type);
/* Push the type slot of the composite on the top of the stack onto the top of the stack. */
dl_error_t duckVM_copyCompositeType(duckVM_t *duckVM, dl_size_t *type);
/* Push the function slot of the composite on the top of the stack onto the top of the stack. */
dl_error_t duckVM_pushCompositeValue(duckVM_t *duckVM);
/* Push the function slot of the composite on the top of the stack onto the top of the stack. */
dl_error_t duckVM_pushCompositeFunction(duckVM_t *duckVM);
/* Set the value slot of the composite on the top of the stack to the value at the specified stack index. */
dl_error_t duckVM_setCompositeValue(duckVM_t *duckVM, dl_ptrdiff_t stack_index);
/* Set the function slot of the composite on the top of the stack to the value at the specified stack index. */
dl_error_t duckVM_setCompositeFunction(duckVM_t *duckVM, dl_ptrdiff_t stack_index);

/* Lists -- See sequence operations below that operate on these objects. */
/* Push nil onto the stack. */
dl_error_t duckVM_pushNil(duckVM_t *duckVM);
/* Push a cons onto the stack with both CAR and CDR set to nil. */
dl_error_t duckVM_pushCons(duckVM_t *duckVM);

/* Vectors -- See sequence operations below that operate on these objects. */
/* Push a vector with the specified length onto the stack with each element to nil. */
dl_error_t duckVM_pushVector(duckVM_t *duckVM, dl_size_t length);

/* Closures -- See sequence operations below that operate on these objects. */
/* Copy the "name" of the closure into the provided variable. Note that different closures may share the same "name". */
dl_error_t duckVM_copyClosureName(duckVM_t *duckVM, dl_ptrdiff_t *name);
/* Push the closure's bytecode object on the top of the stack. */
dl_error_t duckVM_pushClosureBytecode(duckVM_t *duckVM);
/* Copy the arity of the closure into the provided variable. */
dl_error_t duckVM_copyClosureArity(duckVM_t *duckVM, dl_uint8_t *arity);
/* Return whether or not the closure is variadic in the provided variable. */
dl_error_t duckVM_copyClosureIsVariadic(duckVM_t *duckVM, dl_bool_t *is_variadic);

/* Sequences */
/* These are operations for lists, vectors, strings, and closures. Not all sequence types support all operations. */
/* Push the first element of the sequence on the top of the stack onto the top of the stack. */
dl_error_t duckVM_pushCar(duckVM_t *duckVM);
dl_error_t duckVM_pushFirst(duckVM_t *duckVM);
/* Get all elements except the first from the sequence on top of the stack and push them on top of the stack.
   Lists:
     Nil: Push nil on top of the stack.
     Cons: Push the CDR on top of the stack.
   Vectors:
     Push a new vector on top of the stack that contains all elements of the old vector except the first. These
     like with the CDR of a list, these remaining elements are references to the elements of the original list.
   Strings: Push a new string on top of the stack that contains all bytes of the old string except the first.
   This operation fails on closures. */
dl_error_t duckVM_pushCdr(duckVM_t *duckVM);
dl_error_t duckVM_pushRest(duckVM_t *duckVM);
/* Set the first element of the sequence at the specified stack index to the value of the object at the top of the
   stack.
   This operation fails on nil, strings, and closures. */
dl_error_t duckVM_setCar(duckVM_t *duckVM, dl_ptrdiff_t stack_index);
dl_error_t duckVM_setFirst(duckVM_t *duckVM, dl_ptrdiff_t stack_index);
/* Set the CDR of the list at the specified stack index to the value of the object at the top of the stack.
   Only conses support this operation. */
dl_error_t duckVM_setCdr(duckVM_t *duckVM, dl_ptrdiff_t stack_index);
dl_error_t duckVM_setRest(duckVM_t *duckVM, dl_ptrdiff_t stack_index);
/* Get the specified element of the sequence on top of the stack, and push it on top of the stack.
   Lists:Push the indexed element on top of the stack.
   Vectors: Push the indexed element on top of the stack.
   Strings: Push the indexed byte on top of the stack as an integer.
   This operation fails on sequences are shorter than the index. */
dl_error_t duckVM_pushElement(duckVM_t *duckVM, dl_ptrdiff_t sequence_index);
/* Set the specified element of the sequence on top of the stack to the value at the specified stack index.
   Lists:
     Nil: Push nil on top of the stack.
     Cons: Push the CAR on top of the stack if this is the cons referenced by the index.
   Vectors: Push the indexed element on top of the stack.
   Strings: Push the indexed byte on top of the stack as an integer.
   This operation fails on lists and vectors that are shorter than the sequence index, and strings and closures. */
dl_error_t duckVM_setElement(duckVM_t *duckVM, dl_ptrdiff_t sequence_index, dl_ptrdiff_t stack_index);
/* Return the length of the sequence into the passed variable. */
dl_error_t duckVM_length(duckVM_t *duckVM, dl_size_t *length);

dl_error_t duckVM_isNone(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isBoolean(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isInteger(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isFloat(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isString(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isSymbol(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isType(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isComposite(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isList(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isNil(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isCons(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isVector(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isClosure(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isUser(duckVM_t *duckVM, dl_bool_t *result);
dl_error_t duckVM_isEmpty(duckVM_t *duckVM, dl_bool_t *result);


/* Advanced API
   Here be dragons and use-after-frees. */

/* It says "user" but: Advanced. */
duckVM_object_t duckVM_object_makeUser(void *data,
                                       dl_error_t (*marker)(duckVM_gclist_t *, dl_array_t *, struct duckVM_object_s *),
                                       dl_error_t (*destructor)(duckVM_gclist_t *, struct duckVM_object_s *));

/* Copy an object onto the heap. Advanced. */
dl_error_t duckVM_allocateHeapObject(duckVM_t *duckVM, duckVM_object_t **heapObjectOut, duckVM_object_t objectIn);

dl_error_t duckVM_object_pop(duckVM_t *duckVM, duckVM_object_t *object);
dl_error_t duckVM_object_push(duckVM_t *duckVM, duckVM_object_t *object);

duckVM_object_t duckVM_object_makeBoolean(dl_bool_t boolean);
duckVM_object_t duckVM_object_makeInteger(dl_ptrdiff_t integer);
duckVM_object_t duckVM_object_makeFloat(double floatingPoint);
dl_error_t duckVM_object_makeString(duckVM_t *duckVM,
                                    duckVM_object_t *stringOut,
                                    dl_uint8_t *stringIn,
                                    dl_size_t stringIn_length);
dl_error_t duckVM_object_makeSymbol(duckVM_t *duckVM,
                                    duckVM_object_t *symbolOut,
                                    dl_size_t id,
                                    dl_uint8_t *string,
                                    dl_size_t string_length);
void duckVM_object_makeCompressedSymbol(duckVM_object_t *symbolOut, dl_size_t id);
duckVM_object_t duckVM_object_makeList(duckVM_object_t *cons);
duckVM_object_t duckVM_object_makeCons(duckVM_object_t *car, duckVM_object_t *cdr);
duckVM_object_t duckVM_object_makeClosure(dl_ptrdiff_t name,
                                          duckVM_object_t *bytecode,
                                          duckVM_object_t *upvalueArray,
                                          dl_uint8_t arity,
                                          dl_bool_t variadic);

dl_error_t duckVM_closure_getUpvalueArray(duckVM_closure_t closure, duckVM_upvalueArray_t *upvalueArray);
dl_error_t duckVM_closure_setUpvalue(duckVM_t *duckVM,
                                     duckVM_closure_t closure,
                                     duckVM_object_t *object,
                                     dl_ptrdiff_t index);
dl_error_t duckVM_upvalueArray_getUpvalue(duckVM_t *duckVM,
                                          duckVM_upvalueArray_t upvalueArray,
                                          duckVM_object_t *object,
                                          dl_ptrdiff_t index);
dl_error_t duckVM_upvalueArray_setUpvalue(duckVM_t *duckVM,
                                          duckVM_upvalueArray_t upvalueArray,
                                          duckVM_object_t *object,
                                          dl_ptrdiff_t index);


/* Pretty printing functions for debugging. */

dl_error_t duckVM_upvalue_type_prettyPrint(dl_array_t *string_array, duckVM_upvalue_type_t type);
dl_error_t duckVM_gclist_prettyPrint(dl_array_t *string_array, duckVM_gclist_t gclist);
dl_error_t duckVM_callFrame_prettyPrint(dl_array_t *string_array, duckVM_callFrame_t callFrame);
dl_error_t duckVM_prettyPrint(dl_array_t *string_array, duckVM_t duckVM);
dl_error_t duckVM_internalString_prettyPrint(dl_array_t *string_array, duckVM_internalString_t internalString);
dl_error_t duckVM_string_prettyPrint(dl_array_t *string_array, duckVM_string_t string);
dl_error_t duckVM_symbol_prettyPrint(dl_array_t *string_array, duckVM_symbol_t symbol);
dl_error_t duckVM_function_prettyPrint(dl_array_t *string_array, duckVM_function_t function);
dl_error_t duckVM_closure_prettyPrint(dl_array_t *string_array, duckVM_closure_t closure, duckVM_t duckVM);
dl_error_t duckVM_list_prettyPrint(dl_array_t *string_array, duckVM_list_t list, duckVM_t duckVM);
dl_error_t duckVM_cons_prettyPrint(dl_array_t *string_array, duckVM_cons_t cons, duckVM_t duckVM);
dl_error_t duckVM_upvalue_prettyPrint(dl_array_t *string_array, duckVM_upvalue_t upvalue, duckVM_t duckVM);
dl_error_t duckVM_upvalueArray_prettyPrint(dl_array_t *string_array,
                                           duckVM_upvalueArray_t upvalueArray,
                                           duckVM_t duckVM);
dl_error_t duckVM_internalVector_prettyPrint(dl_array_t *string_array,
                                             duckVM_internalVector_t internalVector,
                                             duckVM_t duckVM);
dl_error_t duckVM_vector_prettyPrint(dl_array_t *string_array, duckVM_vector_t vector, duckVM_t duckVM);
dl_error_t duckVM_bytecode_prettyPrint(dl_array_t *string_array, duckVM_bytecode_t bytecode);
dl_error_t duckVM_internalComposite_prettyPrint(dl_array_t *string_array,
                                                duckVM_internalComposite_t internalComposite,
                                                duckVM_t duckVM);
dl_error_t duckVM_composite_prettyPrint(dl_array_t *string_array, duckVM_composite_t composite, duckVM_t duckVM);
dl_error_t duckVM_user_prettyPrint(dl_array_t *string_array, duckVM_user_t user);
dl_error_t duckVM_object_type_prettyPrint(dl_array_t *string_array, duckVM_object_type_t object_type);
dl_error_t duckVM_object_prettyPrint(dl_array_t *string_array, duckVM_object_t object, duckVM_t duckVM);

#endif /* DUCKVM_H */
