
#ifndef DUCKLISP_H
#define DUCKLISP_H

#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/array.h"

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
	char *token;
	dl_size_t token_length;
	dl_ptrdiff_t string_index;
} duckLisp_cst_bool_t;

typedef struct {
	char *token;
	dl_size_t token_length;
	dl_ptrdiff_t string_index;
} duckLisp_cst_integer_t;

typedef struct {
	char *token;
	dl_size_t token_length;
	dl_ptrdiff_t string_index;
} duckLisp_cst_float_t;

typedef struct {
	char *token;
	dl_size_t token_length;
	dl_ptrdiff_t string_index;
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
	char *token;
	dl_size_t token_length;
	dl_ptrdiff_t string_index;
} duckLisp_cst_identifier_t;

typedef struct {
	struct duckLisp_cst_compoundExpression_s *compoundExpressions;
	dl_size_t compoundExpressions_length;
} duckLisp_cst_expression_t;


typedef enum {
	cst_compoundExpression_type_none = 0,
	cst_compoundExpression_type_expression,
	cst_compoundExpression_type_identifier,
	cst_compoundExpression_type_constant
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

typedef struct duckLisp_astNode_s {
	struct duckLisp_astNode_s *nodes;
	dl_size_t nodes_length;
} duckLisp_astNode_t;

typedef enum {
	node,
	
} duckLisp_token_type_t;

typedef struct {
	// duckLisp_astNode_t node;
	char **token;
	dl_ptrdiff_t **children;
	dl_size_t *children_length;
	
	dl_size_t tree_length;
} duckLisp_ast_t;

// typedef enum {
// 	duckLisp_error_code_none = 0,
// 	duckLisp_error_code_notAnExpression,
// 	duckLisp_error_code_extraOpenParenthesis,
// 	duckLisp_error_code_unexpectedEndOfFile,
// 	duckLisp_error_code_syntax
// } duckLisp_error_code_t;

typedef struct {
	const char *message;
	const dl_size_t error_length;
	dl_size_t index;
	// dl_size_t line;
	// dl_size_t offset;
} duckLisp_error_t;

typedef struct {
	dl_memoryAllocation_t memoryAllocation;
	array_t errors;
	duckLisp_ast_t ast;
	duckLisp_cst_compoundExpression_t cst;
	char **sources;
	dl_size_t *source_lengths;
	dl_size_t sources_length;
} duckLisp_t;

dl_error_t duckLisp_init(duckLisp_t *duckLisp, void *memory, dl_size_t size);
void duckLisp_quit(duckLisp_t *duckLisp);

dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, dl_ptrdiff_t *script_handle, const char *source, const dl_size_t source_length);

#endif // DUCKLISP_H
