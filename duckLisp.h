
#ifndef DUCKLISP_H
#define DUCKLISP_H

#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/array.h"
#include "DuckLib/trie.h"

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


typedef enum {
	cst_number_type_none = 0,
	cst_number_type_bool,
	cst_number_type_int,
	cst_number_type_float
} duckLisp_cst_number_type_t;

typedef struct {
	union {
		duckLisp_cst_bool_t boolean;
		duckLisp_cst_integer_t integer;
		duckLisp_cst_float_t floatingPoint;
	} value;
	duckLisp_cst_number_type_t type;
} duckLisp_cst_number_t;


typedef enum {
	cst_constant_type_none = 0,
	cst_constant_type_number,
	cst_constant_type_string
} duckLisp_cst_constant_type_t;

typedef struct {
	union {
		duckLisp_cst_number_t number;
		duckLisp_cst_string_t string;
	} value;
	duckLisp_cst_constant_type_t type;
} duckLisp_cst_constant_t;


typedef struct {
	dl_size_t token_length;
	dl_ptrdiff_t token_index;
} duckLisp_cst_identifier_t;


typedef struct {
	struct duckLisp_cst_compoundExpression_s *compoundExpressions;
	dl_size_t compoundExpressions_length;
} duckLisp_cst_expression_t;


typedef enum {
	cst_compoundExpression_type_none = 0,
	cst_compoundExpression_type_constant,
	cst_compoundExpression_type_identifier,
	cst_compoundExpression_type_expression
} duckLisp_cst_compoundExpression_type_t;

typedef struct duckLisp_cst_compoundExpression_s {
	union {
		duckLisp_cst_expression_t expression;
		duckLisp_cst_identifier_t identifier;
		duckLisp_cst_constant_t constant;
	} value;
	duckLisp_cst_compoundExpression_type_t type;
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


typedef enum {
	ast_number_type_none = 0,
	ast_number_type_bool,
	ast_number_type_int,
	ast_number_type_float
} duckLisp_ast_number_type_t;

typedef struct {
	union {
		duckLisp_ast_bool_t boolean;
		duckLisp_ast_integer_t integer;
		duckLisp_ast_float_t floatingPoint;
	} value;
	duckLisp_ast_number_type_t type;
} duckLisp_ast_number_t;


typedef enum {
	ast_constant_type_none = 0,
	ast_constant_type_number,
	ast_constant_type_string
} duckLisp_ast_constant_type_t;

typedef struct {
	union {
		duckLisp_ast_number_t number;
		duckLisp_ast_string_t string;
	} value;
	duckLisp_ast_constant_type_t type;
} duckLisp_ast_constant_t;


typedef struct {
	char *value;
	dl_size_t value_length;
} duckLisp_ast_identifier_t;


typedef struct {
	struct duckLisp_ast_compoundExpression_s *compoundExpressions;
	dl_size_t compoundExpressions_length;
} duckLisp_ast_expression_t;


typedef enum {
	ast_compoundExpression_type_none = 0,
	ast_compoundExpression_type_constant,
	ast_compoundExpression_type_identifier,
	ast_compoundExpression_type_expression
} duckLisp_ast_compoundExpression_type_t;

typedef struct duckLisp_ast_compoundExpression_s {
	union {
		duckLisp_ast_expression_t expression;
		duckLisp_ast_identifier_t identifier;
		duckLisp_ast_constant_t constant;
	} value;
	duckLisp_ast_compoundExpression_type_t type;
} duckLisp_ast_compoundExpression_t;

/*
=========
Variables
=========
*/

typedef enum {
	duckLisp_object_type_none,
	duckLisp_object_type_bool,
	duckLisp_object_type_integer,
	duckLisp_object_type_float,
	duckLisp_object_type_string,
	duckLisp_object_type_function
} duckLisp_object_type_t;

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
	duckLisp_functionType_generator,
} duckLisp_functionType_t;

typedef struct {
	// All variable names in the current scope are stored here.
	dl_trie_t variables_trie;   // Points to stack objects.
	dl_size_t variables_length;
	dl_trie_t functions_trie;
	dl_size_t functions_length;
	dl_trie_t generators_trie;  // Points to generator stack callbacks.
	dl_size_t generators_length;
} duckLisp_scope_t;

typedef struct {
	dl_memoryAllocation_t memoryAllocation;
	
	dl_array_t errors;
	
	dl_array_t source; // dl_array_t:char
	
	// This is where we keep all of our variables.
	dl_array_t stack;  // dl_array_t:duckLisp_object_t
	// This is where we keep everything that needs to be scoped.
	dl_array_t scope_stack;  // dl_array_t:duckLisp_scope_t:{dl_trie_t}
	// Points to the start of the local variables on the stack for this scope.
	dl_ptrdiff_t frame_pointer;
	
	dl_array_t bytecode;    // dl_array_t:uint8_t
	dl_array_t generators_stack; // dl_array_t:dl_error_t(*)(duckLisp_t*, const duckLisp_ast_expression_t)
} duckLisp_t;

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
			dl_ptrdiff_t bytecode_index;
			dl_error_t (*callback)(duckLisp_t *);
		} function;
	} value;
	duckLisp_object_type_t type;
} duckLisp_object_t;

// Max number of instructions must be 256.
typedef enum {
	duckLisp_instruction_nop = 0,
	duckLisp_instruction_pushString,
} duckLisp_instruction_t;

typedef enum {
	duckLisp_instructionClass_nop = 0,
	duckLisp_instructionClass_pushString,
	duckLisp_instructionClass_pushInteger,
	duckLisp_instructionClass_pushIndex,
	duckLisp_instructionClass_ccall,
} duckLisp_instructionClass_t;

typedef enum {
	duckLisp_instructionArg_type_none,
	duckLisp_instructionArg_type_integer,
	duckLisp_instructionArg_type_index,
	duckLisp_instructionArg_type_string,
} duckLisp_instructionArg_type_t;

typedef struct duckLisp_instructionArgs_s {
	union {
		int integer;
		dl_ptrdiff_t index;
		struct {
			char *value;
			dl_size_t value_length;
		} string;
	} value;
	duckLisp_instructionArg_type_t type;
} duckLisp_instructionArg_t;

typedef struct duckLisp_instructionObject_s {
	duckLisp_instructionClass_t instructionClass;
	dl_array_t args;
	struct duckLisp_instructionObject_s *next;
	struct duckLisp_instructionObject_s *previous;
} duckLisp_instructionObject_t;

dl_error_t duckLisp_init(duckLisp_t *duckLisp, void *memory, dl_size_t size);
void duckLisp_quit(duckLisp_t *duckLisp);

dl_error_t duckLisp_error_pushRuntime(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length);
dl_error_t duckLisp_checkArgsAndReportError(duckLisp_t *duckLisp, duckLisp_ast_expression_t astExpression, const dl_size_t numArgs);

dl_error_t duckLisp_cst_print(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t cst);
dl_error_t duckLisp_ast_print(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t ast);

dl_error_t duckLisp_emit_pushString(duckLisp_t *duckLisp, dl_array_t *bytecodeBuffer, dl_ptrdiff_t *stackIndex, char *string, dl_size_t string_length);
dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length, const char *source, const dl_size_t source_length);

void duckLisp_getArgLength(duckLisp_t *duckLisp, dl_size_t *length);
dl_error_t duckLisp_getArg(duckLisp_t *duckLisp, duckLisp_object_t *object, dl_ptrdiff_t index);
dl_error_t duckLisp_pushReturn(duckLisp_t *duckLisp, duckLisp_object_t object);
dl_error_t duckLisp_scope_addObject(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length);
// dl_error_t duckLisp_pushObject(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length, const duckLisp_object_t object);
dl_error_t duckLisp_addGenerator(duckLisp_t *duckLisp, dl_error_t (*callback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*),
                                 const char *name, const dl_size_t name_length);
dl_error_t duckLisp_linkCFunction(duckLisp_t *duckLisp, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length);
// dl_error_t duckLisp_pushGenerator(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length,
//                                   const dl_error_t(*generator)(duckLisp_t*, const duckLisp_ast_expression_t));

#endif // DUCKLISP_H
