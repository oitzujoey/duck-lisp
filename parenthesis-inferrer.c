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


typedef enum {
	inferrerTypeSymbol_L,
	inferrerTypeSymbol_I
} inferrerTypeSymbol_t;

typedef enum {
	inferrerTypeSignature_type_expression,
	inferrerTypeSignature_type_symbol
} inferrerTypeSignature_type_t;

typedef struct inferrerTypeSignature_s {
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
	inferrerTypeSignature_type_t type;
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
	dl_array_t *errors;  /* dl_array_t:duckLisp_error_t */
	const char *fileName;
	dl_size_t fileName_length;
	dl_array_t scopeStack;  /* dl_array_t:inferrerScope_t */
} inferrerState_t;


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

	inferrerTypeSignature->value.expression.variadic = dl_false;
	inferrerTypeSignature->value.expression.defaultRestLength = 0;
	e = inferrerTypeSignature_quit(inferrerTypeSignature->value.expression.restSignature, memoryAllocation);

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
			printf(" &rest %zu ", inferrerTypeSignature.value.expression.defaultRestLength);
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
                                     dl_memoryAllocation_t *ma,
                                     dl_size_t maxComptimeVmObjects,
                                     dl_array_t *errors,
                                     const char *fileName,
                                     const dl_size_t fileName_length) {
	dl_error_t e = dl_error_ok;

	inferrerState->memoryAllocation = ma;
	e = duckLisp_init(&inferrerState->duckLisp, ma, maxComptimeVmObjects);
	if (e) goto cleanup;
	inferrerState->errors = errors;
	inferrerState->fileName = fileName;
	inferrerState->fileName_length = fileName_length;
	(void) dl_array_init(&inferrerState->scopeStack, ma, sizeof(inferrerScope_t), dl_array_strategy_double);

 cleanup: return e;
}

static dl_error_t inferrerState_quit(inferrerState_t *s) {
	dl_error_t e = dl_error_ok;
	DL_DOTIMES(i, s->scopeStack.elements_length) {
		inferrerScope_t is;
		e = dl_array_popElement(&s->scopeStack, &is);
		if (e) break;
		e = inferrerScope_quit(&is, s->memoryAllocation);
	}
	e = dl_array_quit(&s->scopeStack);
	s->fileName_length = 0;
	s->fileName = dl_null;
	s->errors = dl_null;
	(void) duckLisp_quit(&s->duckLisp);
	s->memoryAllocation = dl_null;
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


static dl_error_t infer_compoundExpression(inferrerState_t *state,
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


/* Infer a single argument from the tokens/forms. */
static dl_error_t inferArgument(inferrerState_t *state,
                                duckLisp_ast_expression_t *expression,
                                dl_ptrdiff_t *index,
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
		
		DL_DOTIMES(i, compoundExpression->value.identifier.value_length) {
			putchar(compoundExpression->value.identifier.value[i]);
		}
		putchar('\n');
		
		inferrerType_t type;
		dl_bool_t found = dl_false;
		e = findDeclaration(state, &found, &type, compoundExpression->value.identifier);
		if (e) goto cleanup;

		if (found && infer) {
			/* Declared */
			if (type.bytecode_length > 0) {
				/* Execute bytecode */
			}

			ast_print_compoundExpression(state->duckLisp, *compoundExpression);
			printf("::");
			inferrerTypeSignature_print(type.type);
			putchar('\n');

			if (type.type.type == inferrerTypeSignature_type_expression) {
				dl_array_t newExpression;
				(void) dl_array_init(&newExpression,
				                     state->memoryAllocation,
				                     sizeof(duckLisp_ast_compoundExpression_t),
				                     dl_array_strategy_double);
				e = dl_array_pushElement(&newExpression, compoundExpression);
				if (e) goto expressionCleanup;
				DL_DOTIMES(l, type.type.value.expression.positionalSignatures_length) {
					if ((dl_size_t) localIndex >= expression->compoundExpressions_length) {
						e = dl_error_invalidValue;
						(eError
						 = duckLisp_error_pushInference(state,
						                                DL_STR("Too few arguments for declared identifier.")));
						if (eError) e = eError;
						goto expressionCleanup;
					}
					inferrerTypeSignature_t argSignature = type.type.value.expression.positionalSignatures[l];
					if (argSignature.type == inferrerTypeSignature_type_symbol) {
						dl_ptrdiff_t lastIndex = localIndex;
						e = inferArgument(state,
						                  expression,
						                  &localIndex,
						                  (argSignature.value.symbol == inferrerTypeSymbol_I) ? infer : dl_false);
						if (e) goto expressionCleanup;
						e = dl_array_pushElement(&newExpression, &expression->compoundExpressions[lastIndex]);
						if (e) goto expressionCleanup;
						newLength++;
					}
					else {
						(eError
						 = duckLisp_error_pushInference(state,
						                                DL_STR("Nested expression types are not yet supported.")));
						if (eError) e = eError;
					}
				}
				if (type.type.value.expression.variadic) {
					inferrerTypeSignature_t argSignature = *type.type.value.expression.restSignature;
					DL_DOTIMES(l, type.type.value.expression.defaultRestLength) {
						if (argSignature.type == inferrerTypeSignature_type_symbol) {
							dl_ptrdiff_t lastIndex = localIndex;
							e = inferArgument(state,
							                  expression,
							                  &localIndex,
							                  ((argSignature.value.symbol == inferrerTypeSymbol_I)
							                   ? infer
							                   : dl_false));
							if (e) goto expressionCleanup;
							e = dl_array_pushElement(&newExpression, &expression->compoundExpressions[lastIndex]);
							if (e) goto expressionCleanup;
							newLength++;
						}
						else {
							(eError
							 = duckLisp_error_pushInference(state,
							                                DL_STR("Nested expression types are not yet supported.")));
							if (eError) e = eError;
						}
					}
				}

			expressionCleanup:
				if (e) {
					eError = dl_array_quit(&newExpression);
					if (eError) e = eError;
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
					printf("EXPRESSION: ");
					DL_DOTIMES(m, newExpression.elements_length) {
						duckLisp_ast_compoundExpression_t ce;
						e = dl_array_get(&newExpression, &ce, m);
						ast_print_compoundExpression(state->duckLisp, ce);
						putchar(' ');
					}
					puts("");
					printf("ORIGINAL EXPRESSION: ");
					DL_DOTIMES(m, expression->compoundExpressions_length) {
						ast_print_compoundExpression(state->duckLisp, expression->compoundExpressions[m]);
						putchar(' ');
					}
					puts("");
				}
				if (e) goto cleanup;
			}
			else {
				/* Symbol: Do nothing */
			}
		}
		else {
			/* Undeclared */
			puts("Undeclared");
			e = infer_compoundExpression(state, compoundExpression, infer);
			if (e) goto cleanup;
		}
	}
	else {
		puts("Other");
		e = infer_compoundExpression(state, compoundExpression, infer);
		if (e) goto cleanup;
	}
	*index = localIndex;

 cleanup: return e;
}

/* Infer a single argument from the tokens/forms. */
static dl_error_t inferArguments(inferrerState_t *state,
                                 duckLisp_ast_expression_t *expression,
                                 dl_ptrdiff_t index,
                                 dl_bool_t infer) {
	dl_error_t e = dl_error_ok;

	while ((dl_size_t) index < expression->compoundExpressions_length) {
		/* Run inference on the current identifier. */
		dl_ptrdiff_t startIndex = index;
		/* The function needs the whole expression. It is given the pointer to the current index. */
		e = inferArgument(state, expression, &index, infer);
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
				inferrerType_t type;
				inferrerScope_t scope;
				(void) inferrerType_init(&type);

				e = popDeclarationScope(state, &scope);
				if (e) goto scopeCleanup;

				e = inferrerTypeSignature_fromAst(state, &type.type, typeAst);
				if (e) goto scopeCleanup;
				e = dl_trie_insert(&scope.identifiersTrie,
				                   identifier.value,
				                   identifier.value_length,
				                   scope.types.elements_length);
				if (e) goto scopeCleanup;

				type.bytecode = dl_null;
				type.bytecode_length = 0;

				e = dl_array_pushElement(&scope.types, &type);
				if (e) goto scopeCleanup;
				e = pushDeclarationScope(state, &scope);
				if (e) goto scopeCleanup;
			scopeCleanup:
				if (e) goto cleanup;
				inferrerTypeSignature_print(type.type);
			}
		}
	}

 cleanup: return e;
}

static dl_error_t infer_expression(inferrerState_t *state,
                                   duckLisp_ast_compoundExpression_t *compoundExpression,
                                   dl_bool_t infer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

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
	         type check
	         nested types

	   Two functions can only be used if the right data structure exists. Or maybe I can copy the tree instead of
	   modifying it?

	   declare m (L I)
	   m a 1  —  Infer
	   (m a 1)  —  Check arity
	   (#m a 1)  —  Untyped
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

		e = inferArguments(state, expression, 1, dl_false);
		if (e) goto cleanup;

		compoundExpression->type = duckLisp_ast_type_expression;
	}
	else if (head->type == duckLisp_ast_type_callback) {
		/* Don't type-check. */
		e = infer_callback(state, head, dl_false);
		if (e) goto cleanup;
		/* Run argument inference */

		e = inferArguments(state, expression, 1, infer);
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
			if (type.bytecode_length > 0) {
				/* Execute bytecode */
			}

			dl_ptrdiff_t index = 1;
			{
				DL_DOTIMES(l, type.type.value.expression.positionalSignatures_length) {
					if ((dl_size_t) index >= expression->compoundExpressions_length) {
						e = dl_error_invalidValue;
						(eError
						 = duckLisp_error_pushInference(state,
						                                DL_STR("Too few arguments for declared identifier.")));
						if (eError) e = eError;
						goto expressionCleanup;
					}
					inferrerTypeSignature_t argSignature = type.type.value.expression.positionalSignatures[l];
					if (argSignature.type == inferrerTypeSignature_type_symbol) {
						e = inferArgument(state,
						                  expression,
						                  &index,
						                  (argSignature.value.symbol == inferrerTypeSymbol_I) ? infer : dl_false);
						if (e) goto expressionCleanup;
					}
					else {
						(eError
						 = duckLisp_error_pushInference(state,
						                                DL_STR("Nested expression types are not yet supported.")));
						if (eError) e = eError;
					}
				}
				if (type.type.value.expression.variadic) {
					inferrerTypeSignature_t argSignature = *type.type.value.expression.restSignature;
					while ((dl_size_t) index < expression->compoundExpressions_length) {
						if (argSignature.type == inferrerTypeSignature_type_symbol) {
							e = inferArgument(state,
							                  expression,
							                  &index,
							                  ((argSignature.value.symbol == inferrerTypeSymbol_I)
							                   ? infer
							                   : dl_false));
							if (e) goto expressionCleanup;
						}
						else {
							(eError
							 = duckLisp_error_pushInference(state,
							                                DL_STR("Nested expression types are not yet supported.")));
							if (eError) e = eError;
						}
					}
				}
				else if ((dl_size_t) index > expression->compoundExpressions_length) {
					e = dl_error_invalidValue;
					eError = duckLisp_error_pushInference(state, DL_STR("Too few arguments for identifier."));
					if (eError) e = eError;
				}
				else if ((dl_size_t) index < expression->compoundExpressions_length) {
					e = dl_error_invalidValue;
					eError = duckLisp_error_pushInference(state, DL_STR("Too many arguments for identifier."));
					if (eError) e = eError;
				}

			expressionCleanup:
				if (e) goto cleanup;
			}
		}
		else {
			/* Undeclared. */
			/* Argument inference stuff goes here. */
			e = inferArguments(state, expression, 1, infer);
			if (e) goto cleanup;
		}

		(void) expressionIdentifier;
	}
	else {
		/* This is a scope. */
		inferrerScope_t scope;
		(void) inferrerScope_init(&scope, state->memoryAllocation);
		e = pushDeclarationScope(state, &scope);
		if (e) goto cleanup;

		e = inferArguments(state, expression, 0, infer);
		if (e) goto cleanup;

		e = popDeclarationScope(state, dl_null);
		if (e) goto cleanup;
	}

 cleanup: return e;
}

static dl_error_t infer_compoundExpression(inferrerState_t *state,
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
		e = infer_expression(state, compoundExpression, infer);
		break;
	default:
		eError = duckLisp_error_pushInference(state, DL_STR("Illegal type."));
		e = eError ? eError : dl_error_cantHappen;
	}
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t callback_declareIdentifier(duckVM_t *vm) {
	(void) vm;
	return dl_error_ok;
}

static dl_error_t callback_inferAndGetNextArgument(duckVM_t *vm) {
	(void) vm;
	return dl_error_ok;
}

static dl_error_t callback_pushScope(duckVM_t *vm) {
	(void) vm;
	return dl_error_ok;
}

static dl_error_t callback_popScope(duckVM_t *vm) {
	(void) vm;
	return dl_error_ok;
}

static dl_error_t generator_declarationScope(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_ast_expression_t *expression) {
	(void) duckLisp;
	(void) compileState;
	(void) assembly;
	(void) expression;
	return dl_error_ok;
}


dl_error_t inferParentheses(dl_memoryAllocation_t *memoryAllocation,
                            const dl_size_t maxComptimeVmObjects,
                            dl_array_t *errors,
                            const char *fileName,
                            const dl_size_t fileName_length,
                            duckLisp_ast_compoundExpression_t *ast) {
	dl_error_t e = dl_error_ok;

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
	}

	e = duckLisp_addGenerator(&state.duckLisp, generator_declarationScope, DL_STR("__declaration-scope"));
	if (e) goto cleanup;

	{
		inferrerType_t type;
		inferrerScope_t scope;

		(void) inferrerScope_init(&scope, memoryAllocation);

		(void) inferrerType_init(&type);
		{
			duckLisp_ast_compoundExpression_t ast;
			e = duckLisp_read(&state.duckLisp,
			                  dl_false,
			                  0,
			                  DL_STR("<INFERRER>"),
			                  DL_STR("(L L &rest 0 I)"),
			                  &ast,
			                  0,
			                  dl_true);
			if (e) goto scopeCleanup;
			e = inferrerTypeSignature_fromAst(&state, &type.type, ast);
			if (e) goto scopeCleanup;
		}
		e = dl_trie_insert(&scope.identifiersTrie, DL_STR("__declare"), scope.types.elements_length);
		if (e) goto scopeCleanup;
		e = dl_array_pushElement(&scope.types, &type);
		if (e) goto scopeCleanup;

		e = pushDeclarationScope(&state, &scope);
		if (e) goto scopeCleanup;
	scopeCleanup:
		if (e) goto cleanup;
	}

	e = infer_compoundExpression(&state, ast, dl_true);
	if (e) goto cleanup;

	printf("\nFINAL: ");
	ast_print_compoundExpression(state.duckLisp, *ast);
	puts("");

	e = inferrerState_quit(&state);
	if (e) goto cleanup;

 cleanup: return e;
}
