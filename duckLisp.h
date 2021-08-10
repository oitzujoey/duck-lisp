
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

typedef struct {
	dl_memoryAllocation_t memoryAllocation;
	array_t errors;
	duckLisp_ast_expression_t ast;
	duckLisp_cst_expression_t cst;
	array_t source;
} duckLisp_t;

dl_error_t duckLisp_init(duckLisp_t *duckLisp, void *memory, dl_size_t size);
void duckLisp_quit(duckLisp_t *duckLisp);

dl_error_t duckLisp_cst_print(duckLisp_t *duckLisp);
dl_error_t duckLisp_ast_print(duckLisp_t *duckLisp);
dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, dl_ptrdiff_t *handle, const char *source, const dl_size_t source_length);

#endif // DUCKLISP_H
