/*
MIT License

Copyright (c) 2023 Joseph Herguth

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

#include "parenthesis-inferrer.h"
#include "DuckLib/string.h"
#include "duckLisp.h"
#include "duckVM.h"
#include "parser.h"

#include <stdio.h>


/* This contains the parenthesis inferrer for duck-lisp. When Forth-like syntax mode is enabled the raw AST from the
   reader is only partially complete. Forth syntax is not context free, so another stage (this one) is desirable. It
   *is* possible to implement the algorithm entirely in duck-lisp's recursive descent parser, but keeping the inferrer
   separate from the parser improves modularity and readability. The algorithm used for tree correction is similar to
   Forth's algorithm, the primary difference being that this variant requires a static type system since it operates on
   the entire tree at compile-time.

   Basic rules:
   * Literal value types are not touched: bool, int, float, string
   * Parenthesized expressions are not touched. Use of parentheses for a function call opts out of inference for that
   call. Parentheses are still inferred for the arguments if appropriate.
   * Callbacks are converted to literal identifiers. Callbacks are not candidates for inference.
   * Identifiers that do not occur at the start of a parenthesized expression may be candidates for inference.

   When an identifier that is a candidate for inference is encountered, its name is looked up in the function dictionary
   stack. If it was not found, then the identifier is assumed not to be a function call and inference is stopped. If it
   was found, then the type is returned.

   There are two basic types:

   I: Infer -- Inference will be run on this argument.
   L: Literal -- Inference will not be run on this argument or its sub-forms.

   Normal variables have the type `L`. Functions have a list as their type. It can be empty, or it can contain a
   combination of `L` and `I`. Variadic functions are indicated by `&rest` in the third-to-last position of the
   type. The default number of arguments for a variadic function is in the last position.
   Some examples: `()`, `(I I I)`, `(L I)`, `(L L L &rest 1 I)`.

   Types are declared with the keyword `__declare`. It accepts two arguments by default.

   __declare __* (I I)
   __declare __if (I I I)
   __declare __setq (L I)

   Unfortunately the use of these static type declarations is limited due to the existence of macros. To allow the type
   system to understand macros such as `defun`, `__declare` can be passed a duck-lisp script. When `__declare` is passed
   three or more arguments, the remaining arguments are interpreted as the body of the script. The script should parse
   and analyze the arguments in order to declare additional identifiers used by arguments or by forms that occur in the
   same declaration scope. The duck-lisp scripts that occur in the body of a `__declare` form are run at
   inference-time. This is a separate instance of the duck-lisp compiler and VM than is used in the rest of the
   language. This instance is used solely in the inferrer.

   The inference-time instance of duck-lisp is defined with three additional functions:

   (__infer-and-get-next-argument)::Any -- C callback -- Switches to the next argument and runs inference on the current
   argument. Returns the resulting AST.
   (__declare-identifier name::Symbol type::List)::Nil -- C callback -- Declares the provided symbol `name` as an
   identifier in the current declaration scope with a type specified by `type`.
   (__declaration-scope &rest body::Any)::Any -- Generator -- Create a new declaration scope. Identifiers declared in
   the body using `__declare-identifier` are automatically deleted when the scope is exited.

   Examples:

   ;; `var' itself makes a declaration, so it requires a script. This declaration will persist until the end of the
   ;; scope `var' was used in.
   (__declare var (L I)
              (__declare-identifier (__infer-and-get-next-argument)
                                    (__quote L)))

   ;; `defun' makes two declarations. One of them is scoped to the body while the other persists until the end of the
   ;; scope `defun' was used in.
   (__declare defun (L L L &rest 1 I)
              ;; Infer and get the first three arguments in order.
              (__var name (__infer-and-get-next-argument))
              (__var parameters (__infer-and-get-next-argument))
              (__var type (__infer-and-get-next-argument))
              ;; `defun' defines `self' in order to allow recursion. Scope it to just the body form by running
              ;; `__infer-and-get-next-argument' in the `__declaration-scope' form.
              (__declaration-scope
               (__declare-identifier (__quote self) type)
               ;; This will infer all forms in the body.
               (__var body (__infer-and-get-next-argument)))
              ;; Perform the main function declaration.
              (__declare-identifier name type))

   (__declare let ((&rest 1 (L I)) &rest 1 I)
              ;; The bindings of the `let' statement is the first argument. It will be inferred using the type above.
              (__var bindings (__infer-and-get-next-argument))
              ;; Create a new scope. All declarations in the scope will be deleted when the scope exits.
              (__declaration-scope
               (dolist binding bindings
                       ;; Declare each new identifier in the `let' as a variable.
                       (__declare-identifier (first binding) (__quote L)))
               ;; Infer body in scope.
               (__infer-and-get-next-argument)))

   This system cannot recognize some macros due to the simplicity of the parsing functions used in inference-time
   scripts. It can correctly infer a complicated form like `let`, but is unable to infer `let*`. */


#ifdef USE_PARENTHESIS_INFERENCE

typedef enum {
	inferrerTypeSymbol_L,
	inferrerTypeSymbol_I
} inferrerTypeSymbol_t;

typedef enum {
	inferrerTypeSignature_type_expression,
	inferrerTypeSignature_type_symbol
} inferrerTypeSignature_type_t;

typedef struct inferrerTypeSignature_s {
	inferrerTypeSignature_type_t type;
	union {
		struct {
			/* '(L L I) or '(L L &rest 2 I) or '((&rest 1 (L I)) &rest 1 I) &c. */
			struct inferrerTypeSignature_s *positionalSignatures;
			dl_size_t positionalSignatures_length;
			struct inferrerTypeSignature_s *restSignature;  /* Contains a max of one element */
			dl_size_t defaultRestLength;
			dl_bool_t variadic;
		} expression;
		/* 'I or 'L */
		inferrerTypeSymbol_t symbol;
	} value;
} inferrerTypeSignature_t;


typedef struct {
	dl_uint8_t *bytecode;
	dl_size_t bytecode_length;
	inferrerTypeSignature_t type;
} inferrerType_t;

typedef struct {
	dl_trie_t identifiersTrie;  /* A:int-->types[A],bytecode[A] */
	dl_array_t types;  /* dl_array_t:inferrerType_t */
} inferrerScope_t;

typedef struct {
	dl_memoryAllocation_t *memoryAllocation;
	duckLisp_t duckLisp;
	duckVM_t duckVM;
	dl_array_t *errors;  /* dl_array_t:duckLisp_error_t */
	const char *fileName;
	dl_size_t fileName_length;
	dl_array_t scopeStack;  /* dl_array_t:inferrerScope_t */
} inferrerState_t;

typedef struct {
	inferrerState_t *state;
	char *fileName;
	dl_size_t fileName_length;
	inferrerType_t type;
	dl_ptrdiff_t *type_index;
	duckLisp_ast_expression_t *expression;
	dl_ptrdiff_t *expression_index;
	dl_array_t *newExpression;
	dl_size_t *newLength;
	dl_bool_t parenthesized;
	dl_bool_t infer;
} vmContext_t;


static dl_error_t duckLisp_error_pushInference(inferrerState_t *inferrerState,
                                               const char *message,
                                               const dl_size_t message_length);


static void inferrerTypeSignature_init(inferrerTypeSignature_t *inferrerTypeSignature) {
	inferrerTypeSignature->type = inferrerTypeSignature_type_symbol;
	inferrerTypeSignature->value.symbol = inferrerTypeSymbol_L;
}

static dl_error_t inferrerTypeSignature_quit(inferrerTypeSignature_t *inferrerTypeSignature,
                                             dl_memoryAllocation_t *memoryAllocation) {
	dl_error_t e = dl_error_ok;
	if (inferrerTypeSignature == dl_null) {
		/* OK */
		return e;
	}
	if (inferrerTypeSignature->type == inferrerTypeSignature_type_symbol) {
		/* OK */
		return e;
	}

	if (inferrerTypeSignature->value.expression.variadic) {
		e = inferrerTypeSignature_quit(inferrerTypeSignature->value.expression.restSignature, memoryAllocation);
	}
	inferrerTypeSignature->value.expression.variadic = dl_false;
	inferrerTypeSignature->value.expression.defaultRestLength = 0;

	DL_DOTIMES(i, inferrerTypeSignature->value.expression.positionalSignatures_length) {
		e = inferrerTypeSignature_quit(inferrerTypeSignature->value.expression.positionalSignatures + i,
		                               memoryAllocation);
		if (e) break;
	}

	return e;
}

void inferrerTypeSignature_print(inferrerTypeSignature_t inferrerTypeSignature) {
	if (inferrerTypeSignature.type == inferrerTypeSignature_type_symbol) {
		if (inferrerTypeSignature.value.symbol == inferrerTypeSymbol_L) {
			printf("L");
		}
		else {
			printf("I");
		}
	}
	else {
		printf("(");
		dl_bool_t first = dl_true;
		DL_DOTIMES(j, inferrerTypeSignature.value.expression.positionalSignatures_length) {
			if (first) {
				first = dl_false;
			}
			else {
				printf(" ");
			}
			inferrerTypeSignature_print(inferrerTypeSignature.value.expression.positionalSignatures[j]);
		}
		if (inferrerTypeSignature.value.expression.variadic) {
			if (inferrerTypeSignature.value.expression.positionalSignatures_length > 0) putchar(' ');
			printf("&rest %zu ", inferrerTypeSignature.value.expression.defaultRestLength);
			inferrerTypeSignature_print(*inferrerTypeSignature.value.expression.restSignature);
		}
		printf(")");
	}
}

static dl_error_t inferrerTypeSignature_fromAst(inferrerState_t *state,
                                                inferrerTypeSignature_t *inferrerTypeSignature,
                                                duckLisp_ast_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	switch (compoundExpression.type) {
	case duckLisp_ast_type_identifier:
		/* Fall through */
	case duckLisp_ast_type_callback: {
		dl_bool_t result;
		duckLisp_ast_identifier_t identifier = compoundExpression.value.identifier;
		(void) dl_string_compare(&result, DL_STR("L"), identifier.value, identifier.value_length);
		if (result) {
			inferrerTypeSignature->value.symbol = inferrerTypeSymbol_L;
		}
		else {
			(void) dl_string_compare(&result, DL_STR("I"), identifier.value, identifier.value_length);
			if (result) {
				inferrerTypeSignature->value.symbol = inferrerTypeSymbol_I;
			}
			else {
				e = dl_error_invalidValue;
				eError = duckLisp_error_pushInference(state, DL_STR("Invalid type symbol"));
				if (eError) e = eError;
				goto cleanup;
			}
		}
		inferrerTypeSignature->type = inferrerTypeSignature_type_symbol;
		break;
	}
	case duckLisp_ast_type_expression: {
		duckLisp_ast_expression_t expression = compoundExpression.value.expression;
		inferrerTypeSignature_t subInferrerTypeSignature;
		(void) inferrerTypeSignature_init(&subInferrerTypeSignature);
		dl_array_t subInferrerTypeSignaturesArray;
		(void) dl_array_init(&subInferrerTypeSignaturesArray,
		                     state->memoryAllocation,
		                     sizeof(inferrerTypeSignature_t),
		                     dl_array_strategy_double);
		dl_bool_t variadic = dl_false;
		dl_ptrdiff_t variadic_arg = 0;
		DL_DOTIMES(j, expression.compoundExpressions_length) {
			if (variadic_arg == 1) {
				variadic_arg++;
				if (expression.compoundExpressions[j].type != duckLisp_ast_type_int) {
					e = dl_error_invalidValue;
					eError = duckLisp_error_pushInference(state, DL_STR("Default argument length is not an integer"));
					if (eError) e = eError;
					goto expressionCleanup;
				}
				inferrerTypeSignature->value.expression.defaultRestLength = (expression.compoundExpressions[j]
				                                                             .value.integer.value);
				continue;
			}
			if (variadic_arg == 2) {
				variadic_arg++;
				e = inferrerTypeSignature_fromAst(state, &subInferrerTypeSignature, expression.compoundExpressions[j]);
				if (e) goto expressionCleanup;
				e = DL_MALLOC(state->memoryAllocation,
				              &inferrerTypeSignature->value.expression.restSignature,
				              1,
				              inferrerTypeSignature_t);
				if (e) goto expressionCleanup;
				*inferrerTypeSignature->value.expression.restSignature = subInferrerTypeSignature;
				continue;
			}
			if ((expression.compoundExpressions[j].type == duckLisp_ast_type_identifier)
			    || (expression.compoundExpressions[j].type == duckLisp_ast_type_callback)) {
				dl_bool_t result;
				duckLisp_ast_identifier_t identifier = expression.compoundExpressions[j].value.identifier;
				(void) dl_string_compare(&result, DL_STR("&rest"), identifier.value, identifier.value_length);
				if (result) {
					if (variadic) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state, DL_STR("Duplicate \"&rest\" in type specifier"));
						if (eError) e = eError;
						goto expressionCleanup;
					}
					if ((dl_size_t) j != (expression.compoundExpressions_length - 3)) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state,
						                                      DL_STR("Exactly two forms should follow \"&rest\"."));
						if (eError) e = eError;
						goto expressionCleanup;
					}
					variadic = dl_true;
					variadic_arg++;
					continue;
				}
			}
			e = inferrerTypeSignature_fromAst(state, &subInferrerTypeSignature, expression.compoundExpressions[j]);
			if (e) goto expressionCleanup;
			e = dl_array_pushElement(&subInferrerTypeSignaturesArray, &subInferrerTypeSignature);
			if (e) goto expressionCleanup;
		}

		inferrerTypeSignature->type = inferrerTypeSignature_type_expression;
		inferrerTypeSignature->value.expression.positionalSignatures = subInferrerTypeSignaturesArray.elements;
		(inferrerTypeSignature
		 ->value.expression.positionalSignatures_length) = subInferrerTypeSignaturesArray.elements_length;
		inferrerTypeSignature->value.expression.variadic = variadic;
		expressionCleanup:
		if (e) {
			eError = dl_array_quit(&subInferrerTypeSignaturesArray);
			if (eError) e = eError;
			eError = inferrerTypeSignature_quit(&subInferrerTypeSignature, state->memoryAllocation);
			if (eError) e = eError;
		}
		break;
	}
	case duckLisp_ast_type_bool:
		/* Fall through */
	case duckLisp_ast_type_int:
		/* Fall through */
	case duckLisp_ast_type_float:
		/* Fall through */
	case duckLisp_ast_type_string:
		e = dl_error_invalidValue;
		// Log error.
		goto cleanup;
	default:
		e = dl_error_cantHappen;
		// Log error.
		goto cleanup;
	}

 cleanup:
	if (e) {
		eError = inferrerTypeSignature_quit(inferrerTypeSignature, state->memoryAllocation);
		if (eError) e = eError;
	}
	return e;
}

static void inferrerType_init(inferrerType_t *inferrerType) {
	(void) inferrerTypeSignature_init(&inferrerType->type);
	inferrerType->bytecode = dl_null;
	inferrerType->bytecode_length = 0;
}

static dl_error_t inferrerType_quit(inferrerType_t *inferrerType, dl_memoryAllocation_t *memoryAllocation) {
	dl_error_t e = inferrerTypeSignature_quit(&inferrerType->type, memoryAllocation);
	e = DL_FREE(memoryAllocation, &inferrerType->bytecode);
	inferrerType->bytecode_length = 0;
	return e;
}

static void inferrerScope_init(inferrerScope_t *inferrerScope, dl_memoryAllocation_t *ma) {
	(void) dl_trie_init(&inferrerScope->identifiersTrie, ma, -1);
	(void) dl_array_init(&inferrerScope->types, ma, sizeof(inferrerType_t), dl_array_strategy_double);
}

static dl_error_t inferrerScope_quit(inferrerScope_t *inferrerScope, dl_memoryAllocation_t *ma) {
	dl_error_t e = dl_trie_quit(&inferrerScope->identifiersTrie);
	DL_DOTIMES(i, inferrerScope->types.elements_length) {
		inferrerType_t it;
		e = dl_array_popElement(&inferrerScope->types, &it);
		if (e) break;
		e = inferrerType_quit(&it, ma);
	}
	e = dl_array_quit(&inferrerScope->types);
	return e;
}



static dl_error_t inferrerState_init(inferrerState_t *inferrerState,
                                     dl_memoryAllocation_t *memoryAllocation,
                                     dl_size_t maxComptimeVmObjects,
                                     dl_array_t *errors,
                                     const char *fileName,
                                     const dl_size_t fileName_length) {
	dl_error_t e = dl_error_ok;

	dl_array_t *inferrerContext = dl_null;
	e = DL_MALLOC(inferrerState->memoryAllocation, &inferrerContext, 1, vmContext_t);
	if (e) goto cleanup;
	(void) dl_array_init(inferrerContext,
	                     inferrerState->memoryAllocation,
	                     sizeof(vmContext_t),
	                     dl_array_strategy_double);
	inferrerState->memoryAllocation = memoryAllocation;
	e = duckLisp_init(&inferrerState->duckLisp, memoryAllocation, maxComptimeVmObjects);
	if (e) goto cleanup;
	e = duckVM_init(&inferrerState->duckVM, memoryAllocation, maxComptimeVmObjects);
	if (e) goto cleanup;
	inferrerState->duckVM.inferrerContext = inferrerContext;
	inferrerState->errors = errors;
	inferrerState->fileName = fileName;
	inferrerState->fileName_length = fileName_length;
	(void) dl_array_init(&inferrerState->scopeStack,
	                     memoryAllocation,
	                     sizeof(inferrerScope_t),
	                     dl_array_strategy_double);

 cleanup: return e;
}

static dl_error_t inferrerState_quit(inferrerState_t *inferrerState) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	DL_DOTIMES(i, inferrerState->scopeStack.elements_length) {
		inferrerScope_t is;
		e = dl_array_popElement(&inferrerState->scopeStack, &is);
		if (e) break;
		e = inferrerScope_quit(&is, inferrerState->memoryAllocation);
	}
	eError = dl_array_quit(&inferrerState->scopeStack);
	if (eError) e = eError;
	inferrerState->fileName_length = 0;
	inferrerState->fileName = dl_null;
	inferrerState->errors = dl_null;
	e = dl_array_quit((dl_array_t *) inferrerState->duckVM.inferrerContext);
	eError = DL_FREE(inferrerState->memoryAllocation, &inferrerState->duckVM.inferrerContext);
	if (eError) e = eError;
	(void) duckVM_quit(&inferrerState->duckVM);
	(void) duckLisp_quit(&inferrerState->duckLisp);
	inferrerState->memoryAllocation = dl_null;
	return e;
}


static dl_error_t pushDeclarationScope(inferrerState_t *state, inferrerScope_t *scope) {
	return dl_array_pushElement(&state->scopeStack, scope);
}

static dl_error_t popDeclarationScope(inferrerState_t *state, inferrerScope_t *scope) {
	return dl_array_popElement(&state->scopeStack, scope);
}

static dl_error_t findDeclaration(inferrerState_t *state,
                                  dl_bool_t *found,
                                  inferrerType_t *type,
                                  duckLisp_ast_identifier_t identifier) {
	dl_error_t e = dl_error_ok;

	*found = dl_false;

	dl_array_t scopeStack = state->scopeStack;
	for (dl_ptrdiff_t i = state->scopeStack.elements_length - 1; i >= 0; --i) {
		inferrerScope_t scope;
		e = dl_array_get(&scopeStack, &scope, i);
		if (e) goto cleanup;
		dl_ptrdiff_t index = -1;
		(void) dl_trie_find(scope.identifiersTrie, &index, identifier.value, identifier.value_length);
		if (index != -1) {
			e = dl_array_get(&scope.types, type, index);
			if (e) goto cleanup;
			*found = dl_true;
			break;
		}
	}

 cleanup: return e;
}

static dl_error_t addDeclaration(inferrerState_t *state,
                                 const char *name,
                                 const dl_size_t name_length,
                                 duckLisp_ast_compoundExpression_t typeAst,
                                 dl_uint8_t *bytecode,
                                 dl_size_t bytecode_length) {
	dl_error_t e = dl_error_ok;

	inferrerType_t type;
	inferrerScope_t scope;
	(void) inferrerType_init(&type);

	e = popDeclarationScope(state, &scope);
	if (e) goto cleanup;

	type.bytecode = bytecode;
	type.bytecode_length = bytecode_length;
	e = inferrerTypeSignature_fromAst(state, &type.type, typeAst);
	if (e) goto cleanup;
	e = dl_trie_insert(&scope.identifiersTrie, name, name_length, scope.types.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElement(&scope.types, &type);
	if (e) goto cleanup;

	e = pushDeclarationScope(state, &scope);
	if (e) goto cleanup;

 cleanup: return e;
}

static dl_error_t compileAndAddDeclaration(inferrerState_t *state,
                                           const char *fileName,
                                           const dl_size_t fileName_length,
                                           const char *name,
                                           const dl_size_t name_length,
                                           const duckLisp_ast_compoundExpression_t typeAst,
                                           const duckLisp_ast_compoundExpression_t scriptAst) {
	dl_error_t e = dl_error_ok;
	(void) fileName;
	(void) fileName_length;

	dl_uint8_t *bytecode = dl_null;
	dl_size_t bytecode_length = 0;

	if (scriptAst.type != duckLisp_ast_type_none) {
		dl_array_t bytecodeArray;
		duckLisp_compileState_t compileState;

		(void) duckLisp_compileState_init(&state->duckLisp, &compileState);
		e = duckLisp_compileAST(&state->duckLisp, &compileState, &bytecodeArray, scriptAst);
		if (e) goto cleanup;
		e = duckLisp_compileState_quit(&state->duckLisp, &compileState);
		if (e) goto cleanup;

		bytecode = ((unsigned char*) bytecodeArray.elements);
		bytecode_length = bytecodeArray.elements_length;
	}
	e = addDeclaration(state,
	                   name,
	                   name_length,
	                   typeAst,
	                   bytecode,
	                   bytecode_length);
	if (e) goto cleanup;

 cleanup: return e;
}



static dl_error_t duckLisp_error_pushInference(inferrerState_t *inferrerState,
                                               const char *message,
                                               const dl_size_t message_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_error_t error;

	const char inferrerPrefix[] = "Inference error: ";
	const dl_size_t inferrerPrefix_length = sizeof(inferrerPrefix)/sizeof(*inferrerPrefix) - 1;

	e = dl_malloc(inferrerState->memoryAllocation,
	              (void **) &error.message,
	              (inferrerPrefix_length + message_length) * sizeof(char));
	if (e) goto cleanup;
	e = dl_memcopy((void *) error.message, (void *) inferrerPrefix, inferrerPrefix_length * sizeof(char));
	if (e) goto cleanup;
	e = dl_memcopy((void *) (error.message + inferrerPrefix_length), (void *) message, message_length * sizeof(char));
	if (e) goto cleanup;

	e = DL_MALLOC(inferrerState->memoryAllocation, &error.fileName, inferrerState->fileName_length, char);
	if (e) goto cleanup;
	e = dl_memcopy((void *) error.fileName,
	               (void *) inferrerState->fileName,
	               inferrerState->fileName_length * sizeof(char));
	if (e) goto cleanup;

	error.message_length = inferrerPrefix_length + message_length;
	error.fileName_length = inferrerState->fileName_length;
	error.start_index = -1;
	error.end_index = -1;

	e = dl_array_pushElement(inferrerState->errors, &error);
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t inferArgument(inferrerState_t *state,
                                const char *fileName,
                                const dl_size_t fileName_length,
                                duckLisp_ast_expression_t *expression,
                                dl_ptrdiff_t *index,
                                dl_bool_t parenthesized,
                                dl_bool_t infer);
static dl_error_t infer_compoundExpression(inferrerState_t *state,
                                           const char *fileName,
                                           const dl_size_t fileName_length,
                                           duckLisp_ast_compoundExpression_t *compoundExpression,
                                           dl_bool_t infer);

static dl_error_t infer_callback(inferrerState_t *state,
                                 duckLisp_ast_compoundExpression_t *compoundExpression,
                                 dl_bool_t infer) {
	(void) state;
	(void) infer;
	compoundExpression->type = duckLisp_ast_type_identifier;
	return dl_error_ok;
}


			/* expr index
			   type index
			*/

static dl_error_t runVm(inferrerState_t *state,
                        inferrerType_t type,
                        dl_ptrdiff_t *type_index,
                        duckLisp_ast_expression_t *expression,
                        dl_ptrdiff_t *expression_index,
                        dl_array_t *newExpression,
                        dl_size_t *newLength,
                        dl_bool_t parenthesized,
                        dl_bool_t infer) {
	dl_error_t e = dl_error_ok;

	vmContext_t context;
	context.state = state;
	context.type = type;
	context.type_index = type_index;
	context.expression = expression;
	context.expression_index = expression_index;
	context.newExpression = newExpression;
	context.newLength = newLength;
	context.parenthesized = parenthesized;
	context.infer = infer;

	e = dl_array_pushElement((dl_array_t *) state->duckVM.inferrerContext, &context);
	if (e) goto cleanup;
	/* puts(duckLisp_disassemble(state->memoryAllocation, type.bytecode, type.bytecode_length)); */
	e = duckVM_execute(&state->duckVM, dl_null, type.bytecode, type.bytecode_length);
	if (e) goto cleanup;
	e = dl_array_popElement((dl_array_t *) state->duckVM.inferrerContext, dl_null);
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t inferIncrementally(inferrerState_t *state,
                                     const char *fileName,
                                     const dl_size_t fileName_length,
                                     inferrerType_t type,
                                     dl_ptrdiff_t *type_index,
                                     duckLisp_ast_expression_t *expression,
                                     dl_ptrdiff_t *expression_index,
                                     dl_array_t *newExpression,  /* dl_array_t:duckLisp_ast_compoundExpression_t */
                                     dl_size_t *newExpression_length,
                                     dl_bool_t parenthesized,
                                     dl_bool_t infer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	if ((dl_size_t) *type_index <= type.type.value.expression.positionalSignatures_length) {
		/* A coroutine could be nice here, but this works too. */
		if ((dl_size_t) *type_index < type.type.value.expression.positionalSignatures_length) {
			if ((dl_size_t) *expression_index >= expression->compoundExpressions_length) {
				e = dl_error_invalidValue;
				(eError
				 = duckLisp_error_pushInference(state,
				                                DL_STR("Too few arguments for declared identifier.")));
				if (eError) e = eError;
				goto cleanup;
			}
			inferrerTypeSignature_t argSignature = (type.type.value.expression.positionalSignatures
			                                        [*type_index]);
			if (argSignature.type == inferrerTypeSignature_type_symbol) {
				dl_ptrdiff_t lastIndex = *expression_index;
				e = inferArgument(state,
				                  fileName,
				                  fileName_length,
				                  expression,
				                  expression_index,
				                  dl_false,
				                  (argSignature.value.symbol == inferrerTypeSymbol_I) ? infer : dl_false);
				if (e) goto cleanup;
				if (!parenthesized) {
					e = dl_array_pushElement(newExpression, &expression->compoundExpressions[lastIndex]);
					if (e) goto cleanup;
					(*newExpression_length)++;
				}
			}
			else {
				(eError
				 = duckLisp_error_pushInference(state,
				                                DL_STR("Nested expression types are not yet supported.")));
				if (eError) e = eError;
			}
		}
		else {
			// This does not need to be incremental, but it does need to be treated as the parg, and pargs
			// are inferred incrementally.
			if (type.type.value.expression.variadic) {
				inferrerTypeSignature_t argSignature = *type.type.value.expression.restSignature;
				for (dl_ptrdiff_t l = 0;
				     (parenthesized
				      ? ((dl_size_t) *expression_index < expression->compoundExpressions_length)
				      : ((dl_size_t) l < type.type.value.expression.defaultRestLength));
				     l++) {
					if (argSignature.type == inferrerTypeSignature_type_symbol) {
						dl_ptrdiff_t lastIndex = *expression_index;
						e = inferArgument(state,
						                  fileName,
						                  fileName_length,
						                  expression,
						                  expression_index,
						                  dl_false,
						                  ((argSignature.value.symbol == inferrerTypeSymbol_I)
						                   ? infer
						                   : dl_false));
						if (e) goto cleanup;
						if (!parenthesized) {
							e = dl_array_pushElement(newExpression,
							                         &expression->compoundExpressions[lastIndex]);
							if (e) goto cleanup;
							(*newExpression_length)++;
						}
					}
					else {
						(eError
						 = duckLisp_error_pushInference(state,
						                                DL_STR("Nested expression types are not yet supported.")));
						if (eError) e = eError;
					}
				}
			}
		}
		(*type_index)++;
	}

 cleanup: return e;
}

/* Infer a single argument from the tokens/forms. */
static dl_error_t inferArgument(inferrerState_t *state,
                                const char *fileName,
                                const dl_size_t fileName_length,
                                duckLisp_ast_expression_t *expression,
                                dl_ptrdiff_t *index,
                                dl_bool_t parenthesized,
                                dl_bool_t infer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t localIndex = *index;
	dl_ptrdiff_t startLocalIndex = localIndex;
	dl_size_t newLength = 0;
	if ((dl_size_t) localIndex >= expression->compoundExpressions_length) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushInference(state, DL_STR("To few arguments for declared identifier."));
		if (eError) e = eError;
		goto cleanup;
	}
	duckLisp_ast_compoundExpression_t *compoundExpression = expression->compoundExpressions + localIndex;
	localIndex++;
	if (compoundExpression->type == duckLisp_ast_type_identifier) {
		inferrerType_t type;
		dl_bool_t found = dl_false;
		e = findDeclaration(state, &found, &type, compoundExpression->value.identifier);
		if (e) goto cleanup;

		if (!infer) {
			printf("\x1B[34m");
		}
		else if (found && infer) {
			printf("\x1B[32m");
		}
		else {
			printf("\x1B[31m");
		}
		DL_DOTIMES(i, compoundExpression->value.identifier.value_length) {
			putchar(compoundExpression->value.identifier.value[i]);
		}

		if (found && infer) {
			/* Declared */
			if (type.bytecode_length > 0) {
				/* Execute bytecode */
			}

			printf("::");
			inferrerTypeSignature_print(type.type);
			puts("\x1B[0m");

			/* lol */
			if (type.type.type == inferrerTypeSignature_type_expression) {
				dl_array_t newExpression;
				(void) dl_array_init(&newExpression,
				                     state->memoryAllocation,
				                     sizeof(duckLisp_ast_compoundExpression_t),
				                     dl_array_strategy_double);
				if (!parenthesized) {
					e = dl_array_pushElement(&newExpression, compoundExpression);
					if (e) goto expressionCleanup;
				}

				dl_ptrdiff_t type_index = 0;

				/* This part may do some inference. */
				if (type.bytecode_length > 0) {
					e = runVm(state,
					          type,
					          &type_index,
					          expression,
					          &localIndex,
					          &newExpression,
					          &newLength,
					          parenthesized,
					          infer);
					if (e) goto cleanup;
				}

				/* This part does whatever inference the script didn't do. */
				while ((dl_size_t) type_index <= type.type.value.expression.positionalSignatures_length) {
					e = inferIncrementally(state,
					                       fileName,
					                       fileName_length,
					                       type,
					                       &type_index,
					                       expression,
					                       &localIndex,
					                       &newExpression,
					                       &newLength,
					                       parenthesized,
					                       infer);
					if (e) goto expressionCleanup;
				}

			expressionCleanup:
				if (e) {
					eError = dl_array_quit(&newExpression);
					if (eError) e = eError;
				}
				else if (parenthesized) {
					if ((dl_size_t) localIndex > expression->compoundExpressions_length) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state, DL_STR("Too few arguments for identifier."));
						if (eError) e = eError;
					}
					else if ((dl_size_t) localIndex < expression->compoundExpressions_length) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state, DL_STR("Too many arguments for identifier."));
						if (eError) e = eError;
					}
				}
				else {
					duckLisp_ast_compoundExpression_t ce;
					ce.type = duckLisp_ast_type_expression;
					ce.value.expression.compoundExpressions_length = newExpression.elements_length;
					ce.value.expression.compoundExpressions = newExpression.elements;
					expression->compoundExpressions[startLocalIndex] = ce;
					DL_DOTIMES(n, expression->compoundExpressions_length - startLocalIndex - newLength - 1) {
						(expression->compoundExpressions[startLocalIndex + n + 1]
						 = expression->compoundExpressions[startLocalIndex + newLength + n + 1]);
					}
					expression->compoundExpressions_length -= newLength;
					localIndex = startLocalIndex + 1;
				}
				if (e) goto cleanup;
			}
			else if (parenthesized) {
				e = dl_error_invalidValue;
				eError = duckLisp_error_pushInference(state, DL_STR("Cannot call an identifier."));
				if (eError) e = eError;
				goto cleanup;
			}
			else {
				/* Do nothing */
			}
		}
		else {
			if (found) {
				/* Inference disabled */
				printf("::");
				inferrerTypeSignature_print(type.type);
				puts("\x1B[0m");
				e = infer_compoundExpression(state, fileName, fileName_length, compoundExpression, infer);
				if (e) goto cleanup;
			}
			else {
				/* Undeclared */
				puts("::Undeclared\x1B[0m");
				e = infer_compoundExpression(state, fileName, fileName_length, compoundExpression, infer);
				if (e) goto cleanup;
			}
		}
	}
	else {
		/* Anything not an identifier */
		e = infer_compoundExpression(state, fileName, fileName_length, compoundExpression, infer);
		if (e) goto cleanup;
	}
	*index = localIndex;

 cleanup: return e;
}

/* Infer a single argument from the tokens/forms. */
static dl_error_t inferArguments(inferrerState_t *state,
                                 const char *fileName,
                                 const dl_size_t fileName_length,
                                 duckLisp_ast_expression_t *expression,
                                 dl_ptrdiff_t index,
                                 dl_bool_t infer) {
	dl_error_t e = dl_error_ok;

	while ((dl_size_t) index < expression->compoundExpressions_length) {
		/* Run inference on the current identifier. */
		dl_ptrdiff_t startIndex = index;
		/* The function needs the whole expression. It is given the pointer to the current index. */
		e = inferArgument(state, fileName, fileName_length, expression, &index, dl_false, infer);
		if (e) goto cleanup;

		/* `__declare` interpreter */
		duckLisp_ast_compoundExpression_t ce = expression->compoundExpressions[startIndex];
		if ((ce.type == duckLisp_ast_type_expression)
		    && ((ce.value.expression.compoundExpressions_length == 3)
		        || (ce.value.expression.compoundExpressions_length == 4))
		    && (ce.value.expression.compoundExpressions[0].type == duckLisp_ast_type_identifier)
		    && (ce.value.expression.compoundExpressions[1].type == duckLisp_ast_type_identifier)) {
			dl_bool_t result;
			duckLisp_ast_identifier_t keyword = ce.value.expression.compoundExpressions[0].value.identifier;
			duckLisp_ast_identifier_t identifier = ce.value.expression.compoundExpressions[1].value.identifier;
			duckLisp_ast_compoundExpression_t typeAst = ce.value.expression.compoundExpressions[2];
			(void) dl_string_compare(&result, DL_STR("__declare"), keyword.value, keyword.value_length);
			if (result) {
				duckLisp_ast_compoundExpression_t scriptAst;
				(void) duckLisp_ast_compoundExpression_init(&scriptAst);
				if (ce.value.expression.compoundExpressions_length == 4) {
					scriptAst = ce.value.expression.compoundExpressions[3];
				}
				e = compileAndAddDeclaration(state,
				                             fileName,
				                             fileName_length,
				                             identifier.value,
				                             identifier.value_length,
				                             typeAst,
				                             scriptAst);
				if (e) goto cleanup;
			}
		}
	}

 cleanup: return e;
}

static dl_error_t infer_expression(inferrerState_t *state,
                                   const char *fileName,
                                   const dl_size_t fileName_length,
                                   duckLisp_ast_compoundExpression_t *compoundExpression,
                                   dl_bool_t infer) {
	dl_error_t e = dl_error_ok;

	if (!infer) {
		/* OK */
		goto cleanup;
	}

	duckLisp_ast_expression_t *expression = &compoundExpression->value.expression;

	/* Expression inference:
	     Is an identifier? — Done
	       Is declared? — Done
	         Has declarator? — Done
	           Run declarator.

	   TODO: declarator scripts
	         nested types

	   Two functions can only be used if the right data structure exists. Or maybe I can copy the tree instead of
	   modifying it?

	   declare m (L I)
	   m a 1  —  Infer, infer second arg
	   (m a 1)  —  Check arity, infer second arg
	   (#m a 1)  —  Untyped, infer both args
	   #(m a 1)  —  Untyped, don't infer any args
	*/

	if (expression->compoundExpressions_length == 0) {
		/* OK. Is a Nil. */
		goto cleanup;
	}

	/* This pointer is weak. */
	duckLisp_ast_compoundExpression_t *head = expression->compoundExpressions;

	if (compoundExpression->type == duckLisp_ast_type_literalExpression) {
		/* Don't type-check. */
		e = infer_callback(state, head, dl_false);
		if (e) goto cleanup;
		/* Run argument inference */

		e = inferArguments(state, fileName, fileName_length, expression, 1, dl_false);
		if (e) goto cleanup;

		compoundExpression->type = duckLisp_ast_type_expression;
	}
	else if (head->type == duckLisp_ast_type_callback) {
		/* Don't type-check. */
		e = infer_callback(state, head, dl_false);
		if (e) goto cleanup;
		/* Run argument inference */

		e = inferArguments(state, fileName, fileName_length, expression, 1, infer);
		if (e) goto cleanup;
	}
	else if (head->type == duckLisp_ast_type_identifier) {
		/* This is a function call. It needs to be type checked. */
		duckLisp_ast_identifier_t expressionIdentifier = head->value.identifier;
		inferrerType_t type;

		dl_bool_t found = dl_false;
		e = findDeclaration(state, &found, &type, expressionIdentifier);
		if (e) goto cleanup;

		if (found) {
			/* Declared. */
			dl_ptrdiff_t expression_index = 0;
			e = inferArgument(state, fileName, fileName_length, expression, &expression_index, dl_true, infer);
			if (e) goto cleanup;
		}
		else {
			/* Undeclared. */
			/* Argument inference stuff goes here. */
			e = inferArguments(state, fileName, fileName_length, expression, 1, infer);
			if (e) goto cleanup;
		}
	}
	else {
		/* This is a scope. */
		inferrerScope_t scope;
		(void) inferrerScope_init(&scope, state->memoryAllocation);
		e = pushDeclarationScope(state, &scope);
		if (e) goto cleanup;

		e = inferArguments(state, fileName, fileName_length, expression, 0, infer);
		if (e) goto cleanup;

		e = popDeclarationScope(state, dl_null);
		if (e) goto cleanup;
	}

 cleanup: return e;
}

static dl_error_t infer_compoundExpression(inferrerState_t *state,
                                           const char *fileName,
                                           const dl_size_t fileName_length,
                                           duckLisp_ast_compoundExpression_t *compoundExpression,
                                           dl_bool_t infer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	/* Due to how the parser works, which is to only read one form, we can only infer for forms inside an expression. If
	   the outermost type is not an expression, then we can't infer anything. Because of this, all inference happens
	   inside the inference function for expressions. */

	switch (compoundExpression->type) {
	case duckLisp_ast_type_none:
		eError = duckLisp_error_pushInference(state, DL_STR("Node type is \"None\"."));
		e = eError ? eError : dl_error_cantHappen;
		break;
	case duckLisp_ast_type_bool:
		/* Fall through */
	case duckLisp_ast_type_int:
		/* Fall through */
	case duckLisp_ast_type_float:
		/* Fall through */
	case duckLisp_ast_type_string:
		/* Fall through */
	case duckLisp_ast_type_identifier:
		break;
	case duckLisp_ast_type_callback:
		e = infer_callback(state, compoundExpression, infer);
		break;
	case duckLisp_ast_type_literalExpression:
		/* Fall through */
	case duckLisp_ast_type_expression:
		e = infer_expression(state, fileName, fileName_length, compoundExpression, infer);
		break;
	default:
		eError = duckLisp_error_pushInference(state, DL_STR("Illegal type."));
		e = eError ? eError : dl_error_cantHappen;
	}
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t callback_declareIdentifier(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckVM_object_t identifierObject;
	duckVM_object_t typeObject;
	vmContext_t context;
	inferrerState_t *state = dl_null;

	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto cleanup;
	state = context.state;

	e = duckVM_pop(vm, &typeObject);
	if (e) goto cleanup;
	if ((typeObject.type != duckVM_object_type_symbol)
	    && (typeObject.type != duckVM_object_type_list)) {
		e = dl_error_invalidValue;
		(eError
		 = duckVM_error_pushRuntime(vm,
		                            DL_STR("Second argument of `__declare-identifier` should be a type signature.")));
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckVM_pop(vm, &identifierObject);
	if (e) goto cleanup;
	if ((identifierObject.type != duckVM_object_type_symbol)
	    && (identifierObject.type != duckVM_object_type_string)) {
		e = dl_error_invalidValue;
		(eError
		 = duckVM_error_pushRuntime(vm,
		                            DL_STR("First argument of `__declare-identifier` should be an identifier or a string.")));
		if (eError) e = eError;
		goto cleanup;
	}

	duckLisp_ast_compoundExpression_t typeAst;
	e = duckLisp_objectToAST(&state->duckLisp, &typeAst, &typeObject, dl_true);
	if (e) goto cleanup;

	if (identifierObject.type == duckVM_object_type_string) {
		duckVM_string_t identifierString = duckVM_object_getString(identifierObject);
		duckVM_internalString_t identifierInternalString;
		e = duckVM_string_getInternalString(identifierString, &identifierInternalString);
		if (e) goto cleanup;
		e = addDeclaration(state,
		                   (char *) identifierInternalString.value + identifierString.offset,
		                   identifierString.length - identifierString.offset,
		                   typeAst,
		                   dl_null,
		                   0);
	}
	else {
		duckVM_symbol_t identifierSymbol = duckVM_object_getSymbol(identifierObject);
		duckVM_internalString_t identifierInternalString;
		e = duckVM_symbol_getInternalString(identifierSymbol, &identifierInternalString);
		if (e) goto cleanup;
		e = addDeclaration(state,
		                   (char *) identifierInternalString.value,
		                   identifierInternalString.value_length,
		                   typeAst,
		                   dl_null,
		                   0);
	}
	if (e) goto cleanup;

	e = duckVM_pushNil(vm);
	if (e) goto cleanup;

 cleanup: return e;
}

static dl_error_t callback_inferAndGetNextArgument(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;

	vmContext_t context;
	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto preDefinitionCleanup;

	inferrerState_t *state = context.state;
	const char *fileName = context.fileName;
	const dl_size_t fileName_length = context.fileName_length;
	inferrerType_t type = context.type;
	dl_ptrdiff_t *type_index = context.type_index;
	duckLisp_ast_expression_t *expression = context.expression;
	dl_ptrdiff_t *expression_index = context.expression_index;
	dl_array_t *newExpression = context.newExpression;
	dl_size_t *newLength = context.newLength;
	const dl_bool_t parenthesized = context.parenthesized;
	const dl_bool_t infer = context.infer;

	duckLisp_t *duckLisp = &state->duckLisp;

	/* infer */

	e = inferIncrementally(state,
	                       fileName,
	                       fileName_length,
	                       type,
	                       type_index,
	                       expression,
	                       expression_index,
	                       newExpression,
	                       newLength,
	                       parenthesized,
	                       infer);
	if (e) goto postDefinitionCleanup;

	/* AndGetNextArgument */

	duckVM_object_t object;
	e = duckLisp_astToObject(duckLisp, vm, &object, expression->compoundExpressions[*expression_index - 1]);
	if (e) goto postDefinitionCleanup;

	e = duckVM_push(vm, &object);
	if (e) goto postDefinitionCleanup;

 postDefinitionCleanup:
 preDefinitionCleanup: return e;
}

static dl_error_t callback_pushScope(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;

	inferrerScope_t scope;
	vmContext_t context;

	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto cleanup;

	(void) inferrerScope_init(&scope, context.state->memoryAllocation);
	e = pushDeclarationScope(context.state, &scope);
	if (e) goto cleanup;

	e = duckVM_pushNil(vm);
	if (e) goto cleanup;

 cleanup: return e;
}

static dl_error_t callback_popScope(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;

	vmContext_t context;

	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto cleanup;

	e = popDeclarationScope(context.state, dl_null);
	if (e) goto cleanup;

	e = duckVM_pushNil(vm);
	if (e) goto cleanup;

 cleanup: return e;
}

static dl_error_t generator_declarationScope(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	/* `(__declaration-scope ,@body) */
	/* `(
	     (__push-declaration-scope)
	     ,@body
	     (__pop-declaration-scope)) */

	duckLisp_ast_compoundExpression_t pushC;
	pushC.type = duckLisp_ast_type_identifier;
	pushC.value.identifier.value = "\0__push-declaration-scope";
	pushC.value.identifier.value_length = sizeof("\0__push-declaration-scope")/sizeof(char) - 1;
	duckLisp_ast_expression_t pushE;
	pushE.compoundExpressions_length = 1;
	pushE.compoundExpressions = &pushC;
	duckLisp_ast_compoundExpression_t popC;
	popC.type = duckLisp_ast_type_identifier;
	popC.value.identifier.value = "\0__pop-declaration-scope";
	popC.value.identifier.value_length = sizeof("\0__pop-declaration-scope")/sizeof(char) - 1;
	duckLisp_ast_expression_t popE;
	popE.compoundExpressions_length = 1;
	popE.compoundExpressions = &popC;
	duckLisp_ast_expression_t scopeE;
	scopeE.compoundExpressions_length = 0;
	e = DL_MALLOC(duckLisp->memoryAllocation,
	              &scopeE.compoundExpressions,
	              expression->compoundExpressions_length + 1,
	              duckLisp_ast_compoundExpression_t);
	if (e) goto cleanup;
	scopeE.compoundExpressions_length = expression->compoundExpressions_length + 1;

	scopeE.compoundExpressions[0].type = duckLisp_ast_type_expression;
	scopeE.compoundExpressions[0].value.expression = pushE;
	DL_DOTIMES(i, expression->compoundExpressions_length - 1) {
		scopeE.compoundExpressions[i + 1] = expression->compoundExpressions[i + 1];
	}
	scopeE.compoundExpressions[scopeE.compoundExpressions_length - 1].type = duckLisp_ast_type_expression;
	scopeE.compoundExpressions[scopeE.compoundExpressions_length - 1].value.expression = popE;

	e = duckLisp_generator_expression(duckLisp,
	                                  compileState,
	                                  assembly,
	                                  &scopeE);
	if (e) goto cleanup;

 cleanup:
	if (scopeE.compoundExpressions_length > 0) {
		eError = DL_FREE(duckLisp->memoryAllocation, &scopeE.compoundExpressions);
		if (eError) e = eError;
	}

	return e;
}


dl_error_t inferParentheses(dl_memoryAllocation_t *memoryAllocation,
                            const dl_size_t maxComptimeVmObjects,
                            dl_array_t *errors,
                            const char *fileName,
                            const dl_size_t fileName_length,
                            duckLisp_ast_compoundExpression_t *ast) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	inferrerState_t state;
	e = inferrerState_init(&state, memoryAllocation, maxComptimeVmObjects, errors, fileName, fileName_length);
	if (e) goto cleanup;

	struct {
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
	} callbacks[] = {
		{DL_STR("__declare-identifier"),          callback_declareIdentifier},
		{DL_STR("__infer-and-get-next-argument"), callback_inferAndGetNextArgument},
		{DL_STR("\0__push-declaration-scope"),    callback_pushScope},
		{DL_STR("\0__pop-declaration-scope"),     callback_popScope},
	};
	DL_DOTIMES(i, sizeof(callbacks)/sizeof(*callbacks)) {
		e = duckLisp_linkCFunction(&state.duckLisp, callbacks[i].callback, callbacks[i].name, callbacks[i].name_length);
		if (e) goto cleanup;
		dl_ptrdiff_t id = duckLisp_symbol_nameToValue(&state.duckLisp, callbacks[i].name, callbacks[i].name_length);
		e = duckVM_linkCFunction(&state.duckVM, id, callbacks[i].callback);
		if (e) goto cleanup;
	}

	e = duckLisp_addGenerator(&state.duckLisp, generator_declarationScope, DL_STR("__declaration-scope"));
	if (e) goto cleanup;

	{
		inferrerScope_t scope;
		(void) inferrerScope_init(&scope, memoryAllocation);
		e = pushDeclarationScope(&state, &scope);
		if (e) goto cleanup;
	}

	{
		struct {
			char *name;
			dl_size_t name_length;
			char *typeString;
			dl_size_t typeString_length;
			char *script;
			dl_size_t script_length;
		} declarations[] = {{DL_STR("__declare"), DL_STR("(L L &rest 0 I)"), dl_null, 0},
		                    {DL_STR("__nop"), DL_STR("()"), dl_null, 0},
		                    {DL_STR("__funcall"), DL_STR("(I &rest 1 I)"), dl_null, 0},
		                    {DL_STR("__apply"), DL_STR("(I &rest 1 I)"), dl_null, 0},
		                    {DL_STR("__var"),
		                     DL_STR("(L I)"),
		                     DL_STR("(__declare-identifier (__infer-and-get-next-argument) (__quote L))")},
		                    {DL_STR("__global"), DL_STR("(L I)"), dl_null, 0},
		                    {DL_STR("__setq"), DL_STR("(L I)"), dl_null, 0},
		                    {DL_STR("__not"), DL_STR("(I)"), dl_null, 0},
		                    {DL_STR("__*"), DL_STR("(I I)"), dl_null, 0},
		                    {DL_STR("__/"), DL_STR("(I I)"), dl_null, 0},
		                    {DL_STR("__+"), DL_STR("(I I)"), dl_null, 0},
		                    {DL_STR("__-"), DL_STR("(I I)"), dl_null, 0},
		                    {DL_STR("__while"), DL_STR("(I &rest 1 I)"), dl_null, 0},
		                    {DL_STR("__if"), DL_STR("(I I I)"), dl_null, 0},
		                    {DL_STR("__when"), DL_STR("(I &rest 1 I)"), dl_null, 0},
		                    {DL_STR("__unless"), DL_STR("(I &rest 1 I)"), dl_null, 0},
		                    {DL_STR("__="), DL_STR("(I I)"), dl_null, 0},
		                    {DL_STR("__<"), DL_STR("(I I)"), dl_null, 0},
		                    {DL_STR("__>"), DL_STR("(I I)"), dl_null, 0},
		                    {DL_STR("__defun"),
		                     DL_STR("(L L &rest 1 I)"),
		                     DL_STR(" \
( \
 (__var name (__infer-and-get-next-argument)) \
 (__var parameters (__infer-and-get-next-argument)) \
 (__var type ()) \
 ( \
  (__var parameters parameters) \
  (__while parameters \
           (__if (__= (__quote &rest) (__car parameters)) \
                 (__setq type (__cons 0 (__cons (__quote &rest) type))) \
                 (__setq type (__cons (__quote I) type))) \
           (__setq parameters (__cdr parameters)))) \
 ( \
  (__var type2 type) \
  (__setq type ()) \
  (__while type2 \
           (__setq type (__cons (__car type2) type)) \
           (__setq type2 (__cdr type2)))) \
 (__declaration-scope \
  (__while parameters \
           (__unless (__= (__quote &rest) (__car parameters)) \
                     (__declare-identifier (__car parameters) (__quote L))) \
           (__setq parameters (__cdr parameters))) \
  (__declare-identifier (__quote self) type) \
  (__infer-and-get-next-argument)) \
 (__declare-identifier name type)) \
")},
		                    {DL_STR("__lambda"),
		                     DL_STR("(L &rest 1 I)"),
		                     DL_STR(" \
( \
 (__var parameters (__infer-and-get-next-argument)) \
 (__var type ()) \
 ( \
  (__var parameters parameters) \
  (__while parameters \
           (__if (__= (__quote &rest) (__car parameters)) \
                 (__setq type (__cons 0 (__cons (__quote &rest) type))) \
                 (__setq type (__cons (__quote I) type))) \
           (__setq parameters (__cdr parameters)))) \
 ( \
  (__var type2 type) \
  (__setq type ()) \
  (__while type2 \
           (__setq type (__cons (__car type2) type)) \
           (__setq type2 (__cdr type2)))) \
 (__declaration-scope \
  (__while parameters \
           (__unless (__= (__quote &rest) (__car parameters)) \
                     (__declare-identifier (__car parameters) (__quote L))) \
           (__setq parameters (__cdr parameters))) \
  (__declare-identifier (__quote self) type) \
  (__infer-and-get-next-argument))) \
")},
		                    {DL_STR("__defmacro"),
		                     DL_STR("(L L &rest 1 I)"),
		                     DL_STR(" \
( \
 (__var name (__infer-and-get-next-argument)) \
 (__var parameters (__infer-and-get-next-argument)) \
 (__var type ()) \
 ( \
  (__var parameters parameters) \
  (__while parameters \
           (__if (__= (__quote &rest) (__car parameters)) \
                 (__setq type (__cons 0 (__cons (__quote &rest) type))) \
                 (__setq type (__cons (__quote I) type))) \
           (__setq parameters (__cdr parameters)))) \
 ( \
  (__var type2 type) \
  (__setq type ()) \
  (__while type2 \
           (__setq type (__cons (__car type2) type)) \
           (__setq type2 (__cdr type2)))) \
 (__declaration-scope \
  (__while parameters \
           (__unless (__= (__quote &rest) (__car parameters)) \
                     (__declare-identifier (__car parameters) (__quote L))) \
           (__setq parameters (__cdr parameters))) \
  (__declare-identifier (__quote self) type) \
  (__infer-and-get-next-argument)) \
 (__declare-identifier name type)) \
")},
		                    {DL_STR("__noscope"),                DL_STR("(&rest 0 I)"),     dl_null, 0},
		                    {DL_STR("__comptime"),               DL_STR("(&rest 0 I)"),     dl_null, 0},
		                    {DL_STR("__quote"),                  DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__list"),                   DL_STR("(&rest 0 I)"),     dl_null, 0},
		                    {DL_STR("__vector"),                 DL_STR("(&rest 0 I)"),     dl_null, 0},
		                    {DL_STR("__make-vector"),            DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__get-vector-element"),     DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__set-vector-element"),     DL_STR("(I I I)"),         dl_null, 0},
		                    {DL_STR("__cons"),                   DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__car"),                    DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__cdr"),                    DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__set-car"),                DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__set-cdr"),                DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__null?"),                  DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__type-of"),                DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__make-type"),              DL_STR("()"),              dl_null, 0},
		                    {DL_STR("__make-instance"),          DL_STR("(I I I)"),         dl_null, 0},
		                    {DL_STR("__composite-value"),        DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__composite-function"),     DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__set-composite-value"),    DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__set-composite-function"), DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__make-string"),            DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__concatenate"),            DL_STR("(I I)"),           dl_null, 0},
		                    {DL_STR("__substring"),              DL_STR("(I I I)"),         dl_null, 0},
		                    {DL_STR("__length"),                 DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__symbol-string"),          DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__symbol-id"),              DL_STR("(I)"),             dl_null, 0},
		                    {DL_STR("__error"),                  DL_STR("(I)"),             dl_null, 0}};
		DL_DOTIMES(i, sizeof(declarations)/sizeof(*declarations)) {
			duckLisp_ast_compoundExpression_t typeAst;
			duckLisp_ast_compoundExpression_t scriptAst;
			(void) duckLisp_ast_compoundExpression_init(&typeAst);
			(void) duckLisp_ast_compoundExpression_init(&scriptAst);

			e = duckLisp_read(&state.duckLisp,
			                  dl_false,
			                  0,
			                  DL_STR("<INFERRER>"),
			                  declarations[i].typeString,
			                  declarations[i].typeString_length,
			                  &typeAst,
			                  0,
			                  dl_true);
			if (e) goto cleanup;

			if (declarations[i].script_length > 0) {
				e = duckLisp_read(&state.duckLisp,
				                  dl_false,
				                  0,
				                  DL_STR("<INFERRER>"),
				                  declarations[i].script,
				                  declarations[i].script_length,
				                  &scriptAst,
				                  0,
				                  dl_true);
				if (e) goto cleanup;
			}

			e = compileAndAddDeclaration(&state,
			                             fileName,
			                             fileName_length,
			                             declarations[i].name,
			                             declarations[i].name_length,
			                             typeAst,
			                             scriptAst);
			if (e) goto cleanup;
		}
	}


	e = infer_compoundExpression(&state, fileName, fileName_length, ast, dl_true);
	if (e) goto cleanup;

 cleanup:
	eError = dl_array_pushElements(errors, state.duckLisp.errors.elements, state.duckLisp.errors.elements_length);
	if (eError) e = eError;
	state.duckLisp.errors.elements = dl_null;
	state.duckLisp.errors.elements_length = 0;
	eError = dl_array_pushElements(errors, state.duckVM.errors.elements, state.duckVM.errors.elements_length);
	if (eError) e = eError;
	state.duckVM.errors.elements = dl_null;
	state.duckVM.errors.elements_length = 0;

	eError = inferrerState_quit(&state);
	if (eError) e = eError;

	return e;
}

#endif /* USE_PARENTHESIS_INFERENCE */
