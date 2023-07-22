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

#include "duckLisp.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "DuckLib/sort.h"
#include "DuckLib/trie.h"
#include <stdio.h>

dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_defun(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);
static dl_error_t scope_getFunctionFromName(duckLisp_t *duckLisp,
                                            duckLisp_subCompileState_t *subCompileState,
                                            duckLisp_functionType_t *functionType,
                                            dl_ptrdiff_t *index,
                                            const char *name,
                                            const dl_size_t name_length);

/*
  ===============
  Error reporting
  ===============
*/

dl_error_t duckLisp_error_pushRuntime(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_error_t error;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &error.message, message_length * sizeof(char));
	if (e) goto cleanup;
	e = dl_memcopy((void *) error.message, (void *) message, message_length * sizeof(char));
	if (e) goto cleanup;

	error.message_length = message_length;
	error.start_index = -1;
	error.end_index = -1;

	e = dl_array_pushElement(&duckLisp->errors, &error);
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_checkArgsAndReportError(duckLisp_t *duckLisp,
                                            duckLisp_ast_expression_t astExpression,
                                            const dl_size_t numArgs,
                                            const dl_bool_t variadic) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_array_t string;
	char *fewMany = dl_null;
	dl_size_t fewMany_length = 0;

	/**/ dl_array_init(&string, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	if (astExpression.compoundExpressions_length == 0) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if (astExpression.compoundExpressions[0].type != duckLisp_ast_type_identifier) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if ((!variadic && (astExpression.compoundExpressions_length != numArgs))
	    || (variadic && (astExpression.compoundExpressions_length < numArgs))) {
		e = dl_array_pushElements(&string, DL_STR("Too "));
		if (e) goto cleanup;

		if (astExpression.compoundExpressions_length < numArgs) {
			fewMany = "few";
			fewMany_length = sizeof("few") - 1;
		}
		else {
			fewMany = "many";
			fewMany_length = sizeof("many") - 1;
		}

		e = dl_array_pushElements(&string, fewMany, fewMany_length);
		if (e) goto cleanup;

		e = dl_array_pushElements(&string, DL_STR(" arguments for function \""));
		if (e) goto cleanup;

		e = dl_array_pushElements(&string, astExpression.compoundExpressions[0].value.identifier.value,
		                          astExpression.compoundExpressions[0].value.identifier.value_length);
		if (e) goto cleanup;

		e = dl_array_pushElements(&string, DL_STR("\"."));
		if (e) goto cleanup;

		e = duckLisp_error_pushRuntime(duckLisp,
		                               (char *) string.elements,
		                               string.elements_length * string.element_size);
		if (e) goto cleanup;

		e = dl_error_invalidValue;
	}

 cleanup:

	eError = dl_array_quit(&string);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_checkTypeAndReportError(duckLisp_t *duckLisp,
                                            duckLisp_ast_identifier_t functionName,
                                            duckLisp_ast_compoundExpression_t astCompoundExpression,
                                            const duckLisp_ast_type_t type) {
	dl_error_t e = dl_error_ok;

	dl_array_t string;
	/**/ dl_array_init(&string, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	const duckLisp_ast_identifier_t typeString[] = {
#define X(s) s, sizeof(s) - 1
		{X("duckLisp_ast_type_none")},
		{X("duckLisp_ast_type_expression")},
		{X("duckLisp_ast_type_identifier")},
		{X("duckLisp_ast_type_string")},
		{X("duckLisp_ast_type_float")},
		{X("duckLisp_ast_type_int")},
		{X("duckLisp_ast_type_bool")},
#undef X
	};


	if (astCompoundExpression.type != type) {
		e = dl_array_pushElements(&string, DL_STR("Expected type \""));
		if (e) goto l_cleanup;

		e = dl_array_pushElements(&string, typeString[type].value, typeString[type].value_length);
		if (e) goto l_cleanup;

		e = dl_array_pushElements(&string, DL_STR("\" for argument "));
		if (e) goto l_cleanup;

		e = dl_array_pushElements(&string, DL_STR(" of function \""));
		if (e) goto l_cleanup;

		e = dl_array_pushElements(&string, functionName.value, functionName.value_length);
		if (e) goto l_cleanup;

		e = dl_array_pushElements(&string, DL_STR("\". Was passed type \""));
		if (e) goto l_cleanup;

		e = dl_array_pushElements(&string,
		                          typeString[astCompoundExpression.type].value,
		                          typeString[astCompoundExpression.type].value_length);
		if (e) goto l_cleanup;

		e = dl_array_pushElements(&string, DL_STR("\"."));
		if (e) goto l_cleanup;

		e = duckLisp_error_pushRuntime(duckLisp,
		                               (char *) string.elements,
		                               string.elements_length * string.element_size);
		if (e) goto l_cleanup;

		e = dl_error_invalidValue;
	}

 l_cleanup:

	return e;
}


/*
  =======
  Symbols
  =======
 */

/* Accepts a symbol name and returns its value. Returns -1 if the symbol is not found. */
dl_ptrdiff_t duckLisp_symbol_nameToValue(const duckLisp_t *duckLisp, const char *name, const dl_size_t name_length) {
	dl_ptrdiff_t value = -1;
	/**/ dl_trie_find(duckLisp->symbols_trie, &value, name, name_length);
	return value;
}

/* Guaranteed not to create a new symbol if a symbol with the given name already exists. */
dl_error_t duckLisp_symbol_create(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	dl_ptrdiff_t key = duckLisp_symbol_nameToValue(duckLisp, name, name_length);
	if (key == -1) {
		duckLisp_ast_identifier_t tempIdentifier;
		e = dl_trie_insert(&duckLisp->symbols_trie, name, name_length, duckLisp->symbols_array.elements_length);
		if (e) goto l_cleanup;
		tempIdentifier.value_length = name_length;
		e = dl_malloc(duckLisp->memoryAllocation, (void **) &tempIdentifier.value, name_length);
		if (e) goto l_cleanup;
		/**/ dl_memcopy_noOverlap(tempIdentifier.value, name, name_length);
		e = dl_array_pushElement(&duckLisp->symbols_array, (void *) &tempIdentifier);
		if (e) goto l_cleanup;
	}

 l_cleanup:
	return e;
}


/*
  =====
  Scope
  =====
*/

static void scope_init(duckLisp_t *duckLisp, duckLisp_scope_t *scope, dl_bool_t is_function) {
	/**/ dl_trie_init(&scope->locals_trie, duckLisp->memoryAllocation, -1);
	/**/ dl_trie_init(&scope->functions_trie, duckLisp->memoryAllocation, -1);
	scope->functions_length = 0;
	/**/ dl_trie_init(&scope->macros_trie, duckLisp->memoryAllocation, -1);
	scope->macros_length = 0;
	/**/ dl_trie_init(&scope->labels_trie, duckLisp->memoryAllocation, -1);
	scope->function_scope = is_function;
	scope->scope_uvs = dl_null;
	scope->scope_uvs_length = 0;
	scope->function_uvs = dl_null;
	scope->function_uvs_length = 0;
}

static dl_error_t scope_quit(duckLisp_t *duckLisp, duckLisp_scope_t *scope) {
	dl_error_t e = dl_error_ok;
	(void) duckLisp;
	/**/ dl_trie_quit(&scope->locals_trie);
	/**/ dl_trie_quit(&scope->functions_trie);
	scope->functions_length = 0;
	/**/ dl_trie_quit(&scope->macros_trie);
	scope->macros_length = 0;
	/**/ dl_trie_quit(&scope->labels_trie);
	scope->function_scope = dl_false;
	if (scope->scope_uvs != dl_null) {
		e = dl_free(duckLisp->memoryAllocation, (void **) &scope->scope_uvs);
		if (e) goto cleanup;
	}
	scope->scope_uvs_length = 0;
	if (scope->function_uvs != dl_null) {
		e = dl_free(duckLisp->memoryAllocation, (void **) &scope->function_uvs);
		if (e) goto cleanup;
	}
	scope->function_uvs_length = 0;

 cleanup:
	return e;
}

dl_error_t duckLisp_pushScope(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              duckLisp_scope_t *scope,
                              dl_bool_t is_function) {
	dl_error_t e = dl_error_ok;

	duckLisp_subCompileState_t *subCompileState = &compileState->runtimeCompileState;
	if (scope == dl_null) {
		duckLisp_scope_t localScope;
		/**/ scope_init(duckLisp,
		                &localScope,
		                is_function && (subCompileState == compileState->currentCompileState));
		e = dl_array_pushElement(&subCompileState->scope_stack, &localScope);
	}
	else {
		e = dl_array_pushElement(&subCompileState->scope_stack, scope);
	}
	if (e) goto cleanup;

	subCompileState = &compileState->comptimeCompileState;
	if (scope == dl_null) {
		duckLisp_scope_t localScope;
		/**/ scope_init(duckLisp,
		                &localScope,
		                is_function && (subCompileState == compileState->currentCompileState));
		e = dl_array_pushElement(&subCompileState->scope_stack, &localScope);
	}
	else {
		e = dl_array_pushElement(&subCompileState->scope_stack, scope);
	}

 cleanup:
	return e;
}

static dl_error_t scope_getTop(duckLisp_t *duckLisp,
                               duckLisp_subCompileState_t *subCompileState,
                               duckLisp_scope_t *scope) {
	dl_error_t e = dl_error_ok;

	e = dl_array_getTop(&subCompileState->scope_stack, scope);
	if (e == dl_error_bufferUnderflow) {
		/* Push a scope if we don't have one yet. */
		/**/ scope_init(duckLisp, scope, dl_true);
		e = dl_array_pushElement(&subCompileState->scope_stack, scope);
		if (e) goto cleanup;
	}

 cleanup:
	return e;
}

static dl_error_t scope_setTop(duckLisp_subCompileState_t *subCompileState, duckLisp_scope_t *scope) {
	return dl_array_set(&subCompileState->scope_stack, scope, subCompileState->scope_stack.elements_length - 1);
}

dl_error_t duckLisp_popScope(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             duckLisp_scope_t *scope) {
	dl_error_t e = dl_error_ok;
	duckLisp_scope_t local_scope;
	duckLisp_subCompileState_t *subCompileState = &compileState->runtimeCompileState;
	if (subCompileState->scope_stack.elements_length > 0) {
		e = scope_getTop(duckLisp, subCompileState, &local_scope);
		if (e) goto cleanup;
		e = scope_quit(duckLisp, &local_scope);
		if (e) goto cleanup;
		e = scope_setTop(subCompileState, &local_scope);
		if (e) goto cleanup;
		e = dl_array_popElement(&subCompileState->scope_stack, scope);
	}
	else e = dl_error_bufferUnderflow;

	subCompileState = &compileState->comptimeCompileState;
	if (subCompileState->scope_stack.elements_length > 0) {
		e = scope_getTop(duckLisp, subCompileState, &local_scope);
		if (e) goto cleanup;
		e = scope_quit(duckLisp, &local_scope);
		if (e) goto cleanup;
		e = scope_setTop(subCompileState, &local_scope);
		if (e) goto cleanup;
		e = dl_array_popElement(&subCompileState->scope_stack, scope);
	}
	else e = dl_error_bufferUnderflow;

 cleanup:
	return e;
}

/*
  Failure if return value is set or index is -1.
  "Local" is defined as remaining inside the current function.
*/
dl_error_t duckLisp_scope_getMacroFromName(duckLisp_subCompileState_t *subCompileState,
                                           dl_ptrdiff_t *index,
                                           const char *name,
                                           const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = subCompileState->scope_stack.elements_length;

	*index = -1;

	while (dl_true) {
		e = dl_array_get(&subCompileState->scope_stack, (void *) &scope, --(scope_index));
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
			}
			break;
		}

		/**/ dl_trie_find(scope.locals_trie, index, name, name_length);
		if (*index != -1) {
			break;
		}
	};

	return e;
}

/*
  Failure if return value is set or index is -1.
  "Local" is defined as remaining inside the current function.
*/
dl_error_t duckLisp_scope_getLocalIndexFromName(duckLisp_subCompileState_t *subCompileState,
                                                dl_ptrdiff_t *index,
                                                const char *name,
                                                const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = subCompileState->scope_stack.elements_length;

	*index = -1;

	do {
		e = dl_array_get(&subCompileState->scope_stack, (void *) &scope, --(scope_index));
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
			}
			break;
		}

		/**/ dl_trie_find(scope.locals_trie, index, name, name_length);
		if (*index != -1) {
			break;
		}
	} while (!scope.function_scope);

	return e;
}

dl_error_t duckLisp_scope_getFreeLocalIndexFromName_helper(duckLisp_t *duckLisp,
                                                           duckLisp_subCompileState_t *subCompileState,
                                                           dl_bool_t *found,
                                                           dl_ptrdiff_t *index,
                                                           dl_ptrdiff_t *scope_index,
                                                           const char *name,
                                                           const dl_size_t name_length,
                                                           duckLisp_scope_t function_scope,
                                                           dl_ptrdiff_t function_scope_index) {
	dl_error_t e = dl_error_ok;

	// Fix this stupid variable here.
	dl_ptrdiff_t return_index = -1;
	/* First look for an upvalue in the scope immediately above. If it exists, make a normal upvalue to it. If it
	   doesn't exist, search in higher scopes. If it exists, create an upvalue to it in the function below that scope.
	   Then chain upvalues leading to that upvalue through all the nested functions. Stack upvalues will have a positive
	   index. Upvalue upvalues will have a negative index.
	   Scopes will always have positive indices. Functions may have negative indices.
	*/

	/* dl_ptrdiff_t local_index; */
	*found = dl_false;

	duckLisp_scope_t scope = {0};
	do {
		e = dl_array_get(&subCompileState->scope_stack, (void *) &scope, --(*scope_index));
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
			}
			goto cleanup;
		}

		/**/ dl_trie_find(scope.locals_trie, index, name, name_length);
		if (*index != -1) {
			*found = dl_true;
			break;
		}
	} while (!scope.function_scope);
	dl_ptrdiff_t local_scope_index = *scope_index;
	dl_bool_t chained = !*found;
	if (chained) {
		e = duckLisp_scope_getFreeLocalIndexFromName_helper(duckLisp,
		                                                    subCompileState,
		                                                    found,
		                                                    index,
		                                                    scope_index,
		                                                    name,
		                                                    name_length,
		                                                    scope,
		                                                    *scope_index);
		if (e) goto cleanup;
		// Don't set `index` below here.
		// Create a closure to the scope above.
		if (*index >= 0) *index = -(*index + 1);
	}
	/* sic. */
	if (*found) {
		/* We found it, which means it's an upvalue. Check to make sure it has been registered. */
		dl_bool_t found_upvalue = dl_false;
		DL_DOTIMES(i, function_scope.function_uvs_length) {
			if (function_scope.function_uvs[i] == *index) {
				found_upvalue = dl_true;
				return_index = i;
				break;
			}
		}
		if (!found_upvalue) {
			e = dl_array_get(&subCompileState->scope_stack, (void *) &function_scope, function_scope_index);
			if (e) {
				if (e == dl_error_invalidValue) {
					e = dl_error_ok;
				}
				goto cleanup;
			}
			/* Not registered. Register. */
			e = dl_realloc(duckLisp->memoryAllocation,
			               (void **) &function_scope.function_uvs,
			               (function_scope.function_uvs_length + 1) * sizeof(dl_ptrdiff_t));
			if (e) goto cleanup;
			function_scope.function_uvs_length++;
			function_scope.function_uvs[function_scope.function_uvs_length - 1] = *index;
			return_index = function_scope.function_uvs_length - 1;
			e = dl_array_set(&subCompileState->scope_stack, (void *) &function_scope, function_scope_index);
			if (e) goto cleanup;
		}

		/* Now register the upvalue in the original scope if needed. */
		found_upvalue = dl_false;
		DL_DOTIMES(i, scope.scope_uvs_length) {
			if (scope.scope_uvs[i] == *index) {
				found_upvalue = dl_true;
				break;
			}
		}
		if (!found_upvalue) {
			e = dl_array_get(&subCompileState->scope_stack, (void *) &scope, local_scope_index);
			if (e) {
				if (e == dl_error_invalidValue) {
					e = dl_error_ok;
				}
				goto cleanup;
			}
			e = dl_realloc(duckLisp->memoryAllocation,
			               (void **) &scope.scope_uvs,
			               (scope.scope_uvs_length + 1) * sizeof(dl_ptrdiff_t));
			if (e) goto cleanup;
			scope.scope_uvs_length++;
			scope.scope_uvs[scope.scope_uvs_length - 1] = *index;
			e = dl_array_set(&subCompileState->scope_stack, (void *) &scope, local_scope_index);
			if (e) goto cleanup;
		}
		*index = return_index;
	}

 cleanup:
	return e;
}

dl_error_t duckLisp_scope_getFreeLocalIndexFromName(duckLisp_t *duckLisp,
                                                    duckLisp_subCompileState_t *subCompileState,
                                                    dl_bool_t *found,
                                                    dl_ptrdiff_t *index,
                                                    dl_ptrdiff_t *scope_index,
                                                    const char *name,
                                                    const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;
	duckLisp_scope_t function_scope;
	dl_ptrdiff_t function_scope_index = subCompileState->scope_stack.elements_length;
	/* Skip the current function. */
	do {
		e = dl_array_get(&subCompileState->scope_stack, (void *) &function_scope, --function_scope_index);
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
				*found = dl_false;
			}
			goto cleanup;
		}
	} while (!function_scope.function_scope);

	*scope_index = function_scope_index;
	e = duckLisp_scope_getFreeLocalIndexFromName_helper(duckLisp,
	                                                    subCompileState,
	                                                    found,
	                                                    index,
	                                                    scope_index,
	                                                    name,
	                                                    name_length,
	                                                    function_scope,
	                                                    function_scope_index);
 cleanup:
	return e;
}

static dl_error_t scope_getFunctionFromName(duckLisp_t *duckLisp,
                                            duckLisp_subCompileState_t *subCompileState,
                                            duckLisp_functionType_t *functionType,
                                            dl_ptrdiff_t *index,
                                            const char *name,
                                            const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = subCompileState->scope_stack.elements_length;
	dl_ptrdiff_t tempPtrdiff = -1;
	*index = -1;
	*functionType = duckLisp_functionType_none;

	/* Check functions */

	while (dl_true) {
		e = dl_array_get(&subCompileState->scope_stack, (void *) &scope, --scope_index);
		if (e) {
			if (e == dl_error_invalidValue) {
				/* We've gone though all the scopes. */
				e = dl_error_ok;
			}
			break;
		}

		/**/ dl_trie_find(scope.functions_trie, &tempPtrdiff, name, name_length);
		/* Return the function in the nearest scope. */
		if (tempPtrdiff != -1) {
			break;
		}
	}

	if (tempPtrdiff == -1) {
		*functionType = duckLisp_functionType_none;

		/* Check globals */

		/**/ dl_trie_find(duckLisp->callbacks_trie, index, name, name_length);
		if (*index != -1) {
			*index = duckLisp_symbol_nameToValue(duckLisp, name, name_length);
			*functionType = duckLisp_functionType_c;
		}
		else {
			/* Check generators */

			/**/ dl_trie_find(duckLisp->generators_trie, index, name, name_length);
			if (*index != -1) {
				*functionType = duckLisp_functionType_generator;
			}
		}
	}
	else {
		*functionType = tempPtrdiff;
	}

	return e;
}

static dl_error_t scope_getLabelFromName(duckLisp_subCompileState_t *subCompileState,
                                         dl_ptrdiff_t *index,
                                         const char *name,
                                         dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = subCompileState->scope_stack.elements_length;

	*index = -1;

	while (dl_true) {
		e = dl_array_get(&subCompileState->scope_stack, (void *) &scope, --scope_index);
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
			}
			break;
		}

		/**/ dl_trie_find(scope.labels_trie, index, name, name_length);
		if (*index != -1) {
			break;
		}
	}

	return e;
}

static void incrementLocalsLength(duckLisp_compileState_t *compileState) {
	compileState->currentCompileState->locals_length++;
}

static void decrementLocalsLength(duckLisp_compileState_t *compileState) {
	--compileState->currentCompileState->locals_length;
}

static dl_size_t getLocalsLength(duckLisp_compileState_t *compileState) {
	return compileState->currentCompileState->locals_length;
}

/*
  ========
  Emitters
  ========
*/

dl_error_t duckLisp_emit_nullaryOperator(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_instructionClass_t instructionClass) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = instructionClass;

	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ incrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_unaryOperator(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_instructionClass_t instructionClass,
                                       duckLisp_instructionArgClass_t argument) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = instructionClass;

	/* Push arguments into instruction. */
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ incrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_binaryOperator(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_instructionClass_t instructionClass,
                                        duckLisp_instructionArgClass_t argument0,
                                        duckLisp_instructionArgClass_t argument1) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = instructionClass;

	// Push arguments into instruction.
	e = dl_array_pushElement(&instruction.args, &argument0);
	if (e) goto cleanup;

	e = dl_array_pushElement(&instruction.args, &argument1);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ incrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_ternaryOperator(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_instructionClass_t instructionClass,
                                        duckLisp_instructionArgClass_t argument0,
                                        duckLisp_instructionArgClass_t argument1,
                                        duckLisp_instructionArgClass_t argument2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = instructionClass;

	// Push arguments into instruction.
	e = dl_array_pushElement(&instruction.args, &argument0);
	if (e) goto cleanup;

	e = dl_array_pushElement(&instruction.args, &argument1);
	if (e) goto cleanup;

	e = dl_array_pushElement(&instruction.args, &argument2);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ incrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_unaryStackOperator(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            dl_array_t *assembly,
                                            duckLisp_instructionClass_t instructionClass,
                                            dl_ptrdiff_t index) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = getLocalsLength(compileState) - index;
	return duckLisp_emit_unaryOperator(duckLisp,
	                                   compileState,
	                                   assembly,
	                                   instructionClass,
	                                   argument);
}

dl_error_t duckLisp_emit_binaryStackOperator(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_instructionClass_t instructionClass,
                                             dl_ptrdiff_t index0,
                                             dl_ptrdiff_t index1) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - index0;
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = getLocalsLength(compileState) - index1;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    instructionClass,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_ternaryStackOperator(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              duckLisp_instructionClass_t instructionClass,
                                              dl_ptrdiff_t index0,
                                              dl_ptrdiff_t index1,
                                              dl_ptrdiff_t index2) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	duckLisp_instructionArgClass_t argument2 = {0};
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - index0;
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = getLocalsLength(compileState) - index1;
	argument2.type = duckLisp_instructionArgClass_type_index;
	argument2.value.index = getLocalsLength(compileState) - index2;
	return duckLisp_emit_ternaryOperator(duckLisp,
	                                     compileState,
	                                     assembly,
	                                     instructionClass,
	                                     argument0,
	                                     argument1,
	                                     argument2);
}


dl_error_t duckLisp_emit_nil(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly) {
	return duckLisp_emit_nullaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_nil);
}

dl_error_t duckLisp_emit_makeString(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_makeString,
	                                        source_index);
}

dl_error_t duckLisp_emit_concatenate(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     const dl_ptrdiff_t vec_index,
                                     const dl_ptrdiff_t index_index) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_concatenate,
	                                         vec_index,
	                                         index_index);
}

dl_error_t duckLisp_emit_substring(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t string_index,
                                   const dl_ptrdiff_t start_index_index,
                                   const dl_ptrdiff_t end_index_index) {
	return duckLisp_emit_ternaryStackOperator(duckLisp,
	                                          compileState,
	                                          assembly,
	                                          duckLisp_instructionClass_substring,
	                                          string_index,
	                                          start_index_index,
	                                          end_index_index);
}

dl_error_t duckLisp_emit_length(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_length,
	                                        source_index);
}

dl_error_t duckLisp_emit_symbolString(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_symbolString,
	                                        source_index);
}

dl_error_t duckLisp_emit_symbolId(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_symbolId,
	                                        source_index);
}

dl_error_t duckLisp_emit_typeof(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_typeof,
	                                        source_index);
}

dl_error_t duckLisp_emit_makeType(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly) {
	return duckLisp_emit_nullaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_makeType);
}

dl_error_t duckLisp_emit_makeInstance(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t type_index,
                                    const dl_ptrdiff_t value_index,
                                    const dl_ptrdiff_t function_index) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	duckLisp_instructionArgClass_t argument2 = {0};
	/* Type */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - type_index;
	/* Value slot */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = getLocalsLength(compileState) - value_index;
	/* Function slot */
	argument2.type = duckLisp_instructionArgClass_type_index;
	argument2.value.index = getLocalsLength(compileState) - function_index;
	return duckLisp_emit_ternaryOperator(duckLisp,
	                                     compileState,
	                                     assembly,
	                                     duckLisp_instructionClass_makeInstance,
	                                     argument0,
	                                     argument1,
	                                     argument2);
}

dl_error_t duckLisp_emit_compositeValue(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_compositeValue,
	                                        source_index);
}

dl_error_t duckLisp_emit_compositeFunction(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_compositeFunction,
	                                        source_index);
}

dl_error_t duckLisp_emit_setCompositeValue(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           const dl_ptrdiff_t destination_index,
                                           const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCompositeValue,
	                                      destination_index,
	                                      source_index);
	return e;
}

dl_error_t duckLisp_emit_setCompositeFunction(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              const dl_ptrdiff_t destination_index,
                                              const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCompositeFunction,
	                                      destination_index,
	                                      source_index);
	// I should probably make this operator return something.
	return e;
}

dl_error_t duckLisp_emit_nullp(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_nullp,
	                                        source_index);
}

dl_error_t duckLisp_emit_setCar(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t destination_index,
                                const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCar,
	                                      source_index,
	                                      destination_index);
	return e;
}

dl_error_t duckLisp_emit_setCdr(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t destination_index,
                                const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCdr,
	                                      source_index,
	                                      destination_index);
	return e;
}

dl_error_t duckLisp_emit_car(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_car,
	                                        source_index);
}

dl_error_t duckLisp_emit_cdr(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_cdr,
	                                        source_index);
}

dl_error_t duckLisp_emit_cons(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_cons,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_vector(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t *indexes,
                                const dl_size_t indexes_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_vector;

	/* Push arguments into instruction. */
	/* Length */
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = indexes_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto l_cleanup;

	/* Indices */
	DL_DOTIMES(i, indexes_length) {
		argument.type = duckLisp_instructionArgClass_type_index;
		argument.value.index = getLocalsLength(compileState) - indexes[i];
		e = dl_array_pushElement(&instruction.args, &argument);
		if (e) goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto l_cleanup;

	/**/ incrementLocalsLength(compileState);

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_makeVector(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t length_index,
                                    const dl_ptrdiff_t fill_index) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Length */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - length_index;
	/* Fill */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = getLocalsLength(compileState) - fill_index;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_makeVector,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_getVecElt(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t vec_index,
                                   const dl_ptrdiff_t index_index) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_getVecElt,
	                                         vec_index,
	                                         index_index);
}

dl_error_t duckLisp_emit_setVecElt(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t vec_index,
                                   const dl_ptrdiff_t index_index,
                                   const dl_ptrdiff_t value_index) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	duckLisp_instructionArgClass_t argument2 = {0};
	/* Vector */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - vec_index;
	/* Index */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = getLocalsLength(compileState) - index_index;
	/* Value */
	argument2.type = duckLisp_instructionArgClass_type_index;
	argument2.value.index = getLocalsLength(compileState) - value_index;
	return duckLisp_emit_ternaryOperator(duckLisp,
	                                     compileState,
	                                     assembly,
	                                     duckLisp_instructionClass_setVecElt,
	                                     argument0,
	                                     argument1,
	                                     argument2);
}

dl_error_t duckLisp_emit_return(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_size_t count) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_return;

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.index = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length -= count;

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pop(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_size_t count) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	if (count == 0) goto l_cleanup;

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pop;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.index = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	compileState->currentCompileState->locals_length -= count;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_greater(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 const dl_ptrdiff_t source_index1,
                                 const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                              compileState,
	                                              assembly,
	                                              duckLisp_instructionClass_greater,
	                                              source_index1,
	                                              source_index2);
}

dl_error_t duckLisp_emit_equal(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t source_index1,
                               const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                              compileState,
	                                              assembly,
	                                              duckLisp_instructionClass_equal,
	                                              source_index1,
	                                              source_index2);
}

dl_error_t duckLisp_emit_less(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2) {
        return duckLisp_emit_binaryStackOperator(duckLisp,
                                                      compileState,
                                                      assembly,
                                                      duckLisp_instructionClass_less,
                                                      source_index1,
                                                      source_index2);
}

dl_error_t duckLisp_emit_not(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t index) {
	return duckLisp_emit_unaryStackOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_not, index);
}

dl_error_t duckLisp_emit_multiply(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  const dl_ptrdiff_t source_index1,
                                  const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_mul,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_divide(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index1,
                                const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_div,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_add(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_add,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_sub(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_sub,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_nop(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly) {
	/**/ decrementLocalsLength(compileState);
	return duckLisp_emit_nullaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_nop);
}

dl_error_t duckLisp_emit_setStatic(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t destination_static_index,
                                   const dl_ptrdiff_t source_stack_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};

	if (destination_static_index == source_stack_index) goto cleanup;

	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - source_stack_index;
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = destination_static_index;
	e = duckLisp_emit_binaryOperator(duckLisp,
	                                 compileState,
	                                 assembly,
	                                 duckLisp_instructionClass_setStatic,
	                                 argument0,
	                                 argument1);
	decrementLocalsLength(compileState);
 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushGlobal(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t global_key) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = global_key;
	return duckLisp_emit_unaryOperator(duckLisp,
	                                   compileState,
	                                   assembly,
	                                   duckLisp_instructionClass_pushGlobal,
	                                   argument);
}

dl_error_t duckLisp_emit_move(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t destination_index,
                              const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;

	if (destination_index == source_index) goto cleanup;

	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_move,
	                                      source_index,
	                                      destination_index);
	if (e) goto cleanup;
	/**/ decrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushBoolean(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = integer;
	e = duckLisp_emit_unaryOperator(duckLisp,
	                                compileState,
	                                assembly,
	                                duckLisp_instructionClass_pushBoolean,
	                                argument);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = getLocalsLength(compileState) - 1;

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = integer;
	e = duckLisp_emit_unaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_pushInteger, argument);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = getLocalsLength(compileState) - 1;

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushDoubleFloat(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         dl_ptrdiff_t *stackIndex,
                                         const double doubleFloat) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_doubleFloat;
	argument.value.doubleFloat = doubleFloat;
	e = duckLisp_emit_unaryOperator(duckLisp,
	                                compileState,
	                                assembly,
	                                duckLisp_instructionClass_pushDoubleFloat,
	                                argument);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = getLocalsLength(compileState) - 1;

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushString(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    dl_ptrdiff_t *stackIndex,
                                    char *string,
                                    dl_size_t string_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_pushString;

	/* Write string length. */

	if (string_length > DL_UINT16_MAX) {
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    DL_STR("String longer than DL_UINT_MAX. Truncating string to fit."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		string_length = DL_UINT16_MAX;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	if (string_length) {
		e = dl_malloc(duckLisp->memoryAllocation, (void **) &argument.value.string.value, string_length * sizeof(char));
		if (e) goto cleanup;
		/**/ dl_memcopy_noOverlap(argument.value.string.value, string, string_length);
	}
	else {
		argument.value.string.value = dl_null;
	}

	argument.type = duckLisp_instructionArgClass_type_string;
	argument.value.string.value_length = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = getLocalsLength(compileState);
	incrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushSymbol(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    dl_ptrdiff_t *stackIndex,
                                    dl_size_t id,
                                    char *string,
                                    dl_size_t string_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushSymbol;

	// Write string length.

	if (string_length > DL_UINT16_MAX) {
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    DL_STR("String longer than DL_UINT_MAX. Truncating string to fit."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		string_length = DL_UINT16_MAX;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = id;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &argument.value.string.value, string_length * sizeof(char));
	if (e) goto cleanup;
	/**/ dl_memcopy_noOverlap(argument.value.string.value, string, string_length);

	argument.type = duckLisp_instructionArgClass_type_string;
	argument.value.string.value_length = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = getLocalsLength(compileState);
	incrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushClosure(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_bool_t variadic,
                                     const dl_ptrdiff_t function_label_index,
                                     const dl_size_t arity,
                                     const dl_ptrdiff_t *captures,
                                     const dl_size_t captures_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = (variadic
	                                ? duckLisp_instructionClass_pushVaClosure
	                                : duckLisp_instructionClass_pushClosure);

	// Push arguments into instruction.

	// Function label
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = function_label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Arity
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = arity;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Captures
	argument.type = duckLisp_instructionArgClass_type_integer;
	DL_DOTIMES(i, captures_length) {
		argument.value.integer = (captures[i] >= 0
		                          ? ((dl_ptrdiff_t) getLocalsLength(compileState) - captures[i])
		                          : captures[i]);
		e = dl_array_pushElement(&instruction.args, &argument);
		if (e) goto cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = getLocalsLength(compileState);
	incrementLocalsLength(compileState);

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_releaseUpvalues(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         const dl_ptrdiff_t *upvalues,
                                         const dl_size_t upvalues_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_releaseUpvalues;

	// Push arguments into instruction.

	// Upvalues
	argument.type = duckLisp_instructionArgClass_type_integer;
	{
		dl_size_t num_objects = 0;
		DL_DOTIMES(i, upvalues_length) if (upvalues[i] >= 0) num_objects++;
		if (num_objects == 0) goto cleanup;
	}
	DL_DOTIMES(i, upvalues_length) {
		if (upvalues[i] < 0) continue;
		argument.value.integer = getLocalsLength(compileState) - upvalues[i];
		e = dl_array_pushElement(&instruction.args, &argument);
		if (e) goto cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto cleanup;
	}

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_ccall(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               dl_ptrdiff_t callback_index) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = callback_index;
	return duckLisp_emit_unaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_ccall, argument);
}

dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_pushIndex,
	                                        index);
}

dl_error_t duckLisp_emit_pushUpvalue(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     const dl_ptrdiff_t index) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = index;
	return duckLisp_emit_unaryOperator(duckLisp,
	                                   compileState,
	                                   assembly,
	                                   duckLisp_instructionClass_pushUpvalue,
	                                   argument);
}

dl_error_t duckLisp_emit_setUpvalue(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t upvalueIndex,
                                    const dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Upvalue */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = upvalueIndex;
	/* Object */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = getLocalsLength(compileState) - index;
	e = duckLisp_emit_binaryOperator(duckLisp,
	                                 compileState,
	                                 assembly,
	                                 duckLisp_instructionClass_setUpvalue,
	                                 argument0,
	                                 argument1);
	if (e) goto cleanup;
	decrementLocalsLength(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_funcall(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 dl_ptrdiff_t index,
                                 dl_uint8_t arity) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Function index. */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - index;
	/* Arity */
	argument1.type = duckLisp_instructionArgClass_type_integer;
	argument1.value.integer = arity;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_funcall,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_apply(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               dl_ptrdiff_t index,
                               dl_uint8_t arity) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Function index. */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = getLocalsLength(compileState) - index;
	/* Arity */
	argument1.type = duckLisp_instructionArgClass_type_integer;
	argument1.value.integer = arity;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_apply,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_acall(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t function_index,
                               const dl_size_t count) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	argument1.type = duckLisp_instructionArgClass_type_integer;
	argument1.value.integer = getLocalsLength(compileState) - function_index;
	argument0.type = duckLisp_instructionArgClass_type_integer;
	argument0.value.integer = count;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_acall,
	                                    argument0,
	                                    argument1);
}

/* We do label scoping in the emitters because scope will have no meaning during assembly. */

dl_error_t duckLisp_emit_call(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              const dl_size_t label_length,
                              const dl_size_t count) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_call;

	/* `label_index` should never equal -1 after this function exits. */
	e = scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Call references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_brz(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             char *label,
                             dl_size_t label_length,
                             int pops) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_brz;

	if (pops < 0) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("brz: Cannot pop a negative number of objects."));
		if (eError) e = eError;
		goto cleanup;
	}

	/* `label_index` should never equal -1 after this function exits. */
	e = scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("brz references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = pops;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length -= pops;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_brnz(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length,
                              int pops) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_brnz;

	if (pops < 0) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("brnz: Cannot pop a negative number of objects."));
		if (eError) e = eError;
		goto cleanup;
	}

	/* `label_index` should never equal -1 after this function exits. */
	e = scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("brnz references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = pops;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length -= pops;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_jump(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_jump;

	/* `label_index` should never equal -1 after this function exits. */
	e = scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Goto references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_label(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               char *label,
                               dl_size_t label_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_scope_t scope;
	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_pseudo_label;

	/* This is why we pushed the scope here. */
	e = scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	/* Make sure label is declared. */
	/**/ dl_trie_find(scope.labels_trie, &label_index, label, label_length);
	if (e) goto cleanup;
	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\" is not a top-level expression in a closed scope."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

/*
  ==========
  Generators
  ==========
*/

static dl_error_t assembly_quit(duckLisp_t *duckLisp, dl_array_t *assembly);

    /* `gensym` creates a label that is unlikely to ever be used. */
dl_error_t duckLisp_gensym(duckLisp_t *duckLisp, duckLisp_ast_identifier_t *identifier) {
	dl_error_t e = dl_error_ok;

	identifier->value_length = 1 + 8/4*sizeof(dl_size_t);  // This is dependent on the size of the gensym number.
	e = DL_MALLOC(duckLisp->memoryAllocation, (void **) &identifier->value, identifier->value_length, char);
	if (e) {
		identifier->value_length = 0;
		return dl_error_outOfMemory;
	}
	identifier->value[0] = '\0';  /* Surely not even an idiot would start a string with a null char. */
	DL_DOTIMES(i, 8/4*sizeof(dl_size_t)) {
		identifier->value[i + 1] = dl_nybbleToHexChar((duckLisp->gensym_number >> 4*i) & 0xF);
	}
	duckLisp->gensym_number++;
	return e;
}

dl_error_t duckLisp_register_label(duckLisp_t *duckLisp,
                                   duckLisp_subCompileState_t *subCompileState,
                                   char *name,
                                   const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	e = scope_getTop(duckLisp, subCompileState, &scope);
	if (e) goto l_cleanup;

	e = dl_trie_insert(&scope.labels_trie, name, name_length, subCompileState->label_number);
	if (e) goto l_cleanup;
	subCompileState->label_number++;

	e = scope_setTop(subCompileState, &scope);
	if (e) goto l_cleanup;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_generator_nullaryArithmeticOperator(duckLisp_t *duckLisp,
                                                        duckLisp_compileState_t *compileState,
                                                        dl_array_t *assembly,
                                                        duckLisp_ast_expression_t *expression,
                                                        dl_error_t (*emitter)(duckLisp_t *,
                                                                              duckLisp_compileState_t *,
                                                                              dl_array_t *)) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 1, dl_false);
	if (e) goto cleanup;

	e = emitter(duckLisp, compileState, assembly);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_unaryArithmeticOperator(duckLisp_t *duckLisp,
                                                      duckLisp_compileState_t *compileState,
                                                      dl_array_t *assembly,
                                                      duckLisp_ast_expression_t *expression,
                                                      dl_error_t (*emitter)(duckLisp_t *,
                                                                            duckLisp_compileState_t *,
                                                                            dl_array_t *,
                                                                            dl_ptrdiff_t)) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2, dl_false);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &args_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;

	e = emitter(duckLisp, compileState, assembly, args_index);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_binaryArithmeticOperator(duckLisp_t *duckLisp,
                                                       duckLisp_compileState_t *compileState,
                                                       dl_array_t *assembly,
                                                       duckLisp_ast_expression_t *expression,
                                                       dl_error_t (*emitter)(duckLisp_t *,
                                                                             duckLisp_compileState_t *,
                                                                             dl_array_t *,
                                                                             dl_ptrdiff_t,
                                                                             dl_ptrdiff_t)) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t destination_index;
	dl_ptrdiff_t source_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_false);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &destination_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[2],
	                                        &source_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;

	e = emitter(duckLisp, compileState, assembly, destination_index, source_index);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_ternaryArithmeticOperator(duckLisp_t *duckLisp,
                                                        duckLisp_compileState_t *compileState,
                                                        dl_array_t *assembly,
                                                        duckLisp_ast_expression_t *expression,
                                                        dl_error_t (*emitter)(duckLisp_t *,
                                                                              duckLisp_compileState_t *,
                                                                              dl_array_t *,
                                                                              dl_ptrdiff_t,
                                                                              dl_ptrdiff_t,
                                                                              dl_ptrdiff_t)) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t first_index;
	dl_ptrdiff_t second_index;
	dl_ptrdiff_t third_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 4, dl_false);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &first_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[2],
	                                        &second_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[3],
	                                        &third_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;

	e = emitter(duckLisp, compileState, assembly, first_index, second_index, third_index);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_makeString(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_makeString);
}

dl_error_t duckLisp_generator_concatenate(duckLisp_t *duckLisp,
                                          duckLisp_compileState_t *compileState,
                                          dl_array_t *assembly,
                                          duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_concatenate);
}

dl_error_t duckLisp_generator_substring(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_ternaryArithmeticOperator(duckLisp,
	                                                    compileState,
	                                                    assembly,
	                                                    expression,
	                                                    duckLisp_emit_substring);
}

dl_error_t duckLisp_generator_length(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_length);
}

dl_error_t duckLisp_generator_symbolString(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_symbolString);
}

dl_error_t duckLisp_generator_symbolId(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_symbolId);
}

dl_error_t duckLisp_generator_typeof(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_typeof);
}

dl_error_t duckLisp_generator_makeType(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_nullaryArithmeticOperator(duckLisp,
	                                                    compileState,
	                                                    assembly,
	                                                    expression,
	                                                    duckLisp_emit_makeType);
}

dl_error_t duckLisp_generator_makeInstance(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_ternaryArithmeticOperator(duckLisp,
	                                                    compileState,
	                                                    assembly,
	                                                    expression,
	                                                    duckLisp_emit_makeInstance);
}

dl_error_t duckLisp_generator_compositeValue(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_compositeValue);
}

dl_error_t duckLisp_generator_compositeFunction(duckLisp_t *duckLisp,
                                                duckLisp_compileState_t *compileState,
                                                dl_array_t *assembly,
                                                duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_compositeFunction);
}

dl_error_t duckLisp_generator_setCompositeValue(duckLisp_t *duckLisp,
                                                duckLisp_compileState_t *compileState,
                                                dl_array_t *assembly,
                                                duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_setCompositeValue);
}

dl_error_t duckLisp_generator_setCompositeFunction(duckLisp_t *duckLisp,
                                                   duckLisp_compileState_t *compileState,
                                                   dl_array_t *assembly,
                                                   duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_setCompositeFunction);
}

dl_error_t duckLisp_generator_nullp(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_nullp);
}

dl_error_t duckLisp_generator_setCar(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_setCar);
}

dl_error_t duckLisp_generator_setCdr(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_setCdr);
}

dl_error_t duckLisp_generator_car(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_car);
}

dl_error_t duckLisp_generator_cdr(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_cdr);
}

dl_error_t duckLisp_generator_cons(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_cons);
}

dl_error_t duckLisp_generator_list(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;
	dl_ptrdiff_t cons_index;
	e = duckLisp_emit_nil(duckLisp, compileState, assembly);
	if (e) goto cleanup;
	cons_index = getLocalsLength(compileState) - 1;

	DL_DOTIMES(i, expression->compoundExpressions_length - 1) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[expression->compoundExpressions_length
		                                                                         - i
		                                                                         - 1],
		                                        &args_index,
		                                        dl_null,
		                                        dl_false);
		if (e) goto cleanup;
		e = duckLisp_emit_cons(duckLisp, compileState, assembly, args_index, cons_index);
		if (e) goto cleanup;
		cons_index = getLocalsLength(compileState) - 1;
	}

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_vector(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t *args_indexes = dl_null;
	e = duckLisp_emit_nil(duckLisp, compileState, assembly);
	if (e) goto cleanup;

	/* For this one, we will need to save the indices. */
	if (expression->compoundExpressions_length > 1) {
		e = DL_MALLOC(duckLisp->memoryAllocation,
		              &args_indexes,
		              expression->compoundExpressions_length - 1,
		              dl_ptrdiff_t);
		if (e) goto cleanup;
	}

	DL_DOTIMES(i, expression->compoundExpressions_length - 1) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[expression->compoundExpressions_length
		                                                                         - i
		                                                                         - 1],
		                                        &args_indexes[expression->compoundExpressions_length - 2 - i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto cleanupIndices;
	}
	e = duckLisp_emit_vector(duckLisp,
	                         compileState,
	                         assembly,
	                         args_indexes,
	                         expression->compoundExpressions_length - 1);
	if (e) goto cleanupIndices;

 cleanupIndices:
	if (expression->compoundExpressions_length > 1) {
		eError = DL_FREE(duckLisp->memoryAllocation, &args_indexes);
		if (!e) e = eError;
	}

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_makeVector(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_makeVector);
}

dl_error_t duckLisp_generator_getVecElt(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_getVecElt);
}

dl_error_t duckLisp_generator_setVecElt(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression) {
	(void) compileState;
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t vec_index;
	dl_ptrdiff_t index_index;
	dl_ptrdiff_t value_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 4, dl_false);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &vec_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;
	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[2],
	                                        &index_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;
	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[3],
	                                        &value_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanup;
	e = duckLisp_emit_setVecElt(duckLisp, compileState, assembly, vec_index, index_index, value_index);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_quote_helper(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           dl_ptrdiff_t *stack_index,
                                           duckLisp_ast_compoundExpression_t *tree) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_array_t tempString;
	dl_ptrdiff_t temp_index;
	/**/ dl_array_init(&tempString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	duckLisp_ast_identifier_t tempIdentifier;

	/*
	  Recursively convert to a tree made of lists.
	*/

	switch (tree->type) {
	case duckLisp_ast_type_bool:
		e = duckLisp_emit_pushBoolean(duckLisp, compileState, assembly, &temp_index, tree->value.boolean.value);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_int:
		e = duckLisp_emit_pushInteger(duckLisp, compileState, assembly, &temp_index, tree->value.integer.value);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_float:
		e = duckLisp_emit_pushDoubleFloat(duckLisp,
		                                  compileState,
		                                  assembly,
		                                  &temp_index,
		                                  tree->value.floatingPoint.value);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_string:
		e = duckLisp_emit_pushString(duckLisp,
		                             compileState,
		                             assembly,
		                             stack_index,
		                             tree->value.string.value,
		                             tree->value.string.value_length);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_identifier:
		/* Check if symbol is interned */
		/**/ dl_trie_find(duckLisp->symbols_trie,
		                  &temp_index,
		                  tree->value.identifier.value,
		                  tree->value.identifier.value_length);
		if (temp_index < 0) {
			/* It's not. Intern it. */
			temp_index = duckLisp->symbols_array.elements_length;
			e = dl_trie_insert(&duckLisp->symbols_trie,
			                   tree->value.identifier.value,
			                   tree->value.identifier.value_length,
			                   duckLisp->symbols_array.elements_length);
			if (e) goto cleanup;
			tempIdentifier.value_length = tree->value.identifier.value_length;
			e = dl_malloc(duckLisp->memoryAllocation, (void **) &tempIdentifier.value, tempIdentifier.value_length);
			if (e) goto cleanup;
			/**/ dl_memcopy_noOverlap(tempIdentifier.value, tree->value.identifier.value, tempIdentifier.value_length);
			e = dl_array_pushElement(&duckLisp->symbols_array, (void *) &tempIdentifier);
			if (e) goto cleanup;
		}
		/* Push symbol */
		e = duckLisp_emit_pushSymbol(duckLisp,
		                             compileState,
		                             assembly,
		                             stack_index,
		                             temp_index,
		                             tree->value.identifier.value,
		                             tree->value.identifier.value_length);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_expression:
		if (tree->value.expression.compoundExpressions_length > 0) {
			dl_ptrdiff_t last_temp_index;
			e = duckLisp_emit_nil(duckLisp, compileState, assembly);
			if (e) goto cleanup;
			last_temp_index = getLocalsLength(compileState) - 1;
			for (dl_ptrdiff_t j = tree->value.expression.compoundExpressions_length - 1; j >= 0; --j) {
				e = duckLisp_generator_quote_helper(duckLisp,
				                                    compileState,
				                                    assembly,
				                                    &temp_index,
				                                    &tree->value.expression.compoundExpressions[j]);
				if (e) goto cleanup;
				e = duckLisp_emit_cons(duckLisp,
				                       compileState,
				                       assembly,
				                       getLocalsLength(compileState) - 1,
				                       last_temp_index);
				if (e) goto cleanup;
				last_temp_index = getLocalsLength(compileState) - 1;
			}
			*stack_index = getLocalsLength(compileState) - 1;
		}
		else {
			e = duckLisp_emit_nil(duckLisp, compileState, assembly);
			if (e) goto cleanup;
			*stack_index = getLocalsLength(compileState) - 1;
		}
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("quote"));
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

 cleanup:

	eError = dl_array_quit(&tempString);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_quote(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_array_t tempString;
	/**/ dl_array_init(&tempString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	duckLisp_ast_compoundExpression_t *tree = &expression->compoundExpressions[1];
	dl_ptrdiff_t temp_index = -1;
	char *functionName = expression->compoundExpressions[0].value.identifier.value;
	dl_size_t functionName_length = expression->compoundExpressions[0].value.identifier.value_length;
	duckLisp_ast_identifier_t tempIdentifier;

	/*
	  Recursively convert to a tree made of lists.
	*/

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2, dl_false);
	if (e) goto cleanup;

	switch (tree->type) {
	case duckLisp_ast_type_bool:
		e = duckLisp_emit_pushBoolean(duckLisp, compileState, assembly, &temp_index, tree->value.boolean.value);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_int:
		e = duckLisp_emit_pushInteger(duckLisp, compileState, assembly, &temp_index, tree->value.integer.value);
		if (e) goto cleanup;
		break;
		case duckLisp_ast_type_float:
			e = duckLisp_emit_pushDoubleFloat(duckLisp,
			                                  compileState,
			                                  assembly,
			                                  &temp_index,
			                                  tree->value.floatingPoint.value);
			if (e) goto cleanup;
			break;
	case duckLisp_ast_type_string:
		e = duckLisp_emit_pushString(duckLisp,
		                             compileState,
		                             assembly,
		                             &temp_index,
		                             tree->value.string.value,
		                             tree->value.string.value_length);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_identifier:
		// Check if symbol is interned
		/**/ dl_trie_find(duckLisp->symbols_trie,
		                  &temp_index, tree->value.identifier.value,
		                  tree->value.identifier.value_length);
		if (temp_index < 0) {
			// It's not. Intern it.
			temp_index = duckLisp->symbols_array.elements_length;
			e = dl_trie_insert(&duckLisp->symbols_trie,
			                   tree->value.identifier.value,
			                   tree->value.identifier.value_length,
			                   duckLisp->symbols_array.elements_length);
			if (e) goto cleanup;
			tempIdentifier.value_length = tree->value.identifier.value_length;
			e = dl_malloc(duckLisp->memoryAllocation, (void **) &tempIdentifier.value, tempIdentifier.value_length);
			if (e) goto cleanup;
			/**/ dl_memcopy_noOverlap(tempIdentifier.value, tree->value.identifier.value, tempIdentifier.value_length);
			e = dl_array_pushElement(&duckLisp->symbols_array, (void *) &tempIdentifier);
			if (e) goto cleanup;
		}
		// Push symbol
		e = duckLisp_emit_pushSymbol(duckLisp,
		                             compileState,
		                             assembly,
		                             dl_null,
		                             temp_index,
		                             tree->value.identifier.value,
		                             tree->value.identifier.value_length);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_expression:
		if (tree->value.expression.compoundExpressions_length > 0) {
			dl_ptrdiff_t last_temp_index;
			e = duckLisp_emit_nil(duckLisp, compileState, assembly);
			if (e) goto cleanup;
			last_temp_index = getLocalsLength(compileState) - 1;
			for (dl_ptrdiff_t j = tree->value.expression.compoundExpressions_length - 1; j >= 0; --j) {
				e = duckLisp_generator_quote_helper(duckLisp,
				                                    compileState,
				                                    assembly,
				                                    &temp_index,
				                                    &tree->value.expression.compoundExpressions[j]);
				if (e) goto cleanup;
				e = duckLisp_emit_cons(duckLisp,
				                       compileState,
				                       assembly,
				                       getLocalsLength(compileState) - 1,
				                       last_temp_index);
				if (e) goto cleanup;
				last_temp_index = getLocalsLength(compileState) - 1;
			}
		}
		else {
			e = duckLisp_emit_nil(duckLisp, compileState, assembly);
			if (e) goto cleanup;
		}
		break;
	default:
		e = dl_array_pushElements(&eString, functionName, functionName_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

 cleanup:

	eError = dl_array_quit(&tempString);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_noscope(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_size_t startStack_length;
	dl_bool_t foundVar = dl_false;
	dl_bool_t foundDefun = dl_false;
	dl_bool_t foundInclude = dl_false;
	dl_bool_t foundNoscope = dl_false;
	dl_bool_t foundMacro = dl_false;
	dl_ptrdiff_t pops = 0;

	/* Compile */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		duckLisp_ast_compoundExpression_t currentExpression = expression->compoundExpressions[i];
		startStack_length = getLocalsLength(compileState);
		foundDefun = dl_false;
		foundVar = dl_false;
		foundInclude = dl_false;
		foundNoscope = dl_false;
		foundMacro = dl_false;
		if ((currentExpression.type == duckLisp_ast_type_expression)
		    && (currentExpression.value.expression.compoundExpressions_length > 0)
		    && (currentExpression.value.expression.compoundExpressions[0].type == duckLisp_ast_type_identifier)) {
			dl_string_compare(&foundVar,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length,
			                  DL_STR("__var"));
			dl_string_compare(&foundDefun,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length,
			                  DL_STR("__defun"));
			// `include` is an exception because the included file exists in the parent scope.
			dl_string_compare(&foundInclude,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length,
			                  DL_STR("include"));
			dl_string_compare(&foundNoscope,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length,
			                  DL_STR("__noscope"));
			{
				duckLisp_functionType_t functionType = duckLisp_functionType_none;
				dl_ptrdiff_t functionIndex = -1;
				e = scope_getFunctionFromName(duckLisp,
				                              compileState->currentCompileState,
				                              &functionType,
				                              &functionIndex,
				                              (currentExpression.value.expression.compoundExpressions[0]
				                               .value.identifier.value),
				                              (currentExpression.value.expression.compoundExpressions[0]
				                               .value.identifier.value_length));
				if (e) goto cleanup;
				foundMacro = (functionType == duckLisp_functionType_macro);
			}
		}
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        DL_STR("noscope"),
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;
		if (!(foundInclude
		      || foundNoscope
		      || foundMacro
		      || foundVar
		      || foundDefun)) {
			pops = (getLocalsLength(compileState)
			        - startStack_length
			        - ((dl_size_t) i == expression->compoundExpressions_length - 1));
			if (pops > 0) {
				if ((dl_size_t) i == expression->compoundExpressions_length - 1) {
					e = duckLisp_emit_move(duckLisp,
					                       compileState,
					                       assembly,
					                       getLocalsLength(compileState) - 1 - pops,
					                       getLocalsLength(compileState) - 1);
					if (e) goto cleanup;
				}
				e = duckLisp_emit_pop(duckLisp, compileState, assembly, pops);
				if (e) goto cleanup;
			}
			else if (pops < 0) {
				DL_DOTIMES(l, (dl_size_t) (-pops)) {
					e = duckLisp_emit_pushIndex(duckLisp, compileState, assembly, getLocalsLength(compileState) - 1);
					if (e) goto cleanup;
				}
			}
		}
		else if ((foundNoscope || foundMacro || foundVar || foundDefun)
		         && !((dl_size_t) i == expression->compoundExpressions_length - 1)) {
			e = duckLisp_emit_pop(duckLisp, compileState, assembly, 1);
			if (e) goto cleanup;
		}
		foundInclude = dl_false;
		foundNoscope = dl_false;
		foundMacro = dl_false;
	}
	if (expression->compoundExpressions_length == 0) e = duckLisp_emit_nil(duckLisp, compileState, assembly);

 cleanup:
	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_noscope2(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	duckLisp_ast_expression_t subExpression;
	subExpression.compoundExpressions = &expression->compoundExpressions[1];
	subExpression.compoundExpressions_length = expression->compoundExpressions_length - 1;
	return duckLisp_generator_noscope(duckLisp, compileState, assembly, &subExpression);
}

dl_size_t duckLisp_consList_length(duckVM_object_t *cons) {
	dl_size_t length = 0;
	while (cons != dl_null) {
		if (cons->value.cons.cdr == dl_null) {
			cons = dl_null;
			length++;
		}
		else if (cons->value.cons.cdr->type == duckVM_object_type_list) {
			cons = cons->value.cons.cdr->value.list;
			length++;
		}
		else if (cons->value.cons.cdr->type == duckVM_object_type_cons) {
			cons = cons->value.cons.cdr;
			length++;
		}
		else {
			cons = dl_null;
		}
	}
	return length;
}

dl_error_t duckLisp_consToExprAST(duckLisp_t *duckLisp,
                                  duckLisp_ast_compoundExpression_t *ast,
                                  duckVM_object_t *cons) {
	dl_error_t e = dl_error_ok;

	/* (cons a b) */

	if (cons != dl_null) {
		const dl_size_t length = duckLisp_consList_length(cons);
		e = DL_MALLOC(duckLisp->memoryAllocation,
		              (void **) &ast->value.expression.compoundExpressions,
		              length,
		              duckLisp_ast_compoundExpression_t);
		if (e) goto cleanup;
		ast->value.expression.compoundExpressions_length = length;
		ast->type = duckLisp_ast_type_expression;
		dl_ptrdiff_t j = 0;
		while (cons != dl_null) {
			if (cons->value.cons.car == dl_null) {
				e = duckLisp_consToExprAST(duckLisp,
				                           &ast->value.expression.compoundExpressions[j],
				                           cons->value.cons.car);
				if (e) goto cleanup;
			}
			else if (cons->value.cons.car->type == duckVM_object_type_cons) {
				e = duckLisp_consToExprAST(duckLisp,
				                           &ast->value.expression.compoundExpressions[j],
				                           cons->value.cons.car);
				if (e) goto cleanup;
			}
			else {
				e = duckLisp_objectToAST(duckLisp,
				                         &ast->value.expression.compoundExpressions[j],
				                         cons->value.cons.car,
				                         dl_true);
				if (e) goto cleanup;
			}
			if (cons->value.cons.cdr == dl_null) {
				cons = cons->value.cons.cdr;
				j++;
			}
			else if (cons->value.cons.cdr->type == duckVM_object_type_cons) {
				cons = cons->value.cons.cdr;
				j++;
			}
			else if (cons->value.cons.cdr->type == duckVM_object_type_list) {
				cons = cons->value.cons.cdr->value.list;
				j++;
			}
			else {
				duckLisp_objectToAST(duckLisp,
				                     &ast->value.expression.compoundExpressions[j],
				                     cons->value.cons.cdr,
				                     dl_true);
				cons = dl_null;
			}
		}
	}
	else {
		ast->value.expression.compoundExpressions = dl_null;
		ast->value.expression.compoundExpressions_length = 0;
		ast->type = duckLisp_ast_type_expression;
	}

 cleanup:
	return e;
}

dl_error_t duckLisp_consToConsAST(duckLisp_t *duckLisp,
                                  duckLisp_ast_compoundExpression_t *ast,
                                  duckVM_object_t *cons) {
	dl_error_t e = dl_error_ok;

	/* (cons a b) */

	if (cons != dl_null) {
		const dl_uint8_t op = 0;
		const dl_uint8_t car = 1;
		const dl_uint8_t cdr = 2;

		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &ast->value.expression.compoundExpressions,
		              3 * sizeof(duckLisp_ast_compoundExpression_t));
		if (e) goto cleanup;
		ast->value.expression.compoundExpressions_length = 3;
		ast->type = duckLisp_ast_type_expression;

		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &ast->value.expression.compoundExpressions[op].value.identifier.value,
		              sizeof("__cons") - 1);
		if (e) goto cleanup;
		/**/ dl_memcopy_noOverlap(ast->value.expression.compoundExpressions[op].value.identifier.value,
		                          DL_STR("__cons"));
		ast->value.expression.compoundExpressions[op].value.identifier.value_length = sizeof("__cons") - 1;
		ast->value.expression.compoundExpressions[op].type = duckLisp_ast_type_identifier;

		if ((cons->value.cons.car == dl_null) || (cons->value.cons.car->type == duckVM_object_type_cons)) {
			e = duckLisp_consToConsAST(duckLisp, &ast->value.expression.compoundExpressions[car], cons->value.cons.car);
			if (e) goto cleanup;
		}
		else {
			e = duckLisp_objectToAST(duckLisp,
			                         &ast->value.expression.compoundExpressions[car],
			                         cons->value.cons.car,
			                         dl_false);
			if (e) goto cleanup;
		}
		if ((cons->value.cons.cdr == dl_null) || (cons->value.cons.cdr->type == duckVM_object_type_cons)) {
			e = duckLisp_consToConsAST(duckLisp, &ast->value.expression.compoundExpressions[cdr], cons->value.cons.cdr);
			if (e) goto cleanup;
		}
		else {
			e = duckLisp_objectToAST(duckLisp,
			                         &ast->value.expression.compoundExpressions[cdr],
			                         cons->value.cons.cdr,
			                         dl_false);
			if (e) goto cleanup;
		}
	}
	else {
		ast->value.expression.compoundExpressions = dl_null;
		ast->value.expression.compoundExpressions_length = 0;
		ast->type = duckLisp_ast_type_expression;
	}

 cleanup:
	return e;
}

dl_error_t duckLisp_objectToAST(duckLisp_t *duckLisp,
                                duckLisp_ast_compoundExpression_t *ast,
                                duckVM_object_t *object,
                                dl_bool_t useExprs) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	switch (object->type) {
	case duckVM_object_type_bool:
		ast->value.boolean.value = object->value.boolean;
		ast->type = duckLisp_ast_type_bool;
		break;
	case duckVM_object_type_integer:
		ast->value.integer.value = object->value.integer;
		ast->type = duckLisp_ast_type_int;
		break;
	case duckVM_object_type_float:
		ast->value.floatingPoint.value = object->value.floatingPoint;
		ast->type = duckLisp_ast_type_float;
		break;
	case duckVM_object_type_string:
		ast->value.string.value_length = object->value.string.length - object->value.string.offset;
		if (object->value.string.length - object->value.string.offset) {
			e = dl_malloc(duckLisp->memoryAllocation,
			              (void **) &ast->value.string.value,
			              object->value.string.length - object->value.string.offset);
			if (e) break;
			/**/ dl_memcopy_noOverlap(ast->value.string.value,
			                          (object->value.string.internalString->value.internalString.value
			                           + object->value.string.offset),
			                          object->value.string.length - object->value.string.offset);
		}
		else {
			ast->value.string.value = dl_null;
		}
		ast->type = duckLisp_ast_type_string;
		break;
	case duckVM_object_type_list:
		if (useExprs) {
			e = duckLisp_consToExprAST(duckLisp, ast, object->value.list);
		}
		else {
			e = duckLisp_consToConsAST(duckLisp, ast, object->value.list);
		}
		break;
	case duckVM_object_type_symbol:
		ast->value.identifier.value_length = object->value.symbol.internalString->value.internalString.value_length;
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &ast->value.identifier.value,
		              object->value.symbol.internalString->value.internalString.value_length);
		if (e) break;
		/**/ dl_memcopy_noOverlap(ast->value.identifier.value,
		                          object->value.symbol.internalString->value.internalString.value,
		                          object->value.symbol.internalString->value.internalString.value_length);
		ast->type = duckLisp_ast_type_identifier;
		break;
	case duckVM_object_type_function:
		e = dl_error_invalidValue;
		break;
	case duckVM_object_type_closure:
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    DL_STR("objectToAST: Attempted to convert closure to expression."));
		if (!e) e = eError;
		break;
	case duckVM_object_type_type:
		ast->value.integer.value = object->value.type;
		ast->type = duckLisp_ast_type_int;
		break;
	default:
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("objectToAST: Illegal object type."));
		if (!e) e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_comptime(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_array_t compAssembly;  /* dl_array_t:duckLisp_instructionObject_t */
	/**/ dl_array_init(&compAssembly,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionObject_t),
	                   dl_array_strategy_double);
	dl_array_t bytecode;  /* dl_array_t:dl_uint8_t */
	/**/ dl_array_init(&bytecode,
	                   duckLisp->memoryAllocation,
	                   sizeof(dl_uint8_t),
	                   dl_array_strategy_double);
	duckVM_object_t returnValue;
	duckLisp_ast_expression_t subExpression;
	duckLisp_ast_compoundExpression_t returnCompoundExpression;
	dl_uint8_t yieldInstruction = duckLisp_instruction_yield;

	subExpression.compoundExpressions = &expression->compoundExpressions[1];
	subExpression.compoundExpressions_length = expression->compoundExpressions_length - 1;

	duckLisp_subCompileState_t *lastSubCompileState = compileState->currentCompileState;
	compileState->currentCompileState = &compileState->comptimeCompileState;
	e = duckLisp_generator_noscope(duckLisp, compileState, &compAssembly, &subExpression);
	if (e) goto cleanup;

	e = dl_array_pushElements(&compileState->currentCompileState->assembly,
	                          compAssembly.elements,
	                          compAssembly.elements_length);
	if (e) goto cleanup;

	e = duckLisp_assemble(duckLisp, compileState, &bytecode, &compileState->currentCompileState->assembly);
	if (e) goto cleanup;

	e = dl_array_pushElement(&bytecode, &yieldInstruction);
	if (e) goto cleanup;

	e = dl_array_popElements(&compileState->currentCompileState->assembly,
	                         dl_null,
	                         compileState->currentCompileState->assembly.elements_length);
	if (e) goto cleanup;

	/* puts(duckLisp_disassemble(duckLisp->memoryAllocation, bytecode.elements, bytecode.elements_length)); */

	e = duckVM_execute(&duckLisp->vm, &returnValue, bytecode.elements, bytecode.elements_length);
	eError = dl_array_pushElements(&duckLisp->errors,
	                               duckLisp->vm.errors.elements,
	                               duckLisp->vm.errors.elements_length);
	if (!e) e = eError;
	if (e) goto cleanup;
	e = dl_array_popElements(&duckLisp->vm.errors, dl_null, duckLisp->vm.errors.elements_length);
	if (e) goto cleanup;

	e = duckLisp_objectToAST(duckLisp, &returnCompoundExpression, &returnValue, dl_false);
	if (e) goto cleanup;

	e = duckVM_pop(&duckLisp->vm, dl_null);
	if (e) goto cleanup;
	/**/ decrementLocalsLength(compileState);

	compileState->currentCompileState = lastSubCompileState;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &returnCompoundExpression,
	                                        dl_null,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;

	e = ast_compoundExpression_quit(duckLisp, &returnCompoundExpression);
	if (e) goto cleanup;

 cleanup:
	compileState->currentCompileState = lastSubCompileState;

	eError = dl_array_quit(&compAssembly);
	if (!e) e = eError;

	eError = dl_array_quit(&bytecode);
	if (!e) e = eError;

	eError = dl_array_quit(&eString);
	if (!e) e = eError;

	return e;
}

dl_error_t duckLisp_generator_defmacro(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	(void) assembly;

	duckLisp_subCompileState_t *lastCompileState = compileState->currentCompileState;
	dl_array_t macroBytecode;
	duckLisp_instruction_t yieldInstruction = duckLisp_instruction_yield;
	/**/ dl_array_init(&macroBytecode, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 4, dl_true);
	if (e) goto cleanup;
	e = duckLisp_checkTypeAndReportError(duckLisp,
	                                     expression->compoundExpressions[0].value.identifier,
	                                     expression->compoundExpressions[0],
	                                     duckLisp_ast_type_identifier);
	if (e) goto cleanup;
	e = duckLisp_checkTypeAndReportError(duckLisp,
	                                     expression->compoundExpressions[0].value.identifier,
	                                     expression->compoundExpressions[1],
	                                     duckLisp_ast_type_identifier);
	if (e) goto cleanup;
	e = duckLisp_checkTypeAndReportError(duckLisp,
	                                     expression->compoundExpressions[0].value.identifier,
	                                     expression->compoundExpressions[2],
	                                     duckLisp_ast_type_expression);
	if (e) goto cleanup;

	/* Compile */

	compileState->currentCompileState = &compileState->comptimeCompileState;

	e = duckLisp_generator_defun(duckLisp, compileState, &compileState->comptimeCompileState.assembly, expression);
	if (e) goto cleanup;

	e = duckLisp_assemble(duckLisp, compileState, &macroBytecode, &compileState->comptimeCompileState.assembly);
	if (e) goto cleanup;

	e = dl_array_pushElement(&macroBytecode, &yieldInstruction);
	if (e) goto cleanup;

	e = dl_array_popElements(&compileState->comptimeCompileState.assembly,
	                         dl_null,
	                         compileState->comptimeCompileState.assembly.elements_length);
	if (e) goto cleanup;

	/* puts(duckLisp_disassemble(duckLisp->memoryAllocation, */
	/*                           macroBytecode.elements, */
	/*                           macroBytecode.elements_length)); */

	e = duckVM_execute(&duckLisp->vm, dl_null, macroBytecode.elements, macroBytecode.elements_length);
	eError = dl_array_pushElements(&duckLisp->errors,
	                               duckLisp->vm.errors.elements,
	                               duckLisp->vm.errors.elements_length);
	if (!e) e = eError;
	if (e) goto cleanup;
	e = dl_array_popElements(&duckLisp->vm.errors, dl_null, duckLisp->vm.errors.elements_length);
	if (e) goto cleanup;

	/* Save macro program. */
	if (lastCompileState == &compileState->runtimeCompileState) {
		e = duckLisp_addInterpretedGenerator(duckLisp,
		                                     compileState,
		                                     expression->compoundExpressions[1].value.identifier);
		if (e) goto cleanup;

		compileState->currentCompileState = lastCompileState;

		e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		if (e) goto cleanup;
	}
	else {
		compileState->currentCompileState = lastCompileState;
	}
	e = duckLisp_addInterpretedGenerator(duckLisp,
	                                     compileState,
	                                     expression->compoundExpressions[1].value.identifier);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&macroBytecode);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_lambda_raw(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression,
                                         dl_bool_t *pure) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_ast_identifier_t gensym = {0};
	duckLisp_ast_identifier_t selfGensym = {0};
	dl_size_t startStack_length = 0;

	dl_array_t bodyAssembly;
	/**/ dl_array_init(&bodyAssembly,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionObject_t),
	                   dl_array_strategy_double);

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2, dl_true);
	if (e) goto cleanup;

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_expression) {
		e = duckLisp_error_pushRuntime(duckLisp, DL_STR("lambda: Args field must be a list."));
		if (e) goto cleanup;
		e = dl_error_invalidValue;
		goto cleanup;
	}

	duckLisp_ast_expression_t *args_list = &expression->compoundExpressions[1].value.expression;
	dl_bool_t variadic = dl_false;

	/* Register function. */
	/* This is not actually where stack functions are allocated. The magic happens in
	   `duckLisp_generator_expression`. */
	{
		dl_ptrdiff_t function_label_index = -1;

		/* Header. */

		e = duckLisp_pushScope(duckLisp, compileState, dl_null, dl_false);
		if (e) goto cleanup;

		e = duckLisp_scope_addObject(duckLisp, compileState, DL_STR("self"));
		if (e) goto cleanup;
		incrementLocalsLength(compileState);

		{
			duckLisp_ast_identifier_t identifier;
			identifier.value = "self";
			identifier.value_length = sizeof("self") - 1;
			/* Since this is effectively a single pass compiler, I don't see a good way to determine purity before
			   compilation of the body. */
			e = duckLisp_addInterpretedFunction(duckLisp, compileState, identifier, dl_false);
			if (e) goto cleanup;
		}

		e = duckLisp_pushScope(duckLisp, compileState, dl_null, dl_true);
		if (e) goto cleanup;

		e = duckLisp_gensym(duckLisp, &gensym);
		if (e) goto cleanup;

		e = duckLisp_register_label(duckLisp, compileState->currentCompileState, gensym.value, gensym.value_length);
		if (e) goto cleanup_gensym;

		/* (goto gensym) */
		e = duckLisp_emit_jump(duckLisp, compileState, &bodyAssembly, gensym.value, gensym.value_length);
		if (e) goto cleanup_gensym;

		e = duckLisp_gensym(duckLisp, &selfGensym);
		if (e) goto cleanup;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            selfGensym.value,
		                            selfGensym.value_length);
		if (e) goto cleanup_gensym;

		/* (label function_name) */
		e = duckLisp_emit_label(duckLisp, compileState, &bodyAssembly, selfGensym.value, selfGensym.value_length);
		if (e) goto cleanup_gensym;

		/* `label_index` should never equal -1 after this function exits. */
		e = scope_getLabelFromName(compileState->currentCompileState,
		                           &function_label_index,
		                           selfGensym.value,
		                           selfGensym.value_length);
		if (e) goto cleanup_gensym;
		if (function_label_index == -1) {
			/* We literally just added the function name to the parent scope. */
			e = dl_error_cantHappen;
			goto cleanup_gensym;
		}


		/* Arguments */

		startStack_length = getLocalsLength(compileState);

		if (expression->compoundExpressions[1].type != duckLisp_ast_type_int) {
			dl_bool_t foundRest = dl_false;
			for (dl_ptrdiff_t j = 0; (dl_size_t) j < args_list->compoundExpressions_length; j++) {
				if (args_list->compoundExpressions[j].type != duckLisp_ast_type_identifier) {
					e = duckLisp_error_pushRuntime(duckLisp, DL_STR("lambda: All args must be identifiers."));
					if (e) goto cleanup_gensym;
					e = dl_error_invalidValue;
					goto cleanup_gensym;
				}

				dl_string_compare(&foundRest,
				                  args_list->compoundExpressions[j].value.identifier.value,
				                  args_list->compoundExpressions[j].value.identifier.value_length,
				                  DL_STR("&rest"));
				if (foundRest) {
					if (args_list->compoundExpressions_length != ((dl_size_t) j + 2)) {
						e = duckLisp_error_pushRuntime(duckLisp,
						                               DL_STR("lambda: "
						                                      "\"&rest\" must be the second to last parameter."));
						if (e) goto cleanup;
						e = dl_error_invalidValue;
						goto cleanup;
						e = dl_error_invalidValue;
						goto cleanup_gensym;
					}
					variadic = dl_true;
					continue;
				}

				e = duckLisp_scope_addObject(duckLisp,
				                             compileState,
				                             args_list->compoundExpressions[j].value.identifier.value,
				                             args_list->compoundExpressions[j].value.identifier.value_length);
				if (e) goto cleanup_gensym;
				incrementLocalsLength(compileState);
			}
		}

		// Body

		duckLisp_ast_expression_t progn;
		progn.compoundExpressions = &expression->compoundExpressions[2];
		progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
		e = duckLisp_generator_expression(duckLisp, compileState, &bodyAssembly, &progn);
		if (e) goto cleanup_gensym;

		// Footer

		{
			duckLisp_scope_t scope;
			e = scope_getTop(duckLisp, compileState->currentCompileState, &scope);
			if (e) goto cleanup_gensym;

			if (scope.scope_uvs_length) {
				e = duckLisp_emit_releaseUpvalues(duckLisp,
				                                  compileState,
				                                  &bodyAssembly,
				                                  scope.scope_uvs,
				                                  scope.scope_uvs_length);
				if (e) goto cleanup_gensym;
			}
		}

		e = duckLisp_emit_return(duckLisp,
		                         compileState,
		                         &bodyAssembly,
		                         TIF(expression->compoundExpressions[1].type == duckLisp_ast_type_expression,
		                             getLocalsLength(compileState) - startStack_length - 1,
		                             0));
		if (e) goto cleanup_gensym;

		compileState->currentCompileState->locals_length = startStack_length;

		/* (label gensym) */
		e = duckLisp_emit_label(duckLisp, compileState, &bodyAssembly, gensym.value, gensym.value_length);
		if (e) goto cleanup_gensym;

		/* Now that the function is complete, append it to the main bytecode. This mechanism guarantees that function
		   bodies are never nested. */
		e = dl_array_pushElements(&compileState->currentCompileState->assembly,
		                          bodyAssembly.elements,
		                          bodyAssembly.elements_length);
		if (e) goto cleanup_gensym;

		{
			/* This needs to be in the same scope or outer than the function arguments so that they don't get
			   captured. It should not need access to the function's local variables, so this scope should be fine. */
			duckLisp_scope_t scope;
			e = scope_getTop(duckLisp, compileState->currentCompileState, &scope);
			if (e) goto cleanup_gensym;
			decrementLocalsLength(compileState);
			e = duckLisp_emit_pushClosure(duckLisp,
			                              compileState,
			                              assembly,
			                              dl_null,
			                              variadic,
			                              function_label_index,
			                              (variadic
			                               ? (args_list->compoundExpressions_length - 2)
			                               : args_list->compoundExpressions_length),
			                              scope.function_uvs,
			                              scope.function_uvs_length);
			if (e) goto cleanup_gensym;
			if (pure != dl_null) *pure = scope.function_uvs_length == 0;
		}

		e = duckLisp_popScope(duckLisp, compileState, dl_null);
		if (e) goto cleanup_gensym;

		e = duckLisp_popScope(duckLisp, compileState, dl_null);
		if (e) goto cleanup_gensym;
	}

 cleanup_gensym:
	eError = DL_FREE(duckLisp->memoryAllocation, &gensym.value);
	if (eError) e = eError;

 cleanup:

	if (selfGensym.value) {
		eError = DL_FREE(duckLisp->memoryAllocation, &selfGensym.value);
		if (eError) e = eError;
	}

	eError = dl_array_quit(&bodyAssembly);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_lambda(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_lambda_raw(duckLisp, compileState, assembly, expression, dl_null);
}

/* If `pure` is non-null, then it will treat the value form as a lambda NO MATTER WHAT. */
dl_error_t duckLisp_generator_createVar_raw(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            dl_array_t *assembly,
                                            duckLisp_ast_expression_t *expression,
                                            dl_bool_t *pure) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_size_t startStack_length = getLocalsLength(compileState);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_false);
	if (e) goto cleanup;

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	// Notice here, that a variable could potentially refer to itself.
	/* Insert arg1 into this scope's name trie. */
	/* This is not actually where stack variables are allocated. The magic happens in
	   `duckLisp_generator_expression`. */
	dl_size_t startLocals_length = getLocalsLength(compileState);
	if (pure != dl_null) {
		e = duckLisp_generator_lambda_raw(duckLisp,
		                                  compileState,
		                                  assembly,
		                                  &expression->compoundExpressions[2].value.expression,
		                                  pure);
	}
	else {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[2],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
	}
	if (e) goto cleanup;
	dl_size_t endLocals_length = getLocalsLength(compileState);
	compileState->currentCompileState->locals_length = startLocals_length;
	e = duckLisp_scope_addObject(duckLisp,
	                             compileState,
	                             expression->compoundExpressions[1].value.identifier.value,
	                             expression->compoundExpressions[1].value.identifier.value_length);
	if (e) goto cleanup;
	compileState->currentCompileState->locals_length = endLocals_length;

	e = duckLisp_emit_move(duckLisp, compileState, assembly, startStack_length, getLocalsLength(compileState) - 1);
	if (e) goto cleanup;
	if (getLocalsLength(compileState) > startStack_length + 1) {
		e = duckLisp_emit_pop(duckLisp, compileState, assembly, getLocalsLength(compileState) - startStack_length - 1);
		if (e) goto cleanup;
	}
	e = duckLisp_emit_pushIndex(duckLisp, compileState, assembly, startStack_length);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_createVar(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression) {
	/* Sort of like partial application... */
	return duckLisp_generator_createVar_raw(duckLisp, compileState, assembly, expression, dl_null);
}

dl_error_t duckLisp_generator_static(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_false);
	if (e) goto cleanup;

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString,
		                          expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	// Notice here, that a variable could potentially refer to itself.
	/* Insert arg1 into this scope's name trie. */
	/* This is not actually where stack variables are allocated. The magic happens in
	   `duckLisp_generator_expression`. */
	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[2],
	                                        dl_null,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;
	dl_ptrdiff_t static_index = -1;
	e = duckLisp_addStatic(duckLisp,
	                       expression->compoundExpressions[1].value.identifier.value,
	                       expression->compoundExpressions[1].value.identifier.value_length,
	                       &static_index);
	if (e) goto cleanup;

	e = duckLisp_emit_setStatic(duckLisp, compileState, assembly, static_index, getLocalsLength(compileState) - 1);

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_defun(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_true);
	if (e) goto cleanup;

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = duckLisp_error_pushRuntime(duckLisp, DL_STR("defun: Name field must be an identifier."));
		if (e) goto cleanup;
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if ((expression->compoundExpressions[2].type != duckLisp_ast_type_expression)
	    && ((expression->compoundExpressions[2].type == duckLisp_ast_type_int)
	        && (expression->compoundExpressions[2].value.integer.value != 0))) {

		e = duckLisp_error_pushRuntime(duckLisp, DL_STR("defun: Args field must be a list."));
		if (e) goto cleanup;

		e = dl_error_invalidValue;
		goto cleanup;
	}

	duckLisp_ast_expression_t lambda;
	e = DL_MALLOC(duckLisp->memoryAllocation,
	              (void **) &lambda.compoundExpressions,
	              expression->compoundExpressions_length - 1,
	              duckLisp_ast_compoundExpression_t);
	if (e) goto cleanup;
	lambda.compoundExpressions_length = expression->compoundExpressions_length - 1;
	lambda.compoundExpressions[0].type = duckLisp_ast_type_identifier;
	lambda.compoundExpressions[0].value.identifier.value = "\0defun:lambda";
	lambda.compoundExpressions[0].value.identifier.value_length = sizeof("\0defun:lambda") - 1;
	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		lambda.compoundExpressions[i - 1] = expression->compoundExpressions[i];
	}
	duckLisp_ast_expression_t var;
	e = DL_MALLOC(duckLisp->memoryAllocation, (void **) &var.compoundExpressions, 3, duckLisp_ast_compoundExpression_t);
	if (e) goto cleanup;
	var.compoundExpressions_length = 3;
	var.compoundExpressions[0].type = duckLisp_ast_type_identifier;
	var.compoundExpressions[0].value.identifier.value = "\0defun:var";
	var.compoundExpressions[0].value.identifier.value_length = sizeof("\0defun:var") - 1;
	var.compoundExpressions[1] = expression->compoundExpressions[1];
	var.compoundExpressions[2].type = duckLisp_ast_type_expression;
	var.compoundExpressions[2].value.expression = lambda;
	{
		dl_bool_t pure;
		e = duckLisp_generator_createVar_raw(duckLisp, compileState, assembly, &var, &pure);
		if (e) goto cleanup;

		e = DL_FREE(duckLisp->memoryAllocation, (void **) &var.compoundExpressions);
		if (e) goto cleanup;
		e = DL_FREE(duckLisp->memoryAllocation, (void **) &lambda.compoundExpressions);
		if (e) goto cleanup;

		e = duckLisp_addInterpretedFunction(duckLisp,
		                                    compileState,
		                                    expression->compoundExpressions[1].value.identifier,
		                                    pure);
		if (e) goto cleanup;
	}

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_error(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	(void) assembly;
	(void) compileState;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2, dl_false);
	if (e) goto cleanup;
	e = duckLisp_checkTypeAndReportError(duckLisp,
	                                     expression->compoundExpressions[0].value.identifier,
	                                     expression->compoundExpressions[1],
	                                     duckLisp_ast_type_string);
	if (e) goto cleanup;

	e = dl_array_pushElements(&eString,
	                          expression->compoundExpressions[0].value.identifier.value,
	                          expression->compoundExpressions[0].value.identifier.value_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(&eString, DL_STR(": "));
	if (e) goto cleanup;
	e = dl_array_pushElements(&eString,
	                          expression->compoundExpressions[1].value.string.value,
	                          expression->compoundExpressions[1].value.string.value_length);
	if (e) goto cleanup;
	eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
	if (eError) e = eError;
	goto cleanup;

 cleanup:
	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return dl_error_invalidValue;
}

dl_error_t duckLisp_generator_not(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_unaryArithmeticOperator(duckLisp,
	                                                  compileState,
	                                                  assembly,
	                                                  expression,
	                                                  duckLisp_emit_not);
}

dl_error_t duckLisp_generator_multiply(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_multiply);
}

dl_error_t duckLisp_generator_divide(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_divide);
}

dl_error_t duckLisp_generator_add(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_add);
}

dl_error_t duckLisp_generator_sub(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_sub);
}

dl_error_t duckLisp_generator_equal(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_equal);
}

dl_error_t duckLisp_generator_greater(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_greater);
}

dl_error_t duckLisp_generator_less(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression) {
	return duckLisp_generator_binaryArithmeticOperator(duckLisp,
	                                                   compileState,
	                                                   assembly,
	                                                   expression,
	                                                   duckLisp_emit_less);
}

dl_error_t duckLisp_generator_while(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_size_t startStack_length;

	duckLisp_ast_identifier_t gensym_start;
	duckLisp_ast_identifier_t gensym_loop;

	/* Check arguments for call and type errors. */

	if (expression->compoundExpressions[0].type != duckLisp_ast_type_identifier) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    (char *) eString.elements,
		                                    eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_true);
	if (e) goto cleanup;

	// Condition

	switch (expression->compoundExpressions[1].type) {
	case duckLisp_ast_type_bool:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.boolean.value;
		break;
	case duckLisp_ast_type_int:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.integer.value != 0;
		break;
	case duckLisp_ast_type_float:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.floatingPoint.value != 0.0;
		break;
	case duckLisp_ast_type_string:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.string.value_length > 0;
		break;
	case duckLisp_ast_type_identifier:
	case duckLisp_ast_type_expression:
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("while: Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	if (forceGoto && branch) {
		e = duckLisp_gensym(duckLisp, &gensym_start);
		if (e) goto cleanup;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_start.value,
		                            gensym_start.value_length);
		if (e) goto free_gensym_start;

		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_start.value, gensym_start.value_length);
		if (e) goto free_gensym_start;

		{
			duckLisp_ast_expression_t progn;

			e = duckLisp_pushScope(duckLisp, compileState, dl_null, dl_false);
			if (e) goto free_gensym_start;

			// Arguments

			startStack_length = getLocalsLength(compileState);

			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto free_gensym_start;

			if (getLocalsLength(compileState) > startStack_length) {
				e = duckLisp_emit_pop(duckLisp,
				                      compileState,
				                      assembly,
				                      getLocalsLength(compileState) - startStack_length);
				if (e) goto free_gensym_start;
			}

			e = duckLisp_popScope(duckLisp, compileState, dl_null);
			if (e) goto free_gensym_start;
		}

		e = duckLisp_emit_jump(duckLisp,
		                       compileState,
		                       assembly,
		                       gensym_start.value,
		                       gensym_start.value_length);
		if (e) goto free_gensym_start;

		goto free_gensym_start;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_start);
		if (e) goto cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_loop);
		if (e) goto free_gensym_start;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_start.value,
		                            gensym_start.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_loop.value,
		                            gensym_loop.value_length);
		if (e) goto free_gensym_end;

		e = duckLisp_emit_jump(duckLisp, compileState, assembly, gensym_start.value, gensym_start.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_loop.value, gensym_loop.value_length);
		if (e) goto free_gensym_end;

		{
			duckLisp_ast_expression_t progn;

			e = duckLisp_pushScope(duckLisp, compileState, dl_null, dl_false);
			if (e) goto free_gensym_end;

			// Arguments

			startStack_length = getLocalsLength(compileState);

			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto free_gensym_end;

			if (getLocalsLength(compileState) > startStack_length) {
				e = duckLisp_emit_pop(duckLisp,
				                      compileState,
				                      assembly,
				                      getLocalsLength(compileState) - startStack_length);
				if (e) goto free_gensym_end;
			}

			e = duckLisp_popScope(duckLisp, compileState, dl_null);
			if (e) goto free_gensym_end;
		}

		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_start.value, gensym_start.value_length);
		if (e) goto free_gensym_end;
		startStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[1],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_brnz(duckLisp,
		                       compileState,
		                       assembly,
		                       gensym_loop.value,
		                       gensym_loop.value_length,
		                       getLocalsLength(compileState) - startStack_length);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		if (e) goto free_gensym_end;

		goto free_gensym_end;
	}
	/* Flow does not reach here. */

	/*
	  (goto start)
	  (label loop)

	  (label start)
	  (brnz condition loop)
	*/

 free_gensym_end:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_loop.value);
	if (eError) e = eError;
	gensym_loop.value_length = 0;
 free_gensym_start:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_start.value);
	if (eError) e = eError;
	gensym_start.value_length = 0;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_unless(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_ptrdiff_t startStack_length;
	/* dl_bool_t noPop = dl_false; */
	int pops = 0;

	duckLisp_ast_identifier_t gensym_then;
	duckLisp_ast_identifier_t gensym_end;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_true);
	if (e) goto cleanup;

	// Condition
	startStack_length = getLocalsLength(compileState);

	switch (expression->compoundExpressions[1].type) {
	case duckLisp_ast_type_bool:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.boolean.value;
		break;
	case duckLisp_ast_type_int:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.integer.value != 0;
		break;
	case duckLisp_ast_type_float:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.floatingPoint.value != 0.0;
		break;
	case duckLisp_ast_type_string:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.string.value_length > 0;
		break;
	case duckLisp_ast_type_identifier:
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[1],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_expression:
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[1],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;
		pops = getLocalsLength(compileState) - startStack_length;
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("unless: Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	if (forceGoto) {
		if (branch) {
			e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		}
		else {
			duckLisp_ast_expression_t progn;
			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto cleanup;
		}
		goto cleanup;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_then);
		if (e) goto cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_end);
		if (e) goto free_gensym_then;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_then.value,
		                            gensym_then.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_end.value,
		                            gensym_end.value_length);
		if (e) goto free_gensym_end;

		e = duckLisp_emit_brnz(duckLisp, compileState, assembly, gensym_then.value, gensym_then.value_length, pops);
		if (e) goto free_gensym_end;
		startStack_length = getLocalsLength(compileState);
		{
			duckLisp_ast_expression_t progn;
			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto free_gensym_end;
		}
		compileState->currentCompileState->locals_length = startStack_length;
		e = duckLisp_emit_jump(duckLisp, compileState, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_then.value, gensym_then.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto free_gensym_end;

		goto free_gensym_end;
	}
	/* Flow does not reach here. */

 free_gensym_end:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_end.value);
	if (!e) e = eError;
	gensym_end.value_length = 0;
 free_gensym_then:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_then.value);
	if (!e) e = eError;
	gensym_then.value_length = 0;

 cleanup:

	eError = dl_array_quit(&eString);
	if (!e) e = eError;

	return e;
}

dl_error_t duckLisp_generator_when(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_ptrdiff_t startStack_length;
	int pops = 0;

	duckLisp_ast_identifier_t gensym_then;
	duckLisp_ast_identifier_t gensym_end;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_true);
	if (e) goto cleanup;

	/* Condition */
	startStack_length = getLocalsLength(compileState);

	switch (expression->compoundExpressions[1].type) {
	case duckLisp_ast_type_bool:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.boolean.value;
		break;
	case duckLisp_ast_type_int:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.integer.value != 0;
		break;
	case duckLisp_ast_type_float:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.floatingPoint.value != 0.0;
		break;
	case duckLisp_ast_type_string:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.string.value_length > 0;
		break;
	case duckLisp_ast_type_identifier:
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[1],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_expression:
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[1],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;
		pops = getLocalsLength(compileState) - startStack_length;
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("when: Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	if (forceGoto) {
		if (branch) {
			duckLisp_ast_expression_t progn;
			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto cleanup;
		}
		else {
			e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		}
		goto cleanup;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_then);
		if (e) goto cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_end);
		if (e) goto free_gensym_then;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_then.value,
		                            gensym_then.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_end.value,
		                            gensym_end.value_length);
		if (e) goto free_gensym_end;

		e = duckLisp_emit_brnz(duckLisp, compileState, assembly, gensym_then.value, gensym_then.value_length, pops);
		if (e) goto free_gensym_end;
		startStack_length = getLocalsLength(compileState);
		e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		if (e) goto free_gensym_end;
		compileState->currentCompileState->locals_length = startStack_length;
		e = duckLisp_emit_jump(duckLisp, compileState, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_then.value, gensym_then.value_length);
		if (e) goto free_gensym_end;
		{
			duckLisp_ast_expression_t progn;
			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto free_gensym_end;
		}
		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto free_gensym_end;

		goto free_gensym_end;
	}
	/* Flow does not reach here. */

 free_gensym_end:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_end.value);
	if (!e) e = eError;
	gensym_end.value_length = 0;
 free_gensym_then:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_then.value);
	if (!e) e = eError;
	gensym_then.value_length = 0;

 cleanup:

	eError = dl_array_quit(&eString);
	if (!e) e = eError;

	return e;
}

dl_error_t duckLisp_generator_if(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);


	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_ptrdiff_t startStack_length;
	/* dl_bool_t noPop = dl_false; */
	int pops = 0;

	duckLisp_ast_identifier_t gensym_then;
	duckLisp_ast_identifier_t gensym_end;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 4, dl_false);
	if (e) goto cleanup;

	// Condition

	switch (expression->compoundExpressions[1].type) {
	case duckLisp_ast_type_bool:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.boolean.value;
		break;
	case duckLisp_ast_type_int:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.integer.value != 0;
		break;
	case duckLisp_ast_type_float:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.floatingPoint.value != 0.0;
		break;
	case duckLisp_ast_type_string:
		forceGoto = dl_true;
		branch = expression->compoundExpressions[1].value.string.value_length > 0;
		break;
	case duckLisp_ast_type_identifier:
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[1],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_expression: {
		dl_ptrdiff_t temp_index = -1;
		startStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_expression(duckLisp,
		                                compileState,
		                                assembly,
		                                expression->compoundExpressions[0].value.identifier.value,
		                                expression->compoundExpressions[0].value.identifier.value_length,
		                                &expression->compoundExpressions[1].value.expression,
		                                &temp_index);
		if (e) goto cleanup;
		pops = getLocalsLength(compileState) - startStack_length;
		break;
	}
	default:
		e = dl_array_pushElements(&eString, DL_STR("if: Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	if (forceGoto) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[branch
		                                                                         ? 2
		                                                                         : 3],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		goto cleanup;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_then);
		if (e) goto cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_end);
		if (e) goto free_gensym_then;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_then.value,
		                            gensym_then.value_length);
		if (e) goto free_gensym_end;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            gensym_end.value,
		                            gensym_end.value_length);
		if (e) goto free_gensym_end;

		e = duckLisp_emit_brnz(duckLisp, compileState, assembly, gensym_then.value, gensym_then.value_length, pops);
		if (e) goto free_gensym_end;

		startStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[3],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto free_gensym_end;
		pops = getLocalsLength(compileState) - startStack_length - 1;
		if (pops < 0) {
			e = dl_array_pushElements(&eString, DL_STR("if: \"else\" part of expression contains an invalid form"));
		}
		else {
			e = duckLisp_emit_move(duckLisp,
			                       compileState,
			                       assembly,
			                       startStack_length,
			                       getLocalsLength(compileState) - 1);
			if (e) goto free_gensym_end;
			if (pops > 0) {
				e = duckLisp_emit_pop(duckLisp, compileState, assembly, pops);
			}
		}
		if (e) goto free_gensym_end;
		e = duckLisp_emit_jump(duckLisp, compileState, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_then.value, gensym_then.value_length);

		compileState->currentCompileState->locals_length = startStack_length;

		if (e) goto free_gensym_end;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[2],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto free_gensym_end;
		pops = getLocalsLength(compileState) - startStack_length - 1;
		if (pops < 0) {
			e = dl_array_pushElements(&eString, DL_STR("if: \"then\" part of expression contains an invalid form"));
		}
		else {
			e = duckLisp_emit_move(duckLisp,
			                       compileState,
			                       assembly,
			                       startStack_length,
			                       getLocalsLength(compileState) - 1);
			if (e) goto free_gensym_end;
			if (pops > 0) {
				e = duckLisp_emit_pop(duckLisp, compileState, assembly, pops);
			}
		}
		if (e) goto free_gensym_end;

		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto free_gensym_end;

		goto free_gensym_end;
	}
	/* Flow does not reach here. */

	/* (brnz condition $then); */
	/* else; */
	/* (goto $end); */
	/* (label $then); */
	/* then; */
	/* (label $end); */

 free_gensym_end:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_end.value);
	if (eError) e = eError;
	gensym_end.value_length = 0;
 free_gensym_then:
	eError = dl_free(duckLisp->memoryAllocation, (void **) &gensym_then.value);
	if (eError) e = eError;
	gensym_then.value_length = 0;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_setq(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t index = -1;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_false);
	if (e) goto cleanup;

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("setq: Argument 1 of function \""));
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString,
		                          expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		e = dl_error_invalidValue;
		goto cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[2],
	                                        &index,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;

	// Unlike most other instances, this is for assignment.
	e = duckLisp_scope_getLocalIndexFromName(compileState->currentCompileState,
	                                         &identifier_index,
	                                         expression->compoundExpressions[1].value.identifier.value,
	                                         expression->compoundExpressions[1].value.identifier.value_length);
	if (e) goto cleanup;
	if (identifier_index == -1) {
		dl_ptrdiff_t scope_index;
		dl_bool_t found;
		e = duckLisp_scope_getFreeLocalIndexFromName(duckLisp,
		                                             compileState->currentCompileState,
		                                             &found,
		                                             &identifier_index,
		                                             &scope_index,
		                                             expression->compoundExpressions[1].value.identifier.value,
		                                             expression->compoundExpressions[1].value.identifier.value_length);
		if (e) goto cleanup;
		if (!found) {
			identifier_index = duckLisp_symbol_nameToValue(duckLisp,
			                                               expression->compoundExpressions[1].value.identifier.value,
			                                               (expression->compoundExpressions[1]
			                                                .value.identifier.value_length));
			if (identifier_index == -1) {
				e = dl_array_pushElements(&eString, DL_STR("setq: Could not find variable \""));
				if (e) goto cleanup;
				e = dl_array_pushElements(&eString,
				                          expression->compoundExpressions[1].value.identifier.value,
				                          expression->compoundExpressions[1].value.identifier.value_length);
				if (e) goto cleanup;
				e = dl_array_pushElements(&eString, DL_STR("\" in lexical scope. Assuming global scope."));
				if (e) goto cleanup;
				eError = duckLisp_error_pushRuntime(duckLisp,
				                                    eString.elements,
				                                    eString.elements_length * eString.element_size);
				if (eError) e = eError;

				e = duckLisp_symbol_create(duckLisp,
				                           (expression->compoundExpressions[1]
				                            .value.identifier.value),
				                           (expression->compoundExpressions[1]
				                            .value.identifier.value_length));
				if (e) goto cleanup;
				identifier_index = duckLisp_symbol_nameToValue(duckLisp,
				                                               (expression->compoundExpressions[1]
				                                                .value.identifier.value),
				                                               (expression->compoundExpressions[1]
				                                                .value.identifier.value_length));
				e = duckLisp_emit_setStatic(duckLisp,
				                            compileState,
				                            assembly,
				                            identifier_index,
				                            getLocalsLength(compileState) - 1);
				if (e) goto cleanup;
			}
			else {
				e = duckLisp_emit_setStatic(duckLisp,
				                            compileState,
				                            assembly,
				                            identifier_index,
				                            getLocalsLength(compileState) - 1);
				if (e) goto cleanup;
			}
		}
		else {
			/* Now the trick here is that we need to mirror the free variable as a local variable.
			   Actually, scratch that. We need to simply push the UV. Creating it as a local variable is an
			   optimization that can be done in `duckLisp_compile_expression`. It can't be done here. */
			e = duckLisp_emit_setUpvalue(duckLisp,
			                             compileState,
			                             assembly,
			                             identifier_index,
			                             getLocalsLength(compileState) - 1);
			if (e) goto cleanup;
			identifier_index = getLocalsLength(compileState) - 1;
		}
	}
	else {
		e = duckLisp_emit_move(duckLisp, compileState, assembly, identifier_index, getLocalsLength(compileState) - 1);
		if (e) goto cleanup;
	}

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_nop(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression) {
	(void) expression;
	return duckLisp_emit_nop(duckLisp, compileState, assembly);
}

dl_error_t duckLisp_generator_label(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2, dl_false);
	if (e) goto cleanup;

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString,
		                          expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckLisp_emit_label(duckLisp,
	                        compileState,
	                        assembly,
	                        expression->compoundExpressions[1].value.string.value,
	                        expression->compoundExpressions[1].value.string.value_length);
	if (e) goto cleanup;

	// Don't push label into trie. This will be done later during assembly.

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_goto(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2, dl_false);
	if (e) goto cleanup;

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString,
		                          expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckLisp_emit_jump(duckLisp,
	                       compileState,
	                       assembly,
	                       expression->compoundExpressions[1].value.string.value,
	                       expression->compoundExpressions[1].value.string.value_length);
	if (e) goto cleanup;

	/* Don't push label into trie. This will be done later during assembly. */

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_acall(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t outerStartStack_length;
	dl_ptrdiff_t innerStartStack_length;

	if (expression->compoundExpressions_length == 0) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if (expression->compoundExpressions[0].type != duckLisp_ast_type_identifier) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if (expression->compoundExpressions_length < 2) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Too few arguments for function \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString,
		                               expression->compoundExpressions[0].value.identifier.value,
		                               expression->compoundExpressions[0].value.identifier.value_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    (char *) eString.elements,
		                                    eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Generate */

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &identifier_index,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;

	outerStartStack_length = getLocalsLength(compileState);

	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;

		e = duckLisp_emit_move(duckLisp,
		                       compileState,
		                       assembly,
		                       innerStartStack_length,
		                       getLocalsLength(compileState) - 1);
		if (e) goto cleanup;
		if (getLocalsLength(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      getLocalsLength(compileState) - innerStartStack_length - 1);
			if (e) goto cleanup;
		}
	}

	/* The zeroth argument is the function name, which also happens to be a label. */
	e = duckLisp_emit_acall(duckLisp,
	                        compileState,
	                        assembly,
	                        identifier_index,
	                        0);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length = outerStartStack_length + 1;

	/* Don't push label into trie. This will be done later during assembly. */

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

/* dl_error_t duckLisp_generator_subroutine(duckLisp_t *duckLisp, */
/*                                          dl_array_t *assembly, */
/*                                          duckLisp_ast_expression_t *expression) { */
/* 	dl_error_t e = dl_error_ok; */
/* 	dl_error_t eError = dl_error_ok; */
/* 	dl_array_t eString; */
/* 	/\**\/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double); */

/* 	dl_ptrdiff_t identifier_index = -1; */
/* 	dl_ptrdiff_t outerStartStack_length = duckLisp->locals_length; */
/* 	dl_ptrdiff_t innerStartStack_length; */

/* 	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) { */
/* 		innerStartStack_length = duckLisp->locals_length; */
/* 		e = duckLisp_compile_compoundExpression(duckLisp, */
/* 		                                        assembly, */
/* 		                                        expression->compoundExpressions[0].value.identifier.value, */
/* 		                                        expression->compoundExpressions[0].value.identifier.value_length, */
/* 		                                        &expression->compoundExpressions[i], */
/* 		                                        &identifier_index, */
/* 		                                        dl_null, */
/* 		                                        dl_true); */
/* 		if (e) goto l_cleanup; */

/* 		e = duckLisp_emit_move(duckLisp, assembly, innerStartStack_length, duckLisp->locals_length - 1); */
/* 		if (e) goto l_cleanup; */
/* 		e = duckLisp_emit_pop(duckLisp, assembly, duckLisp->locals_length - innerStartStack_length - 1); */
/* 		if (e) goto l_cleanup; */
/* 	} */
/* 	duckLisp->locals_length = outerStartStack_length + 1; */

/* 	// The zeroth argument is the function name, which also happens to be a label. */
/* 	e = duckLisp_emit_call(duckLisp, */
/* 	                       assembly, */
/* 	                       expression->compoundExpressions[0].value.string.value, */
/* 	                       expression->compoundExpressions[0].value.string.value_length, */
/* 	                       0);  // So apparently this does nothing in the VM. */
/* 	if (e) { */
/* 		goto l_cleanup; */
/* 	} */

/* 	// Don't push label into trie. This will be done later during assembly. */

/*  l_cleanup: */

/* 	eError = dl_array_quit(&eString); */
/* 	if (eError) { */
/* 		e = eError; */
/* 	} */

/* 	return e; */
/* } */

dl_error_t duckLisp_generator_funcall(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t innerStartStack_length;
	dl_ptrdiff_t outerStartStack_length;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[0],
	                                        &identifier_index,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;

	outerStartStack_length = getLocalsLength(compileState);

	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;

		e = duckLisp_emit_move(duckLisp,
		                       compileState,
		                       assembly,
		                       innerStartStack_length,
		                       getLocalsLength(compileState) - 1);
		if (e) goto cleanup;
		if (getLocalsLength(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      getLocalsLength(compileState) - innerStartStack_length - 1);
			if (e) goto cleanup;
		}
	}

	/* The zeroth argument is the function name, which also happens to be a label. */
	e = duckLisp_emit_funcall(duckLisp,
	                          compileState,
	                          assembly,
	                          identifier_index,
	                          expression->compoundExpressions_length - 1);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length = outerStartStack_length + 1;

	/* Don't push label into trie. This will be done later during assembly. */

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_funcall2(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t innerStartStack_length;
	dl_ptrdiff_t outerStartStack_length;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &identifier_index,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;

	outerStartStack_length = getLocalsLength(compileState);

	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;

		e = duckLisp_emit_move(duckLisp,
		                       compileState,
		                       assembly,
		                       innerStartStack_length, getLocalsLength(compileState) - 1);
		if (e) goto cleanup;
		if (getLocalsLength(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      getLocalsLength(compileState) - innerStartStack_length - 1);
			if (e) goto cleanup;
		}
	}

	/* The zeroth argument is the function name, which also happens to be a label. */
	e = duckLisp_emit_funcall(duckLisp,
	                          compileState,
	                          assembly,
	                          identifier_index,
	                          expression->compoundExpressions_length - 2);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length = outerStartStack_length + 1;

	/* Don't push label into trie. This will be done later during assembly. */

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_apply(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t innerStartStack_length;
	dl_ptrdiff_t outerStartStack_length;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_true);
	if (e) goto cleanup;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &identifier_index,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;

	outerStartStack_length = getLocalsLength(compileState);

	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;

		e = duckLisp_emit_move(duckLisp,
		                       compileState,
		                       assembly,
		                       innerStartStack_length,
		                       getLocalsLength(compileState) - 1);
		if (e) goto cleanup;
		if (getLocalsLength(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      getLocalsLength(compileState) - innerStartStack_length - 1);
			if (e) goto cleanup;
		}
	}

	/* The zeroth argument is the function name, which also happens to be a label. */
	/* -3 for "apply", function, and list argument. */
	e = duckLisp_emit_apply(duckLisp,
	                        compileState,
	                        assembly,
	                        identifier_index,
	                        expression->compoundExpressions_length - 3);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length = outerStartStack_length + 1;

	/* Don't push label into trie. This will be done later during assembly. */

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_callback(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t callback_key = -1;
	dl_ptrdiff_t innerStartStack_length;
	dl_ptrdiff_t outerStartStack_length;

	callback_key = duckLisp_symbol_nameToValue(duckLisp,
	                                             expression->compoundExpressions[0].value.string.value,
	                                             expression->compoundExpressions[0].value.string.value_length);
	if (e || callback_key == -1) {
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("callback: Could not find callback name."));
		if (eError) e = eError;
		goto cleanup;
	}

	outerStartStack_length = getLocalsLength(compileState);

	/* Push all arguments onto the stack. */
	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = getLocalsLength(compileState);
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;

		e = duckLisp_emit_move(duckLisp,
		                       compileState,
		                       assembly,
		                       innerStartStack_length,
		                       getLocalsLength(compileState) - 1);
		if (e) goto cleanup;
		if (getLocalsLength(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      getLocalsLength(compileState) - innerStartStack_length - 1);
			if (e) goto cleanup;
		}
	}

	/* Create the string variable. */
	e = duckLisp_emit_ccall(duckLisp, compileState, assembly, callback_key);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length = outerStartStack_length + 1;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_macro(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression,
                                    dl_ptrdiff_t *index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_array_t bytecode;  /* dl_array_t:dl_uint8_t */
	duckVM_object_t return_value;
	duckLisp_ast_compoundExpression_t ast;
	duckLisp_instruction_t yieldInstruction = duckLisp_instruction_yield;
	duckLisp_subCompileState_t *lastSubCompileState = compileState->currentCompileState;
	dl_ptrdiff_t functionIndex = -1;
	/**/ dl_array_init(&bytecode, duckLisp->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 1, dl_true);
	if (e) goto cleanupArrays;
	e = duckLisp_checkTypeAndReportError(duckLisp,
	                                     expression->compoundExpressions[0].value.identifier,
	                                     expression->compoundExpressions[0],
	                                     duckLisp_ast_type_identifier);
	if (e) goto cleanupArrays;

	/* Get macro index. */

	compileState->currentCompileState = &compileState->comptimeCompileState;
	dl_size_t lastLocalsLength = getLocalsLength(compileState);
	compileState->currentCompileState->locals_length = duckLisp->vm.stack.elements_length;

	e = duckLisp_scope_getMacroFromName(compileState->currentCompileState,
	                                    &functionIndex,
	                                    expression->compoundExpressions[0].value.identifier.value,
	                                    expression->compoundExpressions[0].value.identifier.value_length);
	if (e) goto cleanupArrays;

	/* Generate bytecode for arguments. */

	{
		dl_size_t outerStartStack_length = getLocalsLength(compileState);
		duckLisp_ast_compoundExpression_t quote;
		e = DL_MALLOC(duckLisp->memoryAllocation,
		              &quote.value.expression.compoundExpressions,
		              2,
		              duckLisp_ast_compoundExpression_t);
		if (e) goto cleanupArrays;
		for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
			dl_size_t innerStartStack_length = getLocalsLength(compileState);
			quote.type = duckLisp_ast_type_expression;
			quote.value.expression.compoundExpressions_length = 2;
			quote.value.expression.compoundExpressions[0].type = duckLisp_ast_type_identifier;
			quote.value.expression.compoundExpressions[0].value.identifier.value = "__quote";
			quote.value.expression.compoundExpressions[0].value.identifier.value_length = sizeof("__quote") - 1;
			quote.value.expression.compoundExpressions[1] = expression->compoundExpressions[i];
			/* call.value.expression.compoundExpressions[i] = quote; */

			e = duckLisp_compile_compoundExpression(duckLisp,
			                                        compileState,
			                                        &compileState->currentCompileState->assembly,
			                                        expression->compoundExpressions[0].value.identifier.value,
			                                        expression->compoundExpressions[0].value.identifier.value_length,
			                                        &quote,
			                                        dl_null,
			                                        dl_null,
			                                        dl_true);
			if (e) goto cleanupArguments;

			e = duckLisp_emit_move(duckLisp,
			                       compileState,
			                       &compileState->currentCompileState->assembly,
			                       innerStartStack_length,
			                       getLocalsLength(compileState) - 1);
			if (e) goto cleanupArguments;
			if (getLocalsLength(compileState) - innerStartStack_length - 1 > 0) {
				e = duckLisp_emit_pop(duckLisp,
				                      compileState,
				                      &compileState->currentCompileState->assembly,
				                      getLocalsLength(compileState) - innerStartStack_length - 1);
				if (e) goto cleanupArguments;
			}
		}

		/* The zeroth argument is the function name, which also happens to be a label. */
		/* { */
		/* 	duckLisp_ast_compoundExpression_t ce; */
		/* 	ce.type = duckLisp_ast_type_expression; */
		/* 	ce.value.expression = *expression; */
		/* 	duckLisp_ast_print(duckLisp, ce); */
		/* } */
		e = duckLisp_emit_funcall(duckLisp,
		                          compileState,
		                          &compileState->currentCompileState->assembly,
		                          functionIndex,
		                          expression->compoundExpressions_length - 1);
		if (e) goto cleanupArguments;

		compileState->currentCompileState->locals_length = outerStartStack_length + 1;

	cleanupArguments:
		if (e) goto cleanupArrays;

		if (quote.value.expression.compoundExpressions) {
			e = DL_FREE(duckLisp->memoryAllocation, &quote.value.expression.compoundExpressions);
			if (e) goto cleanupArrays;
		}
	}

	/* Assemble. */

	e = duckLisp_assemble(duckLisp, compileState, &bytecode, &compileState->currentCompileState->assembly);
	if (e) goto cleanupArrays;

	e = dl_array_pushElement(&bytecode, &yieldInstruction);
	if (e) goto cleanupArrays;

	/* Execute macro. */

	/* puts(duckLisp_disassemble(duckLisp->memoryAllocation, bytecode.elements, bytecode.elements_length)); */

	e = duckVM_execute(&duckLisp->vm, &return_value, bytecode.elements, bytecode.elements_length);
	eError = dl_array_pushElements(&duckLisp->errors,
	                               duckLisp->vm.errors.elements,
	                               duckLisp->vm.errors.elements_length);
	if (!e) e = eError;
	if (e) goto cleanupArrays;
	e = dl_array_popElements(&duckLisp->vm.errors, dl_null, duckLisp->vm.errors.elements_length);
	if (e) goto cleanupArrays;

	e = dl_array_popElements(&compileState->currentCompileState->assembly,
	                         dl_null,
	                         compileState->currentCompileState->assembly.elements_length);
	if (e) goto cleanupArrays;

	/* Compile macro expansion. */

	e = duckLisp_objectToAST(duckLisp, &ast, &return_value, dl_true);
	if (e) goto cleanupArrays;

	/* duckLisp_ast_print(duckLisp, ast); */

	e = duckVM_pop(&duckLisp->vm, dl_null);
	if (e) goto cleanupArrays;
	/**/ decrementLocalsLength(compileState);

	compileState->currentCompileState->locals_length = lastLocalsLength;
	compileState->currentCompileState = lastSubCompileState;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &ast,
	                                        index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto cleanupAST;

 cleanupAST:

	eError = ast_compoundExpression_quit(duckLisp, &ast);
	if (!e) e = eError;

 cleanupArrays:

	eError = dl_array_quit(&bytecode);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	compileState->currentCompileState = lastSubCompileState;

	return e;
}

dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Push a new scope. */
	e = duckLisp_pushScope(duckLisp, compileState, dl_null, dl_false);
	if (e) goto cleanup;

	dl_size_t startStack_length = getLocalsLength(compileState);

	e = duckLisp_generator_noscope(duckLisp, compileState, assembly, expression);
	if (e) goto cleanup;

	duckLisp_scope_t scope;
	e = scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	if (scope.scope_uvs_length) {
		e = duckLisp_emit_releaseUpvalues(duckLisp, compileState, assembly, scope.scope_uvs, scope.scope_uvs_length);
		if (e) goto cleanup;
	}

	dl_ptrdiff_t source = getLocalsLength(compileState) - 1;
	dl_ptrdiff_t destination = startStack_length - 1 + 1;
	if (destination < source) {
		e = duckLisp_emit_move(duckLisp, compileState, assembly, destination, source);
		if (e) goto cleanup;
	}
	dl_ptrdiff_t pops = (getLocalsLength(compileState) - (startStack_length + 1));
	if (pops > 0) {
		e = duckLisp_emit_pop(duckLisp, compileState, assembly, pops);
		if (e) goto cleanup;
	}

	// And pop it... This is so much easier than it used to be. No more queuing `pop-scope`s.
	e = duckLisp_popScope(duckLisp, compileState, dl_null);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

/*
  =======
  Compile
  =======
*/

dl_error_t duckLisp_compile_compoundExpression(duckLisp_t *duckLisp,
                                               duckLisp_compileState_t *compileState,
                                               dl_array_t *assembly,
                                               char *functionName,
                                               const dl_size_t functionName_length,
                                               duckLisp_ast_compoundExpression_t *compoundExpression,
                                               dl_ptrdiff_t *index,
                                               duckLisp_ast_type_t *type,
                                               dl_bool_t pushReference) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t temp_index;
	duckLisp_ast_type_t temp_type;

	switch (compoundExpression->type) {
	case duckLisp_ast_type_bool:
		e = duckLisp_emit_pushBoolean(duckLisp,
		                              compileState,
		                              assembly,
		                              &temp_index,
		                              compoundExpression->value.boolean.value);
		if (e) goto cleanup;
		temp_type = duckLisp_ast_type_bool;
		break;
	case duckLisp_ast_type_int:
		e = duckLisp_emit_pushInteger(duckLisp,
		                              compileState,
		                              assembly,
		                              &temp_index,
		                              compoundExpression->value.integer.value);
		if (e) goto cleanup;
		temp_type = duckLisp_ast_type_int;
		break;
	case duckLisp_ast_type_float:
		e = duckLisp_emit_pushDoubleFloat(duckLisp,
		                                  compileState,
		                                  assembly,
		                                  &temp_index,
		                                  compoundExpression->value.floatingPoint.value);
		if (e) goto cleanup;
		temp_type = duckLisp_ast_type_float;
		break;
	case duckLisp_ast_type_string:
		e = duckLisp_emit_pushString(duckLisp,
		                             compileState,
		                             assembly,
		                             &temp_index,
		                             compoundExpression->value.string.value,
		                             compoundExpression->value.string.value_length);
		if (e) goto cleanup;
		temp_type = duckLisp_ast_type_string;
		break;
	case duckLisp_ast_type_identifier:
		e = duckLisp_scope_getLocalIndexFromName(compileState->currentCompileState,
		                                         &temp_index,
		                                         compoundExpression->value.identifier.value,
		                                         compoundExpression->value.identifier.value_length);
		if (e) goto cleanup;
		if (temp_index == -1) {
			dl_ptrdiff_t scope_index;
			dl_bool_t found;
			e = duckLisp_scope_getFreeLocalIndexFromName(duckLisp,
			                                             compileState->currentCompileState,
			                                             &found,
			                                             &temp_index,
			                                             &scope_index,
			                                             compoundExpression->value.identifier.value,
			                                             compoundExpression->value.identifier.value_length);
			if (e) goto cleanup;
			if (!found) {
				/* Attempt to find a global. Only globals registered with the compiler will be found here. */
				temp_index = duckLisp_symbol_nameToValue(duckLisp,
				                                         compoundExpression->value.identifier.value,
				                                         compoundExpression->value.identifier.value_length);
				if (e) goto cleanup;
				if (temp_index == -1) {
					/* Maybe it's a global that hasn't been defined yet? */
					e = dl_array_pushElements(&eString, DL_STR("compoundExpression: Could not find variable \""));
					if (e) goto cleanup;
					e = dl_array_pushElements(&eString,
					                          compoundExpression->value.identifier.value,
					                          compoundExpression->value.identifier.value_length);
					if (e) goto cleanup;
					e = dl_array_pushElements(&eString, DL_STR("\" in lexical scope. Assuming dynamic scope."));
					if (e) goto cleanup;
					e = duckLisp_error_pushRuntime(duckLisp,
					                               eString.elements,
					                               eString.elements_length * eString.element_size);
					if (e) goto cleanup;
					/* Register global (symbol) and then use it. */
					e = duckLisp_symbol_create(duckLisp,
					                           compoundExpression->value.identifier.value,
					                           compoundExpression->value.identifier.value_length);
					if (e) goto cleanup;
					dl_ptrdiff_t key = duckLisp_symbol_nameToValue(duckLisp,
					                                               compoundExpression->value.identifier.value,
					                                               (compoundExpression
					                                                ->value.identifier.value_length));
					e = duckLisp_emit_pushGlobal(duckLisp, compileState, assembly, key);
					if (e) goto cleanup;
					temp_index = getLocalsLength(compileState) - 1;
				}
				else {
					e = duckLisp_emit_pushGlobal(duckLisp, compileState, assembly, temp_index);
					if (e) goto cleanup;
					temp_index = getLocalsLength(compileState) - 1;
				}
			}
			else {
				/* Now the trick here is that we need to mirror the free variable as a local variable.
				   Actually, scratch that. We need to simply push the UV. Creating it as a local variable is an
				   optimization that can be done in `duckLisp_compile_expression`. It can't be done here. */
				e = duckLisp_emit_pushUpvalue(duckLisp, compileState, assembly, temp_index);
				if (e) goto cleanup;
				temp_index = getLocalsLength(compileState) - 1;
			}
		}
		else if (pushReference) {
			// We are NOT pushing an index since the index is part of the instruction.
			e = duckLisp_emit_pushIndex(duckLisp, compileState, assembly, temp_index);
			if (e) goto cleanup;
		}

		temp_type = duckLisp_ast_type_none;  // Let's use `none` as a wildcard. Variables do not have a set type.
		break;
	case duckLisp_ast_type_expression:
		temp_index = -1;
		e = duckLisp_compile_expression(duckLisp,
		                                compileState,
		                                assembly,
		                                functionName,
		                                functionName_length,
		                                &compoundExpression->value.expression,
		                                &temp_index);
		if (e) goto cleanup;
		if (temp_index == -1) temp_index = getLocalsLength(compileState) - 1;
		temp_type = duckLisp_ast_type_none;
		break;
	default:
		temp_type = duckLisp_ast_type_none;
		e = dl_array_pushElements(&eString, functionName, functionName_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

	if (index != dl_null) *index = temp_index;
	if (type != dl_null) *type = temp_type;

 cleanup:
	eError = dl_array_quit(&eString);
	if (eError) e = eError;
	return e;
}

dl_error_t duckLisp_compile_expression(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       char *functionName,
                                       const dl_size_t functionName_length,
                                       duckLisp_ast_expression_t *expression,
                                       dl_ptrdiff_t *index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_functionType_t functionType;
	dl_ptrdiff_t functionIndex = -1;

	dl_error_t (*generatorCallback)(duckLisp_t*, duckLisp_compileState_t*, dl_array_t*, duckLisp_ast_expression_t*);

	if (expression->compoundExpressions_length == 0) {
		e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		goto cleanup;
	}

	// Compile!
	duckLisp_ast_identifier_t name = expression->compoundExpressions[0].value.identifier;
	switch (expression->compoundExpressions[0].type) {
	case duckLisp_ast_type_bool:
	case duckLisp_ast_type_int:
	case duckLisp_ast_type_float:
		/* e = dl_error_invalidValue; */
		/* eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Constants as function names are not supported.")); */
		/* if (eError) { */
		/*		e = eError; */
		/* } */
		/* goto l_cleanup; */
	case duckLisp_ast_type_string:
		/* // Assume this is a comment or something. Maybe even returning a string. */
		/* break; */
	case duckLisp_ast_type_expression:
		// Run expression generator.
		e = duckLisp_generator_expression(duckLisp, compileState, assembly, expression);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_identifier:
		/* Determine function type. */
		functionType = duckLisp_functionType_none;
		e = scope_getFunctionFromName(duckLisp,
		                              compileState->currentCompileState,
		                              &functionType,
		                              &functionIndex,
		                              name.value,
		                              name.value_length);
		if (e) goto cleanup;
		if (functionType != duckLisp_functionType_macro) {
			dl_ptrdiff_t index = -1;
			e = duckLisp_scope_getLocalIndexFromName(compileState->currentCompileState,
			                                         &index,
			                                         name.value,
			                                         name.value_length);
			if (e) goto cleanup;
			if (index == -1) {
				dl_bool_t found = dl_false;
				dl_ptrdiff_t scope_index;
				e = duckLisp_scope_getFreeLocalIndexFromName(duckLisp,
				                                             compileState->currentCompileState,
				                                             &found,
				                                             &index,
				                                             &scope_index,
				                                             name.value,
				                                             name.value_length);
				if (e) goto cleanup;
				if (found) functionType = duckLisp_functionType_ducklisp;
			}
			else {
				functionType = duckLisp_functionType_ducklisp;
			}
		}
		/* No need to check if it's a pure function since `functionType` is only explicitly set a few lines
		   above. */
		if (functionType != duckLisp_functionType_ducklisp) {
			e = scope_getFunctionFromName(duckLisp,
			                              compileState->currentCompileState,
			                              &functionType,
			                              &functionIndex,
			                              name.value,
			                              name.value_length);
			if (e) goto cleanup;
			if (functionType == duckLisp_functionType_none) {
				e = dl_error_ok;
				eError = dl_array_pushElements(&eString, functionName, functionName_length);
				if (eError) e = eError;
				eError = dl_array_pushElements(&eString, DL_STR(": Could not find variable \""));
				if (eError) e = eError;
				eError = dl_array_pushElements(&eString,
				                               name.value,
				                               name.value_length);
				if (eError) e = eError;
				eError = dl_array_pushElements(&eString, DL_STR("\". Assuming dynamic scope."));
				if (eError) e = eError;
				eError = duckLisp_error_pushRuntime(duckLisp,
				                                    eString.elements,
				                                    eString.elements_length * eString.element_size);
				if (eError) e = eError;
				if (e) goto cleanup;
				/* goto l_cleanup; */
				functionType = duckLisp_functionType_ducklisp_pure;
			}
		}
		/* Compile function. */
		switch (functionType) {
		case duckLisp_functionType_ducklisp:
			/* Fall through */
		case duckLisp_functionType_ducklisp_pure:
			e = duckLisp_generator_funcall(duckLisp, compileState, assembly, expression);
			if (e) goto cleanup;
			break;
		case duckLisp_functionType_c:
			e = duckLisp_generator_callback(duckLisp, compileState, assembly, expression);
			if (e) goto cleanup;
			break;
		case duckLisp_functionType_generator:
			e = dl_array_get(&duckLisp->generators_stack, &generatorCallback, functionIndex);
			if (e) goto cleanup;
			e = generatorCallback(duckLisp, compileState, assembly, expression);
			if (e) goto cleanup;
			break;
		case duckLisp_functionType_macro:
			e = duckLisp_generator_macro(duckLisp, compileState, assembly, expression, index);
			break;
		default:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid function type. Can't happen."));
			if (eError) e = eError;
			goto cleanup;
		}
		break;
	default:
		e = dl_array_pushElements(&eString, functionName, functionName_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) goto cleanup;
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto cleanup;
	}

 cleanup:
	eError = dl_array_quit(&eString);
	if (eError) e = eError;
	return e;
}

static dl_error_t assembly_quit(duckLisp_t *duckLisp, dl_array_t *assembly) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction;
	dl_size_t length = assembly->elements_length;
	DL_DOTIMES(i, length) {
		e = dl_array_popElement(assembly, &instruction);
		if (e) goto cleanup;
		e = duckLisp_instructionObject_quit(duckLisp, &instruction);
		if (e) goto cleanup;
	}
	e = dl_array_quit(assembly);
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_compileAST(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *bytecode,  /* dl_array_t:dl_uint8_t */
                               duckLisp_ast_compoundExpression_t astCompoundexpression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Not initialized until later. This is reused for multiple arrays. */
	dl_array_t *assembly = dl_null; /* dl_array_t:duckLisp_instructionObject_t */
	/* expression stack for navigating the tree. */
	dl_array_t expressionStack;
	/**/ dl_array_init(&expressionStack,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_ast_compoundExpression_t),
	                   dl_array_strategy_double);

	/**/ dl_array_init(bytecode, duckLisp->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);


	/* * * * * *
	 * Compile *
	 * * * * * */


	/* putchar('\n'); */

	/* First stage: Create assembly tree from AST. */

	/* Stack length is zero. */

	compileState->currentCompileState->label_number = 0;

	assembly = &compileState->currentCompileState->assembly;

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        DL_STR("compileAST"),
	                                        &astCompoundexpression,
	                                        dl_null,
	                                        dl_null,
	                                        dl_true);
	if (e) goto cleanup;

	e = duckLisp_emit_return(duckLisp,
	                         compileState,
	                         assembly,
	                         ((getLocalsLength(compileState) == 0) ? 0 : getLocalsLength(compileState) - 1));
	if (e) goto cleanup;

	// Print list.
	/* printf("\n"); */

	/* { */
	/* 	dl_size_t tempDlSize; */
	/* 	/\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* 	printf("Compiler memory usage (post compilation): %llu/%llu (%llu%%)\n\n", */
	/* 	       tempDlSize, */
	/* 	       duckLisp->memoryAllocation->size, */
	/* 	       100*tempDlSize/duckLisp->memoryAllocation->size); */
	/* } */

	/* dl_array_t ia; */
	/* duckLisp_instructionObject_t io; */
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < instructionList.elements_length; i++) { */
	/*		ia = DL_ARRAY_GETADDRESS(instructionList, dl_array_t, i); */
	/* printf("{\n"); */
	/* for (dl_ptrdiff_t j = 0; (dl_size_t) j < assembly.elements_length; j++) { */
	/* 	io = DL_ARRAY_GETADDRESS(assembly, duckLisp_instructionObject_t, j); */
	/* 	/\* printf("{\n"); *\/ */
	/* 	printf("Instruction class: %s", */
	/* 	       (char *[38]){ */
	/* 		       ""  // Trick the formatter. */
	/* 			       "nop", */
	/* 			       "push-string", */
	/* 			       "push-boolean", */
	/* 			       "push-integer", */
	/* 			       "push-index", */
	/* 			       "push-symbol", */
	/* 			       "push-upvalue", */
	/* 			       "push-closure", */
	/* 			       "set-upvalue", */
	/* 			       "release-upvalues", */
	/* 			       "funcall", */
	/* 			       "apply", */
	/* 			       "call", */
	/* 			       "ccall", */
	/* 			       "acall", */
	/* 			       "jump", */
	/* 			       "brz", */
	/* 			       "brnz", */
	/* 			       "move", */
	/* 			       "not", */
	/* 			       "mul", */
	/* 			       "div", */
	/* 			       "add", */
	/* 			       "sub", */
	/* 			       "equal", */
	/* 			       "less", */
	/* 			       "greater", */
	/* 			       "cons", */
	/* 			       "car", */
	/* 			       "cdr", */
	/* 			       "set-car", */
	/* 			       "set-cdr", */
	/* 			       "null?", */
	/* 			       "type-of", */
	/* 			       "pop", */
	/* 			       "return", */
	/* 			       "nil", */
	/* 			       "push-label", */
	/* 			       "label", */
	/* 			       }[io.instructionClass]); */
	/* 	/\* printf("[\n"); *\/ */
	/* 	duckLisp_instructionArgClass_t ia; */
	/* 	if (io.args.elements_length == 0) { */
	/* 		putchar('\n'); */
	/* 	} */
	/* 	for (dl_ptrdiff_t k = 0; (dl_size_t) k < io.args.elements_length; k++) { */
	/* 		putchar('\n'); */
	/* 		ia = ((duckLisp_instructionArgClass_t *) io.args.elements)[k]; */
	/* 		/\* printf("		   {\n"); *\/ */
	/* 		printf("		Type: %s", */
	/* 		       (char *[4]){ */
	/* 			       "none", */
	/* 			       "integer", */
	/* 			       "index", */
	/* 			       "string", */
	/* 		       }[ia.type]); */
	/* 		putchar('\n'); */
	/* 		printf("		Value: "); */
	/* 		switch (ia.type) { */
	/* 		case duckLisp_instructionArgClass_type_none: */
	/* 			printf("None"); */
	/* 			break; */
	/* 		case duckLisp_instructionArgClass_type_integer: */
	/* 			printf("%i", ia.value.integer); */
	/* 			break; */
	/* 		case duckLisp_instructionArgClass_type_index: */
	/* 			printf("%llu", ia.value.index); */
	/* 			break; */
	/* 		case duckLisp_instructionArgClass_type_string: */
	/* 			printf("\""); */
	/* 			for (dl_ptrdiff_t m = 0; (dl_size_t) m < ia.value.string.value_length; m++) { */
	/* 				switch (ia.value.string.value[m]) { */
	/* 				case '\n': */
	/* 					putchar('\\'); */
	/* 					putchar('n'); */
	/* 					break; */
	/* 				default: */
	/* 					putchar(ia.value.string.value[m]); */
	/* 				} */
	/* 			} */
	/* 			printf("\""); */
	/* 			break; */
	/* 		default: */
	/* 			printf("		Undefined type.\n"); */
	/* 		} */
	/* 		putchar('\n'); */
	/* 	} */
	/* 	printf("\n"); */
	/* 	/\* printf("}\n"); *\/ */
	/* } */
	/* printf("}\n"); */
	/* } */

	e = duckLisp_assemble(duckLisp, compileState, bytecode, assembly);
	if (e) goto cleanup;

	/* * * * * *
	 * Cleanup *
	 * * * * * */

 cleanup:

	/* putchar('\n'); */

	eError = duckLisp_compileState_quit(duckLisp, compileState);
	if (eError) e = eError;

	eError = dl_array_quit(&expressionStack);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

/*
  ================
  Public functions
  ================
*/

dl_error_t duckLisp_callback_gensym(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_t *duckLisp = duckVM->duckLisp;
	duckVM_object_t object;
	duckVM_object_t *objectPointer;
	duckLisp_ast_identifier_t identifier;

	e = duckLisp_gensym(duckLisp, &identifier);
	if (e) goto cleanup;

	e = duckLisp_symbol_create(duckLisp, identifier.value, identifier.value_length);
	if (e) goto cleanup;

	object.type = duckVM_object_type_internalString;
	object.value.internalString.value_length = identifier.value_length;
	object.value.internalString.value = (dl_uint8_t *) identifier.value;
	e = duckVM_allocateHeapObject(duckVM, &objectPointer, object);
	if (e) goto cleanup;
	object.type = duckVM_object_type_symbol;
	object.value.symbol.id = duckLisp_symbol_nameToValue(duckLisp, identifier.value, identifier.value_length);
	object.value.symbol.internalString = objectPointer;
	e = duckVM_push(duckVM, &object);
	if (e) goto cleanup;
 cleanup:
	if (identifier.value) {
		eError = DL_FREE(duckLisp->memoryAllocation, &identifier.value);
		if (eError) e = eError;
	}
	return e;
}

dl_error_t duckLisp_init(duckLisp_t *duckLisp,
                         dl_memoryAllocation_t *memoryAllocation,
                         dl_size_t maxComptimeVmObjects) {
	dl_error_t error = dl_error_ok;

	/* All language-defined generators go here. */
	struct {
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, duckLisp_compileState_t *, dl_array_t*, duckLisp_ast_expression_t*);
	} generators[] = {{DL_STR("__nop"),                    duckLisp_generator_nop},
	                  {DL_STR("__funcall"),                duckLisp_generator_funcall2},
	                  {DL_STR("__apply"),                  duckLisp_generator_apply},
	                  {DL_STR("__label"),                  duckLisp_generator_label},
	                  {DL_STR("__var"),                    duckLisp_generator_createVar},
	                  {DL_STR("__global"),                 duckLisp_generator_static},
	                  {DL_STR("__setq"),                   duckLisp_generator_setq},
	                  {DL_STR("__not"),                    duckLisp_generator_not},
	                  {DL_STR("__*"),                      duckLisp_generator_multiply},
	                  {DL_STR("__/"),                      duckLisp_generator_divide},
	                  {DL_STR("__+"),                      duckLisp_generator_add},
	                  {DL_STR("__-"),                      duckLisp_generator_sub},
	                  {DL_STR("__while"),                  duckLisp_generator_while},
	                  {DL_STR("__if"),                     duckLisp_generator_if},
	                  {DL_STR("__when"),                   duckLisp_generator_when},
	                  {DL_STR("__unless"),                 duckLisp_generator_unless},
	                  {DL_STR("__="),                      duckLisp_generator_equal},
	                  {DL_STR("__<"),                      duckLisp_generator_less},
	                  {DL_STR("__>"),                      duckLisp_generator_greater},
	                  {DL_STR("__defun"),                  duckLisp_generator_defun},
	                  {DL_STR("\0defun:lambda"),           duckLisp_generator_lambda},
	                  {DL_STR("\0defmacro:lambda"),        duckLisp_generator_lambda},
	                  {DL_STR("__lambda"),                 duckLisp_generator_lambda},
	                  {DL_STR("__defmacro"),               duckLisp_generator_defmacro},
	                  {DL_STR("__noscope"),                duckLisp_generator_noscope2},
	                  {DL_STR("__comptime"),               duckLisp_generator_comptime},
	                  {DL_STR("__quote"),                  duckLisp_generator_quote},
	                  {DL_STR("__list"),                   duckLisp_generator_list},
	                  {DL_STR("__vector"),                 duckLisp_generator_vector},
	                  {DL_STR("__make-vector"),            duckLisp_generator_makeVector},
	                  {DL_STR("__get-vector-element"),     duckLisp_generator_getVecElt},
	                  {DL_STR("__set-vector-element"),     duckLisp_generator_setVecElt},
	                  {DL_STR("__cons"),                   duckLisp_generator_cons},
	                  {DL_STR("__car"),                    duckLisp_generator_car},
	                  {DL_STR("__cdr"),                    duckLisp_generator_cdr},
	                  {DL_STR("__set-car"),                duckLisp_generator_setCar},
	                  {DL_STR("__set-cdr"),                duckLisp_generator_setCdr},
	                  {DL_STR("__null?"),                  duckLisp_generator_nullp},
	                  {DL_STR("__type-of"),                duckLisp_generator_typeof},
	                  {DL_STR("__make-type"),              duckLisp_generator_makeType},
	                  {DL_STR("__make-instance"),          duckLisp_generator_makeInstance},
	                  {DL_STR("__composite-value"),        duckLisp_generator_compositeValue},
	                  {DL_STR("__composite-function"),     duckLisp_generator_compositeFunction},
	                  {DL_STR("__set-composite-value"),    duckLisp_generator_setCompositeValue},
	                  {DL_STR("__set-composite-function"), duckLisp_generator_setCompositeFunction},
	                  {DL_STR("__make-string"),            duckLisp_generator_makeString},
	                  {DL_STR("__concatenate"),            duckLisp_generator_concatenate},
	                  {DL_STR("__substring"),              duckLisp_generator_substring},
	                  {DL_STR("__length"),                 duckLisp_generator_length},
	                  {DL_STR("__symbol-string"),          duckLisp_generator_symbolString},
	                  {DL_STR("__symbol-id"),              duckLisp_generator_symbolId},
	                  {DL_STR("__error"),                  duckLisp_generator_error},
	                  {dl_null, 0,                         dl_null}};

	struct {
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
	} callbacks[] = {
		{DL_STR("gensym"), duckLisp_callback_gensym},
		{dl_null, 0,       dl_null}
	};

	duckLisp->memoryAllocation = memoryAllocation;

#ifdef USE_DATALOGGING
	duckLisp->datalog.total_bytes_generated = 0;
	duckLisp->datalog.total_instructions_generated = 0;
	duckLisp->datalog.jumpsize_bytes_removed = 0;
	duckLisp->datalog.pushpop_instructions_removed = 0;
#endif /* USE_DATALOGGING */

	/* No error */ dl_array_init(&duckLisp->errors,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_error_t),
	                             dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->generators_stack,
	                             duckLisp->memoryAllocation,
	                             sizeof(dl_error_t (*)(duckLisp_t*, duckLisp_ast_expression_t*)),
	                             dl_array_strategy_double);
	/**/ dl_trie_init(&duckLisp->generators_trie, duckLisp->memoryAllocation, -1);
	duckLisp->generators_length = 0;
	/**/ dl_trie_init(&duckLisp->callbacks_trie, duckLisp->memoryAllocation, -1);
	/* No error */ dl_array_init(&duckLisp->symbols_array,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_ast_identifier_t),
	                             dl_array_strategy_double);
	/* No error */ dl_trie_init(&duckLisp->symbols_trie, duckLisp->memoryAllocation, -1);

	duckLisp->gensym_number = 0;

	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		error = duckLisp_addGenerator(duckLisp, generators[i].callback, generators[i].name, generators[i].name_length);
		if (error) {
			printf("Could not register generator. (%s)\n", dl_errorString[error]);
			goto cleanup;
		}
	}

	error = duckVM_init(&duckLisp->vm, duckLisp->memoryAllocation, maxComptimeVmObjects);
	if (error) goto cleanup;

	duckLisp->vm.duckLisp = duckLisp;

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		error = duckLisp_linkCFunction(duckLisp, callbacks[i].callback, callbacks[i].name, callbacks[i].name_length);
		if (error) {
			printf("Could not create function. (%s)\n", dl_errorString[error]);
			goto cleanup;
		}
	}

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		error = duckVM_linkCFunction(&duckLisp->vm,
		                         duckLisp_symbol_nameToValue(duckLisp, callbacks[i].name, callbacks[i].name_length),
		                         callbacks[i].callback);
		if (error) {
			printf("Could not link callback into VM. (%s)\n", dl_errorString[error]);
			goto cleanup;
		}
	}

 cleanup:
	return error;
}

void duckLisp_quit(duckLisp_t *duckLisp) {
	dl_error_t e;

	/**/ duckVM_quit(&duckLisp->vm);
	duckLisp->gensym_number = 0;
	e = dl_array_quit(&duckLisp->generators_stack);
	/**/ dl_trie_quit(&duckLisp->generators_trie);
	duckLisp->generators_length = 0;
	/**/ dl_trie_quit(&duckLisp->callbacks_trie);
	e = dl_trie_quit(&duckLisp->symbols_trie);
	DL_DOTIMES(i, duckLisp->symbols_array.elements_length) {
		e = DL_FREE(duckLisp->memoryAllocation,
		            &DL_ARRAY_GETADDRESS(duckLisp->symbols_array, duckLisp_ast_identifier_t, i).value);
	}
	e = dl_array_quit(&duckLisp->symbols_array);
	DL_DOTIMES(i, duckLisp->errors.elements_length) {
		e = DL_FREE(duckLisp->memoryAllocation,
		            &DL_ARRAY_GETADDRESS(duckLisp->errors, duckLisp_error_t, i).message);
	}
	e = dl_array_quit(&duckLisp->errors);
	(void) e;
}


dl_error_t duckLisp_ast_print(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t ast) {
	dl_error_t e = dl_error_ok;

	e = ast_print_compoundExpression(*duckLisp, ast);
	putchar('\n');
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}


void duckLisp_subCompileState_init(dl_memoryAllocation_t *memoryAllocation,
                                   duckLisp_subCompileState_t *subCompileState) {
	subCompileState->label_number = 0;
	subCompileState->locals_length = 0;
	/**/ dl_array_init(&subCompileState->scope_stack,
	                   memoryAllocation,
	                   sizeof(duckLisp_scope_t),
	                   dl_array_strategy_double);
	/**/ dl_array_init(&subCompileState->assembly,
	                   memoryAllocation,
	                   sizeof(duckLisp_instructionObject_t),
	                   dl_array_strategy_double);
}

dl_error_t duckLisp_subCompileState_quit(duckLisp_t *duckLisp, duckLisp_subCompileState_t *subCompileState) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	e = dl_array_quit(&subCompileState->scope_stack);
	eError = assembly_quit(duckLisp, &subCompileState->assembly);
	if (eError) e = eError;
	return e;
}


void duckLisp_compileState_init(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState) {
	/**/ duckLisp_subCompileState_init(duckLisp->memoryAllocation, &compileState->runtimeCompileState);
	/**/ duckLisp_subCompileState_init(duckLisp->memoryAllocation, &compileState->comptimeCompileState);
	compileState->currentCompileState = &compileState->runtimeCompileState;
}

dl_error_t duckLisp_compileState_quit(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	compileState->currentCompileState = dl_null;
	e = duckLisp_subCompileState_quit(duckLisp, &compileState->comptimeCompileState);
	eError = duckLisp_subCompileState_quit(duckLisp, &compileState->runtimeCompileState);
	return (e ? e : eError);
}


/*
  Creates a function from a string in the current scope.
*/
dl_error_t duckLisp_loadString(duckLisp_t *duckLisp,
                               unsigned char **bytecode,
                               dl_size_t *bytecode_length,
                               const char *source,
                               const dl_size_t source_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = -1;
	duckLisp_ast_compoundExpression_t ast;
	dl_array_t bytecodeArray;
	dl_bool_t result = dl_false;

	/**/ ast_compoundExpression_init(&ast);

	// Trim whitespace from the beginning of the file.
	do {
		if (dl_string_isSpace(*source)) source++;
	} while (result);

	index = 0;

	/* Parse. */

	e = duckLisp_read(duckLisp, source, source_length, &ast, index, dl_true);
	if (e) goto cleanup;

	/* printf("AST: "); */
	/* e = ast_print_compoundExpression(*duckLisp, ast); putchar('\n'); */
	if (e) goto cleanup;

	/* { */
	/* 	dl_size_t tempDlSize; */
	/* 	/\**\/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation); */
	/* 	printf("\nCompiler memory usage: %llu/%llu (%llu%%)", */
	/* 	       tempDlSize, */
	/* 	       duckLisp->memoryAllocation->size, */
	/* 	       100*tempDlSize/duckLisp->memoryAllocation->size); */
	/* } */

	/* Compile AST to bytecode. */

	duckLisp_compileState_t compileState;
	duckLisp_compileState_init(duckLisp, &compileState);
	e = duckLisp_compileAST(duckLisp, &compileState, &bytecodeArray, ast);
	if (e) goto cleanup;
	e = duckLisp_compileState_quit(duckLisp, &compileState);
	if (e) goto cleanup;

	*bytecode = ((unsigned char*) bytecodeArray.elements);
	*bytecode_length = bytecodeArray.elements_length;

 cleanup:
	if (e) {
		*bytecode_length = 0;
	}

	eError = ast_compoundExpression_quit(duckLisp, &ast);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_scope_addObject(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    const char *name,
                                    const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	/* Stick name and index in the current scope's trie. */
	e = scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	e = dl_trie_insert(&scope.locals_trie, name, name_length, getLocalsLength(compileState));
	if (e) goto cleanup;

	e = scope_setTop(compileState->currentCompileState, &scope);
	if (e) goto cleanup;

 cleanup:

	return e;
}

dl_error_t duckLisp_addStatic(duckLisp_t *duckLisp,
                              const char *name,
                              const dl_size_t name_length,
                              dl_ptrdiff_t *index) {
	dl_error_t e = dl_error_ok;

	e = duckLisp_symbol_create(duckLisp, name, name_length);
	if (e) goto l_cleanup;
	*index = duckLisp_symbol_nameToValue(duckLisp, name, name_length);

 l_cleanup:

	return e;
}

dl_error_t duckLisp_addInterpretedFunction(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           const duckLisp_ast_identifier_t name,
                                           const dl_bool_t pure) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	/* Stick name and index in the current scope's trie. */
	e = scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto l_cleanup;

	/* Record function type in function trie. */
	e = dl_trie_insert(&scope.functions_trie,
	                   name.value,
	                   name.value_length,
	                   pure ? duckLisp_functionType_ducklisp_pure : duckLisp_functionType_ducklisp);
	if (e) goto l_cleanup;
	/* So simple. :) */

	e = scope_setTop(compileState->currentCompileState, &scope);
	if (e) goto l_cleanup;

 l_cleanup:

	return e;
}

/* Interpreted generator, i.e. a macro. */
dl_error_t duckLisp_addInterpretedGenerator(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            const duckLisp_ast_identifier_t name) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	duckLisp_subCompileState_t *originalSubCompileState = compileState->currentCompileState;

	/* I know I should use a function, but this was too convenient. */
 again: {
		/* Stick name and index in the current scope's trie. */
		e = scope_getTop(duckLisp, compileState->currentCompileState, &scope);
		if (e) goto cleanup;

		e = dl_trie_insert(&scope.functions_trie, name.value, name.value_length, duckLisp_functionType_macro);
		if (e) goto cleanup;

		e = scope_setTop(compileState->currentCompileState, &scope);
		if (e) goto cleanup;
	}
	if (compileState->currentCompileState == &compileState->runtimeCompileState) {
		compileState->currentCompileState = &compileState->comptimeCompileState;
		goto again;
	}

 cleanup:
	compileState->currentCompileState = originalSubCompileState;
	return e;
}

dl_error_t duckLisp_addGenerator(duckLisp_t *duckLisp,
                                 dl_error_t (*callback)(duckLisp_t*,
                                                        duckLisp_compileState_t *,
                                                        dl_array_t*,
                                                        duckLisp_ast_expression_t*),
                                 const char *name,
                                 const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	/* Record the generator stack index. */
	e = dl_trie_insert(&duckLisp->generators_trie, name, name_length, duckLisp->generators_stack.elements_length);
	if (e) goto cleanup;
	duckLisp->generators_length++;
	e = dl_array_pushElement(&duckLisp->generators_stack, &callback);
	if (e) goto cleanup;

 cleanup:
	return e;
}

dl_error_t duckLisp_linkCFunction(duckLisp_t *duckLisp,
                                  dl_error_t (*callback)(duckVM_t *),
                                  const char *name,
                                  const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;
	(void) callback;

	/* Record function type in function trie. */
	/* Keep track of the function by using a symbol as the global's key. */
	e = duckLisp_symbol_create(duckLisp, name, name_length);
	if (e) goto cleanup;
	dl_ptrdiff_t key = duckLisp_symbol_nameToValue(duckLisp, name, name_length);
	e = dl_trie_insert(&duckLisp->callbacks_trie, name, name_length, key);
	if (e) goto cleanup;

	/* Add to the VM's scope. */
	e = duckVM_linkCFunction(&duckLisp->vm, key, callback);
	if (e) goto cleanup;

 cleanup:
	return e;
}
