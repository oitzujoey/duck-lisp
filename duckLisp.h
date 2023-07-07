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

#ifndef DUCKLISP_H
#define DUCKLISP_H

#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/array.h"
#include "DuckLib/trie.h"
#include "duckVM.h"

/*
node
	nodes_indices
	nodes_types
	nodes_length
	value
*/

/*
===
CST
===
*/

typedef struct {
	dl_size_t token_length;
	dl_ptrdiff_t token_index;
} duckLisp_cst_bool_t;

typedef struct {
	dl_size_t token_length;
	dl_ptrdiff_t token_index;
} duckLisp_cst_integer_t;

typedef struct {
	dl_size_t token_length;
	dl_ptrdiff_t token_index;
} duckLisp_cst_float_t;

typedef struct {
	dl_size_t token_length;
	dl_ptrdiff_t token_index;
} duckLisp_cst_string_t;


typedef struct {
	dl_size_t token_length;
	dl_ptrdiff_t token_index;
} duckLisp_cst_identifier_t;


typedef struct {
	struct duckLisp_cst_compoundExpression_s *compoundExpressions;
	dl_size_t compoundExpressions_length;
} duckLisp_cst_expression_t;


typedef enum {
	duckLisp_ast_type_none = 0,
	duckLisp_ast_type_expression,
	duckLisp_ast_type_identifier,
	duckLisp_ast_type_string,
	duckLisp_ast_type_float,
	duckLisp_ast_type_int,
	duckLisp_ast_type_bool,
} duckLisp_ast_type_t;

typedef struct duckLisp_cst_compoundExpression_s {
	union {
		duckLisp_cst_expression_t expression;
		duckLisp_cst_identifier_t identifier;
		duckLisp_cst_string_t string;
		duckLisp_cst_float_t floatingPoint;
		duckLisp_cst_integer_t integer;
		duckLisp_cst_bool_t boolean;
	} value;
	duckLisp_ast_type_t type;
} duckLisp_cst_compoundExpression_t;

/*
===
AST
===
*/

typedef struct {
	dl_bool_t value;
} duckLisp_ast_bool_t;

typedef struct {
	dl_ptrdiff_t value;
} duckLisp_ast_integer_t;

typedef struct {
	double value;
} duckLisp_ast_float_t;

typedef struct {
	char *value;
	dl_size_t value_length;
} duckLisp_ast_string_t;

typedef struct {
	char *value;
	dl_size_t value_length;
} duckLisp_ast_identifier_t;

typedef struct {
	struct duckLisp_ast_compoundExpression_s *compoundExpressions;
	dl_size_t compoundExpressions_length;
} duckLisp_ast_expression_t;


typedef struct duckLisp_ast_compoundExpression_s {
	union {
		duckLisp_ast_expression_t expression;
		duckLisp_ast_identifier_t identifier;
		duckLisp_ast_string_t string;
		duckLisp_ast_bool_t boolean;
		duckLisp_ast_integer_t integer;
		duckLisp_ast_float_t floatingPoint;
	} value;
	duckLisp_ast_type_t type;
} duckLisp_ast_compoundExpression_t;

/*
=========
Variables
=========
*/

// typedef enum {
// 	duckLisp_error_code_none = 0,
// 	duckLisp_error_code_notAnExpression,
// 	duckLisp_error_code_extraOpenParenthesis,
// 	duckLisp_error_code_unexpectedEndOfFile,
// 	duckLisp_error_code_syntax
// } duckLisp_error_code_t;

typedef struct {
	const char *message;
	dl_size_t message_length;
	dl_ptrdiff_t index;
	// dl_size_t line;
	// dl_size_t offset;
} duckLisp_error_t;

typedef enum {
	duckLisp_functionType_none = 0,
	duckLisp_functionType_c,
	duckLisp_functionType_ducklisp,
	/* Indicates that this procedure captures no upvalues. */
	duckLisp_functionType_ducklisp_pure,
	duckLisp_functionType_generator,
	duckLisp_functionType_macro
} duckLisp_functionType_t;

typedef struct {
	dl_ptrdiff_t source;
	dl_bool_t absolute;
} duckLisp_label_source_t;

typedef struct {
	dl_ptrdiff_t target;
	dl_array_t sources; /* dl_array_t:duckLisp_label_source_t */
} duckLisp_label_t;

typedef struct {
	/* All variable names in the current scope are stored here. */
	dl_trie_t locals_trie;   /* Points to stack objects. */

	dl_trie_t functions_trie;  /* Records all the function types in this scope. */
	dl_size_t functions_length;

	dl_trie_t macros_trie;  /* Index of macro in `macros`. */
	dl_size_t macros_length;

	dl_trie_t labels_trie;
	dl_bool_t function_scope;  /* Used to determine when to create a deep upvalue. */

	/* Upvalues */
	dl_ptrdiff_t *scope_uvs;
	dl_size_t scope_uvs_length;

	dl_ptrdiff_t *function_uvs;
	dl_size_t function_uvs_length;
} duckLisp_scope_t;

typedef struct {
	/* This is where we keep everything that needs to be scoped. */
	dl_array_t scope_stack;  /* dl_array_t:duckLisp_scope_t:{dl_trie_t} */
	dl_size_t locals_length;
	dl_size_t label_number;
	dl_array_t assembly;  /* dl_array_t:duckLisp_instructionObject_t This is always the true assembly array. */
} duckLisp_subCompileState_t;

/* This can safely be deleted after each compile. */
typedef struct duckLisp_compileState_s {
	duckLisp_subCompileState_t runtimeCompileState;
	duckLisp_subCompileState_t comptimeCompileState;
	duckLisp_subCompileState_t *currentCompileState;
} duckLisp_compileState_t;

/* This remains until the compiler is destroyed. */
typedef struct {
	dl_memoryAllocation_t *memoryAllocation;

	dl_array_t errors;  /* duckLisp_error_t */

	dl_array_t generators_stack; /* dl_array_t:dl_error_t(*)(duckLisp_t*, const duckLisp_ast_expression_t) */
	dl_trie_t generators_trie;  /* Points to generator stack callbacks. */
	dl_size_t generators_length;

	dl_trie_t callbacks_trie;  /* Points to runtime C callbacks. */

	dl_size_t gensym_number;

	dl_trie_t symbols_trie;  /* Index points to the string in `symbols_array` */
	dl_array_t symbols_array;  /* duckLisp_ast_identifier_t */

	/* A VM instance for macros. */
	duckVM_t vm;
} duckLisp_t;

typedef enum {
	duckLisp_instructionClass_nop = 0,
	duckLisp_instructionClass_pushString,
	duckLisp_instructionClass_pushBoolean,
	duckLisp_instructionClass_pushInteger,
	duckLisp_instructionClass_pushDoubleFloat,
	duckLisp_instructionClass_pushIndex,
	duckLisp_instructionClass_pushSymbol,
	duckLisp_instructionClass_pushUpvalue,
	duckLisp_instructionClass_pushClosure,
	duckLisp_instructionClass_pushVaClosure,
	duckLisp_instructionClass_pushGlobal,
	duckLisp_instructionClass_setUpvalue,
	duckLisp_instructionClass_setStatic,
	duckLisp_instructionClass_releaseUpvalues,
	duckLisp_instructionClass_funcall,
	duckLisp_instructionClass_apply,
	duckLisp_instructionClass_call,
	duckLisp_instructionClass_ccall,
	duckLisp_instructionClass_acall,
	duckLisp_instructionClass_jump,
	duckLisp_instructionClass_brz,
	duckLisp_instructionClass_brnz,
	duckLisp_instructionClass_move,
	duckLisp_instructionClass_not,
	duckLisp_instructionClass_mul,
	duckLisp_instructionClass_div,
	duckLisp_instructionClass_add,
	duckLisp_instructionClass_sub,
	duckLisp_instructionClass_equal,
	duckLisp_instructionClass_less,
	duckLisp_instructionClass_greater,
	duckLisp_instructionClass_cons,
	duckLisp_instructionClass_vector,
	duckLisp_instructionClass_makeVector,
	duckLisp_instructionClass_getVecElt,
	duckLisp_instructionClass_setVecElt,
	duckLisp_instructionClass_car,
	duckLisp_instructionClass_cdr,
	duckLisp_instructionClass_setCar,
	duckLisp_instructionClass_setCdr,
	duckLisp_instructionClass_nullp,
	duckLisp_instructionClass_typeof,
	duckLisp_instructionClass_makeType,
	duckLisp_instructionClass_makeInstance,
	duckLisp_instructionClass_compositeValue,
	duckLisp_instructionClass_compositeFunction,
	duckLisp_instructionClass_setCompositeValue,
	duckLisp_instructionClass_setCompositeFunction,
	duckLisp_instructionClass_makeString,
	duckLisp_instructionClass_concatenate,
	duckLisp_instructionClass_substring,
	duckLisp_instructionClass_length,
	duckLisp_instructionClass_symbolString,
	duckLisp_instructionClass_symbolId,
	duckLisp_instructionClass_pop,
	duckLisp_instructionClass_return,
	duckLisp_instructionClass_yield,
	duckLisp_instructionClass_nil,
	duckLisp_instructionClass_pseudo_label,
	duckLisp_instructionClass_internalNop,
} duckLisp_instructionClass_t;

// Max number of instructions must be 256.
// Order must be 8→16→32 otherwise there will be optimization problems.
typedef enum {
	duckLisp_instruction_nop = 0,

	duckLisp_instruction_pushString8,
	duckLisp_instruction_pushString16,
	duckLisp_instruction_pushString32,

	duckLisp_instruction_pushBooleanFalse,
	duckLisp_instruction_pushBooleanTrue,

	duckLisp_instruction_pushInteger8,
	duckLisp_instruction_pushInteger16,
	duckLisp_instruction_pushInteger32,

	duckLisp_instruction_pushDoubleFloat,

	duckLisp_instruction_pushIndex8,
	duckLisp_instruction_pushIndex16,
	duckLisp_instruction_pushIndex32,

	duckLisp_instruction_pushSymbol8,
	duckLisp_instruction_pushSymbol16,
	duckLisp_instruction_pushSymbol32,

	duckLisp_instruction_pushUpvalue8,
	duckLisp_instruction_pushUpvalue16,
	duckLisp_instruction_pushUpvalue32,

	duckLisp_instruction_pushClosure8,
	duckLisp_instruction_pushClosure16,
	duckLisp_instruction_pushClosure32,

	duckLisp_instruction_pushVaClosure8,
	duckLisp_instruction_pushVaClosure16,
	duckLisp_instruction_pushVaClosure32,

	duckLisp_instruction_pushGlobal8,

	duckLisp_instruction_setUpvalue8,
	duckLisp_instruction_setUpvalue16,
	duckLisp_instruction_setUpvalue32,

	duckLisp_instruction_setStatic8,

	duckLisp_instruction_releaseUpvalues8,
	duckLisp_instruction_releaseUpvalues16,
	duckLisp_instruction_releaseUpvalues32,

	duckLisp_instruction_funcall8,
	duckLisp_instruction_funcall16,
	duckLisp_instruction_funcall32,

	duckLisp_instruction_apply8,
	duckLisp_instruction_apply16,
	duckLisp_instruction_apply32,

	duckLisp_instruction_call8,
	duckLisp_instruction_call16,
	duckLisp_instruction_call32,

	duckLisp_instruction_ccall8,
	duckLisp_instruction_ccall16,
	duckLisp_instruction_ccall32,

	duckLisp_instruction_acall8,
	duckLisp_instruction_acall16,
	duckLisp_instruction_acall32,

	duckLisp_instruction_jump8,
	duckLisp_instruction_jump16,
	duckLisp_instruction_jump32,

	duckLisp_instruction_brz8,
	duckLisp_instruction_brz16,
	duckLisp_instruction_brz32,

	duckLisp_instruction_brnz8,
	duckLisp_instruction_brnz16,
	duckLisp_instruction_brnz32,

	duckLisp_instruction_move8,
	duckLisp_instruction_move16,
	duckLisp_instruction_move32,

	duckLisp_instruction_not8,
	duckLisp_instruction_not16,
	duckLisp_instruction_not32,

	duckLisp_instruction_mul8,
	duckLisp_instruction_mul16,
	duckLisp_instruction_mul32,

	duckLisp_instruction_div8,
	duckLisp_instruction_div16,
	duckLisp_instruction_div32,

	duckLisp_instruction_add8,
	duckLisp_instruction_add16,
	duckLisp_instruction_add32,

	duckLisp_instruction_sub8,
	duckLisp_instruction_sub16,
	duckLisp_instruction_sub32,

	duckLisp_instruction_equal8,
	duckLisp_instruction_equal16,
	duckLisp_instruction_equal32,

	duckLisp_instruction_greater8,
	duckLisp_instruction_greater16,
	duckLisp_instruction_greater32,

	duckLisp_instruction_less8,
	duckLisp_instruction_less16,
	duckLisp_instruction_less32,

	duckLisp_instruction_cons8,
	duckLisp_instruction_cons16,
	duckLisp_instruction_cons32,

	duckLisp_instruction_vector8,
	duckLisp_instruction_vector16,
	duckLisp_instruction_vector32,

	duckLisp_instruction_makeVector8,
	duckLisp_instruction_makeVector16,
	duckLisp_instruction_makeVector32,

	duckLisp_instruction_getVecElt8,
	duckLisp_instruction_getVecElt16,
	duckLisp_instruction_getVecElt32,

	duckLisp_instruction_setVecElt8,
	duckLisp_instruction_setVecElt16,
	duckLisp_instruction_setVecElt32,

	duckLisp_instruction_car8,
	duckLisp_instruction_car16,
	duckLisp_instruction_car32,

	duckLisp_instruction_cdr8,
	duckLisp_instruction_cdr16,
	duckLisp_instruction_cdr32,

	duckLisp_instruction_setCar8,
	duckLisp_instruction_setCar16,
	duckLisp_instruction_setCar32,

	duckLisp_instruction_setCdr8,
	duckLisp_instruction_setCdr16,
	duckLisp_instruction_setCdr32,

	duckLisp_instruction_nullp8,
	duckLisp_instruction_nullp16,
	duckLisp_instruction_nullp32,

	duckLisp_instruction_typeof8,
	duckLisp_instruction_typeof16,
	duckLisp_instruction_typeof32,

	duckLisp_instruction_makeType,

	duckLisp_instruction_makeInstance8,
	duckLisp_instruction_makeInstance16,
	duckLisp_instruction_makeInstance32,

	duckLisp_instruction_compositeValue8,
	duckLisp_instruction_compositeValue16,
	duckLisp_instruction_compositeValue32,

	duckLisp_instruction_compositeFunction8,
	duckLisp_instruction_compositeFunction16,
	duckLisp_instruction_compositeFunction32,

	duckLisp_instruction_setCompositeValue8,
	duckLisp_instruction_setCompositeValue16,
	duckLisp_instruction_setCompositeValue32,

	duckLisp_instruction_setCompositeFunction8,
	duckLisp_instruction_setCompositeFunction16,
	duckLisp_instruction_setCompositeFunction32,

	duckLisp_instruction_makeString8,
	duckLisp_instruction_makeString16,
	duckLisp_instruction_makeString32,

	duckLisp_instruction_concatenate8,
	duckLisp_instruction_concatenate16,
	duckLisp_instruction_concatenate32,

	duckLisp_instruction_substring8,
	duckLisp_instruction_substring16,
	duckLisp_instruction_substring32,

	duckLisp_instruction_length8,
	duckLisp_instruction_length16,
	duckLisp_instruction_length32,

	duckLisp_instruction_symbolString8,
	duckLisp_instruction_symbolString16,
	duckLisp_instruction_symbolString32,

	duckLisp_instruction_symbolId8,
	duckLisp_instruction_symbolId16,
	duckLisp_instruction_symbolId32,

	duckLisp_instruction_pop8,
	duckLisp_instruction_pop16,
	duckLisp_instruction_pop32,

	duckLisp_instruction_return0,
	duckLisp_instruction_return8,
	duckLisp_instruction_return16,
	duckLisp_instruction_return32,

	duckLisp_instruction_yield,

	duckLisp_instruction_nil,
} duckLisp_instruction_t;

typedef enum {
	duckLisp_instructionArgClass_type_none,
	duckLisp_instructionArgClass_type_integer,
	duckLisp_instructionArgClass_type_doubleFloat,
	duckLisp_instructionArgClass_type_index,
	duckLisp_instructionArgClass_type_string,
} duckLisp_instructionArgClass_type_t;

typedef enum {
	duckLisp_instructionArg_type_none,
	duckLisp_instructionArg_type_integer8,
	duckLisp_instructionArg_type_integer16,
	duckLisp_instructionArg_type_index8,
	duckLisp_instructionArg_type_index16,
	duckLisp_instructionArg_type_string,
} duckLisp_instructionArg_type_t;

typedef struct duckLisp_instructionArgs_s {
	union {
		int integer;
		dl_ptrdiff_t index;
		double doubleFloat;
		struct {
			char *value;
			dl_size_t value_length;
		} string;
	} value;
	duckLisp_instructionArgClass_type_t type;
} duckLisp_instructionArgClass_t;

typedef struct duckLisp_instructionObject_s {
	duckLisp_instructionClass_t instructionClass;
	dl_array_t args;
} duckLisp_instructionObject_t;

dl_error_t DECLSPEC duckLisp_init(duckLisp_t *duckLisp,
                                  dl_memoryAllocation_t *memoryAllocation,
                                  dl_size_t maxComptimeVmObjects);
void DECLSPEC duckLisp_quit(duckLisp_t *duckLisp);

void DECLSPEC duckLisp_compileState_init(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState);
dl_error_t DECLSPEC duckLisp_compileState_quit(duckLisp_compileState_t *compileState);

dl_error_t DECLSPEC duckLisp_error_pushRuntime(duckLisp_t *duckLisp,
                                               const char *message,
                                               const dl_size_t message_length);
dl_error_t DECLSPEC duckLisp_checkArgsAndReportError(duckLisp_t *duckLisp, duckLisp_ast_expression_t astExpression,
                                                     const dl_size_t numArgs,
                                                     const dl_bool_t variadic);
dl_error_t DECLSPEC duckLisp_checkTypeAndReportError(duckLisp_t *duckLisp,
                                                     duckLisp_ast_identifier_t functionName,
                                                     duckLisp_ast_compoundExpression_t astCompoundExpression,
                                                     const duckLisp_ast_type_t type);

void cst_compoundExpression_init(duckLisp_cst_compoundExpression_t *compoundExpression);
void ast_compoundExpression_init(duckLisp_ast_compoundExpression_t *compoundExpression);
dl_error_t duckLisp_cst_append(duckLisp_t *duckLisp,
                               const char *source,
                               const dl_size_t source_length,
                               duckLisp_cst_compoundExpression_t *cst,
                               const dl_ptrdiff_t index, dl_bool_t throwErrors);
dl_error_t duckLisp_ast_append(duckLisp_t *duckLisp,
                               const char *source,
                               duckLisp_ast_compoundExpression_t *ast,
                               duckLisp_cst_compoundExpression_t *cst,
                               const dl_ptrdiff_t index, dl_bool_t throwErrors);
dl_error_t DECLSPEC duckLisp_cst_print(duckLisp_t *duckLisp,
                                       const char *source,
                                       duckLisp_cst_compoundExpression_t cst);
dl_error_t DECLSPEC duckLisp_ast_print(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t ast);
dl_error_t cst_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression);
dl_error_t ast_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression);
dl_error_t ast_print_compoundExpression(duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression);

dl_error_t duckLisp_scope_getLocalIndexFromName(duckLisp_subCompileState_t *subCompileState,
                                                dl_ptrdiff_t *index,
                                                const char *name,
                                                const dl_size_t name_length);

dl_error_t duckLisp_emit_pop(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_size_t count);
dl_error_t duckLisp_emit_not(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t index);
dl_error_t duckLisp_emit_add(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t destination_index,
                             const dl_ptrdiff_t source_index);
dl_error_t duckLisp_emit_greater(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 const dl_ptrdiff_t source_index1,
                                 const dl_ptrdiff_t source_index2);
dl_error_t duckLisp_emit_less(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2);
dl_error_t duckLisp_emit_move(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t destination_index,
                              const dl_ptrdiff_t source_index);
dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer);
dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t index);
dl_error_t DECLSPEC duckLisp_emit_pushString(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *bytecodeBuffer,
                                             dl_ptrdiff_t *stackIndex,
                                             char *string,
                                             dl_size_t string_length);
dl_error_t duckLisp_emit_brnz(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length,
                              int pops);
dl_error_t duckLisp_emit_jump(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length);

dl_error_t duckLisp_consToExprAST(duckLisp_t *duckLisp,
                                  duckLisp_ast_compoundExpression_t *ast,
                                  duckLisp_object_t *cons);
dl_error_t duckLisp_consToConsAST(duckLisp_t *duckLisp,
                                  duckLisp_ast_compoundExpression_t *ast,
                                  duckLisp_object_t *cons);
dl_error_t duckLisp_objectToAST(duckLisp_t *duckLisp,
                                duckLisp_ast_compoundExpression_t *ast,
                                duckLisp_object_t *object,
                                dl_bool_t useExprs);


dl_error_t duckLisp_generator_noscope(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_createVar(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_lambda(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_compile_compoundExpression(duckLisp_t *duckLisp,
                                               duckLisp_compileState_t *compileState,
                                               dl_array_t *assembly,
                                               char *functionName,
											   const dl_size_t functionName_length,
											   duckLisp_ast_compoundExpression_t *compoundExpression,
											   dl_ptrdiff_t *index,
                                               duckLisp_ast_type_t *type,
											   dl_bool_t pushReference);
dl_error_t duckLisp_compile_expression(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       char *functionName,
                                       const dl_size_t functionName_length,
                                       duckLisp_ast_expression_t *expression,
                                       dl_ptrdiff_t *index);

dl_error_t duckLisp_assemble(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *bytecode,
                             dl_array_t *assembly);
dl_error_t duckLisp_compileAST(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *bytecode,
							   duckLisp_ast_compoundExpression_t astCompoundexpression);

dl_error_t duckLisp_symbol_create(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length);
dl_ptrdiff_t duckLisp_symbol_nameToValue(const duckLisp_t *duckLisp, const char *name, const dl_size_t name_length);
dl_error_t duckLisp_loadString(duckLisp_t *duckLisp,
                               unsigned char **bytecode,
                               dl_size_t *bytecode_length,
                               const char *source,
                               const dl_size_t source_length);

dl_error_t DECLSPEC duckLisp_pushScope(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       duckLisp_scope_t *scope,
                                       dl_bool_t is_function);
dl_error_t DECLSPEC duckLisp_popScope(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      duckLisp_scope_t *scope);
dl_error_t DECLSPEC duckLisp_addStatic(duckLisp_t *duckLisp,
                                       const char *name,
                                       const dl_size_t name_length,
                                       dl_ptrdiff_t *index);
dl_error_t DECLSPEC duckLisp_scope_addObject(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             const char *name,
                                             const dl_size_t name_length);
// dl_error_t duckLisp_pushObject(duckLisp_t *duckLisp, const char *name, const
// dl_size_t name_length, const duckLisp_object_t object);
dl_error_t duckLisp_addInterpretedFunction(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           const duckLisp_ast_identifier_t name,
                                           const dl_bool_t pure);
dl_error_t duckLisp_addInterpretedGenerator(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            const duckLisp_ast_identifier_t name);
dl_error_t DECLSPEC  duckLisp_addGenerator(duckLisp_t *duckLisp,
                                           dl_error_t (*callback)(duckLisp_t*,
                                                                  duckLisp_compileState_t *,
                                                                  dl_array_t*,
                                                                  duckLisp_ast_expression_t*),
                                           const char *name,
                                           const dl_size_t name_length);
dl_error_t DECLSPEC duckLisp_linkCFunction(duckLisp_t *duckLisp,
                                           dl_error_t (*callback)(duckVM_t *),
                                           const char *name,
                                           const dl_size_t name_length);
// dl_error_t duckLisp_pushGenerator(duckLisp_t *duckLisp, const char *name,
// const dl_size_t name_length,
//                                   const dl_error_t(*generator)(duckLisp_t*,
//                                   const duckLisp_ast_expression_t));

char *duckLisp_disassemble(dl_memoryAllocation_t *memoryAllocation, const dl_uint8_t *bytecode, const dl_size_t length);

#endif /* DUCKLISP_H */
