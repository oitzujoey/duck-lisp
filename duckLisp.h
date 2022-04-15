
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
	duckLisp_functionType_generator,
} duckLisp_functionType_t;

typedef struct {
	char *name;
	dl_size_t name_length;
	dl_ptrdiff_t target;
	dl_array_t sources; // dl_ptrdiff_t
} duckLisp_label_t;

typedef struct {
	// All variable names in the current scope are stored here.
	dl_trie_t locals_trie;   // Points to stack objects.
	dl_trie_t statics_trie;   // Points to static objects.
	
	dl_trie_t functions_trie;
	dl_size_t functions_length;
	dl_trie_t generators_trie;  // Points to generator stack callbacks.
	dl_size_t generators_length;
	
	dl_trie_t labels_trie;
} duckLisp_scope_t;

typedef struct {
	dl_memoryAllocation_t *memoryAllocation;
	
	dl_array_t errors;
	
	dl_array_t source; // dl_array_t:char
	
	// This is where we keep all of our variables.
	// dl_array_t stack;  // dl_array_t:duckLisp_object_t
	// This is where we keep everything that needs to be scoped.
	dl_array_t scope_stack;  // dl_array_t:duckLisp_scope_t:{dl_trie_t}
	// Points to the start of the local variables on the stack for this scope.
	// dl_ptrdiff_t frame_pointer;
	dl_size_t locals_length;
	dl_size_t statics_length;
	
	/* dl_array_t bytecode;    // dl_array_t:uint8_t */
	dl_array_t generators_stack; // dl_array_t:dl_error_t(*)(duckLisp_t*, const duckLisp_ast_expression_t)
	dl_array_t labels;  // duckLisp_label_t

	dl_size_t gensym_number;
} duckLisp_t;

typedef enum {
	duckLisp_instructionClass_nop = 0,
	duckLisp_instructionClass_pushString,
	duckLisp_instructionClass_pushInteger,
	duckLisp_instructionClass_pushIndex,
	duckLisp_instructionClass_call,
	duckLisp_instructionClass_ccall,
	duckLisp_instructionClass_jump,
	duckLisp_instructionClass_brz,
	duckLisp_instructionClass_brnz,
	duckLisp_instructionClass_move,
	duckLisp_instructionClass_not,
	duckLisp_instructionClass_add,
	duckLisp_instructionClass_sub,
	duckLisp_instructionClass_equal,
	duckLisp_instructionClass_less,
	duckLisp_instructionClass_greater,
	duckLisp_instructionClass_cons,
	duckLisp_instructionClass_pop,
	duckLisp_instructionClass_return,
	duckLisp_instructionClass_pseudo_label,
} duckLisp_instructionClass_t;

// Max number of instructions must be 256.
// Order must be 8→16→32 otherwise there will be optimization problems.
typedef enum {
	duckLisp_instruction_nop = 0,
	
	duckLisp_instruction_pushString8,
	duckLisp_instruction_pushString16,
	duckLisp_instruction_pushString32,
	
	duckLisp_instruction_pushInteger8,
	duckLisp_instruction_pushInteger16,
	duckLisp_instruction_pushInteger32,
	
	duckLisp_instruction_pushIndex8,
	duckLisp_instruction_pushIndex16,
	duckLisp_instruction_pushIndex32,
	
	duckLisp_instruction_call8,
	duckLisp_instruction_call16,
	duckLisp_instruction_call32,
	
	duckLisp_instruction_ccall8,
	duckLisp_instruction_ccall16,
	duckLisp_instruction_ccall32,
	
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
	
	duckLisp_instruction_pop8,
	duckLisp_instruction_pop16,
	duckLisp_instruction_pop32,
	
	duckLisp_instruction_return0,
	duckLisp_instruction_return8,
	duckLisp_instruction_return16,
	duckLisp_instruction_return32,
} duckLisp_instruction_t;

typedef enum {
	duckLisp_instructionArgClass_type_none,
	duckLisp_instructionArgClass_type_integer,
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
	// struct duckLisp_instructionObject_s *next;
	// struct duckLisp_instructionObject_s *previous;
} duckLisp_instructionObject_t;

dl_error_t DECLSPEC duckLisp_init(duckLisp_t *duckLisp);
void DECLSPEC duckLisp_quit(duckLisp_t *duckLisp);

dl_error_t DECLSPEC duckLisp_error_pushRuntime(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length);
dl_error_t DECLSPEC duckLisp_checkArgsAndReportError(
    duckLisp_t *duckLisp, duckLisp_ast_expression_t astExpression,
    const dl_size_t numArgs);

void cst_compoundExpression_init(
    duckLisp_cst_compoundExpression_t *compoundExpression);
void ast_compoundExpression_init(
    duckLisp_ast_compoundExpression_t *compoundExpression);
dl_error_t duckLisp_cst_append(duckLisp_t *duckLisp,
                               duckLisp_cst_compoundExpression_t *cst,
                               const dl_ptrdiff_t index, dl_bool_t throwErrors);
dl_error_t duckLisp_ast_append(duckLisp_t *duckLisp,
                               duckLisp_ast_compoundExpression_t *ast,
                               duckLisp_cst_compoundExpression_t *cst,
                               const dl_ptrdiff_t index, dl_bool_t throwErrors);
dl_error_t DECLSPEC duckLisp_cst_print(duckLisp_t *duckLisp,
                                       duckLisp_cst_compoundExpression_t cst);
dl_error_t DECLSPEC duckLisp_ast_print(duckLisp_t *duckLisp,
                                       duckLisp_ast_compoundExpression_t ast);
dl_error_t cst_compoundExpression_quit(
    duckLisp_t *duckLisp,
    duckLisp_cst_compoundExpression_t *compoundExpression);
dl_error_t ast_print_compoundExpression(
    duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression);

dl_error_t duckLisp_scope_getLocalIndexFromName(duckLisp_t *duckLisp, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length);

dl_error_t duckLisp_emit_pop(duckLisp_t *duckLisp, dl_array_t *assembly,
                             const dl_size_t count);
dl_error_t duckLisp_emit_not(duckLisp_t *duckLisp, dl_array_t *assembly,
                             const dl_ptrdiff_t index);
dl_error_t duckLisp_emit_add(duckLisp_t *duckLisp, dl_array_t *assembly,
                             const dl_ptrdiff_t destination_index,
                             const dl_ptrdiff_t source_index);
dl_error_t duckLisp_emit_greater(duckLisp_t *duckLisp, dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2);
dl_error_t duckLisp_emit_less(duckLisp_t *duckLisp, dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2);
dl_error_t duckLisp_emit_move(duckLisp_t *duckLisp, dl_array_t *assembly,
                              const dl_ptrdiff_t destination_index,
                              const dl_ptrdiff_t source_index);
dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp, dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer);
dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp, dl_array_t *assembly,
                                   dl_ptrdiff_t *stackIndex,
                                   const dl_ptrdiff_t index);
dl_error_t DECLSPEC duckLisp_emit_pushString(duckLisp_t *duckLisp,
                                             dl_array_t *bytecodeBuffer,
                                             dl_ptrdiff_t *stackIndex,
                                             char *string,
                                             dl_size_t string_length);
dl_error_t duckLisp_emit_brnz(duckLisp_t *duckLisp, dl_array_t *assembly,
                              char *label, dl_size_t label_length, int pops);
dl_error_t duckLisp_emit_jump(duckLisp_t *duckLisp, dl_array_t *assembly,
                              char *label, dl_size_t label_length);

dl_error_t duckLisp_generator_noscope(duckLisp_t *duckLisp,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_compile_compoundExpression(duckLisp_t *duckLisp, dl_array_t *assembly, char *functionName,
											   const dl_size_t functionName_length,
											   duckLisp_ast_compoundExpression_t *compoundExpression,
											   dl_ptrdiff_t *index, duckLisp_ast_type_t *type,
											   dl_bool_t pushReference);
dl_error_t duckLisp_compile_expression(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, unsigned char **bytecode, dl_size_t *bytecode_length,
                               char *source, const dl_size_t source_length);

dl_error_t DECLSPEC duckLisp_pushScope(duckLisp_t *duckLisp, duckLisp_scope_t *scope);
dl_error_t DECLSPEC duckLisp_popScope(duckLisp_t *duckLisp, duckLisp_scope_t *scope);
dl_error_t DECLSPEC duckLisp_scope_addObject(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length);
// dl_error_t duckLisp_pushObject(duckLisp_t *duckLisp, const char *name, const
// dl_size_t name_length, const duckLisp_object_t object);
dl_error_t
duckLisp_addInterpretedFunction(duckLisp_t *duckLisp,
                                const duckLisp_ast_identifier_t name);
dl_error_t DECLSPEC
duckLisp_addGenerator(duckLisp_t *duckLisp,
                      dl_error_t (*callback)(duckLisp_t *, dl_array_t *,
                                             duckLisp_ast_expression_t *),
                      const char *name, const dl_size_t name_length);
dl_error_t DECLSPEC duckLisp_linkCFunction(duckLisp_t *duckLisp, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length);
// dl_error_t duckLisp_pushGenerator(duckLisp_t *duckLisp, const char *name,
// const dl_size_t name_length,
//                                   const dl_error_t(*generator)(duckLisp_t*,
//                                   const duckLisp_ast_expression_t));

char *duckLisp_disassemble(dl_memoryAllocation_t *memoryAllocation, const dl_uint8_t *bytecode, const dl_size_t length);

#endif // DUCKLISP_H
