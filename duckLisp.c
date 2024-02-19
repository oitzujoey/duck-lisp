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

#include "duckLisp.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "DuckLib/sort.h"
#include "DuckLib/trie.h"
#include "parser.h"
#include "emitters.h"
#include "generators.h"
#ifdef USE_PARENTHESIS_INFERENCE
#include "parenthesis-inferrer.h"
#endif /* USE_PARENTHESIS_INFERENCE */

/*
  ===============
  Error reporting
  ===============
*/

dl_error_t duckLisp_error_pushRuntime(duckLisp_t *duckLisp, const dl_uint8_t *message, const dl_size_t message_length) {
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

		e = duckLisp_error_pushRuntime(duckLisp, string.elements, string.elements_length * string.element_size);
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
#define X(s) (dl_uint8_t *) s, sizeof(s) - 1
		{X("duckLisp_ast_type_none")},
		{X("duckLisp_ast_type_expression")},
		{X("duckLisp_ast_type_literalExpression")},
		{X("duckLisp_ast_type_identifier")},
		{X("duckLisp_ast_type_callback")},
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

		e = duckLisp_error_pushRuntime(duckLisp, string.elements, string.elements_length * string.element_size);
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
dl_ptrdiff_t duckLisp_symbol_nameToValue(const duckLisp_t *duckLisp,
                                         const dl_uint8_t *name,
                                         const dl_size_t name_length) {
	dl_ptrdiff_t value = -1;
	/**/ dl_trie_find(duckLisp->symbols_trie, &value, name, name_length);
	return value;
}

/* Guaranteed not to create a new symbol if a symbol with the given name already exists. */
dl_error_t duckLisp_symbol_create(duckLisp_t *duckLisp, const dl_uint8_t *name, const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	dl_ptrdiff_t key = duckLisp_symbol_nameToValue(duckLisp, name, name_length);
	if (key == -1) {
		duckLisp_ast_identifier_t tempIdentifier;
		e = dl_trie_insert(&duckLisp->symbols_trie,
		                   name,
		                   name_length,
		                   duckLisp->symbols_array.elements_length);
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
	/**/ dl_trie_init(&scope->functionLocals_trie, duckLisp->memoryAllocation, -1);
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
	/**/ dl_trie_quit(&scope->functionLocals_trie);
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

dl_error_t duckLisp_scope_getTop(duckLisp_t *duckLisp,
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
		e = duckLisp_scope_getTop(duckLisp, subCompileState, &local_scope);
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
		e = duckLisp_scope_getTop(duckLisp, subCompileState, &local_scope);
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
                                           const dl_uint8_t *name,
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

		/**/ dl_trie_find(scope.functionLocals_trie, index, name, name_length);
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
                                                const dl_uint8_t *name,
                                                const dl_size_t name_length,
                                                const dl_bool_t functionsOnly) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = subCompileState->scope_stack.elements_length;

	dl_ptrdiff_t local_index = -1;
	*index = -1;

	do {
		e = dl_array_get(&subCompileState->scope_stack, (void *) &scope, --(scope_index));
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
			}
			break;
		}

		if (functionsOnly) {
			(void) dl_trie_find(scope.functionLocals_trie, &local_index, name, name_length);
		}
		else {
			(void) dl_trie_find(scope.locals_trie, &local_index, name, name_length);
		}
		if (local_index != -1) {
			*index = local_index;
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
                                                           const dl_uint8_t *name,
                                                           const dl_size_t name_length,
                                                           duckLisp_scope_t function_scope,
                                                           dl_ptrdiff_t function_scope_index,
                                                           const dl_bool_t functionsOnly) {
	dl_error_t e = dl_error_ok;

	// Fix this stupid variable here.
	dl_ptrdiff_t return_index = -1;
	/* First look for an upvalue in the scope immediately above. If it exists, make a normal upvalue to it. If it
	   doesn't exist, search in higher scopes. If it exists, create an upvalue to it in the function below that scope.
	   Then chain upvalues leading to that upvalue through all the nested functions. Stack upvalues will have a positive
	   index. Upvalue upvalues will have a negative index.
	   Scopes will always have positive indices. Functions may have negative indices.
	*/

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

		(void) dl_trie_find(functionsOnly ? scope.functionLocals_trie : scope.locals_trie, index, name, name_length);
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
		                                                    *scope_index,
		                                                    functionsOnly);
		if (e) goto cleanup;
		/* Don't set `index` below here. */
		/* Create a closure to the scope above. */
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
                                                    const dl_uint8_t *name,
                                                    const dl_size_t name_length,
                                                    const dl_bool_t functionsOnly) {
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
	                                                    function_scope_index,
	                                                    functionsOnly);
 cleanup:
	return e;
}

dl_error_t duckLisp_scope_getFunctionFromName(duckLisp_t *duckLisp,
                                              duckLisp_subCompileState_t *subCompileState,
                                              duckLisp_functionType_t *functionType,
                                              dl_ptrdiff_t *index,
                                              const dl_uint8_t *name,
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

dl_error_t duckLisp_scope_getGlobalFromName(duckLisp_t *duckLisp,
                                            dl_ptrdiff_t *symbolId,
                                            const dl_uint8_t *name,
                                            const dl_size_t name_length,
                                            const dl_bool_t isComptime) {
	(void) dl_trie_find(isComptime ? duckLisp->comptimeGlobals_trie : duckLisp->runtimeGlobals_trie, symbolId, name, name_length);
	return dl_error_ok;
}

dl_error_t duckLisp_scope_getLabelFromName(duckLisp_subCompileState_t *subCompileState,
                                           dl_ptrdiff_t *index,
                                           const dl_uint8_t *name,
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

void duckLisp_localsLength_increment(duckLisp_compileState_t *compileState) {
	compileState->currentCompileState->locals_length++;
}

void duckLisp_localsLength_decrement(duckLisp_compileState_t *compileState) {
	--compileState->currentCompileState->locals_length;
}

dl_size_t duckLisp_localsLength_get(duckLisp_compileState_t *compileState) {
	return compileState->currentCompileState->locals_length;
}

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
	const dl_size_t top = 8/4*sizeof(dl_size_t);
	DL_DOTIMES(i, top) {
		identifier->value[1 + ((top - 1) - i)] = dl_nybbleToHexChar((duckLisp->gensym_number >> 4*i)
		                                                                            & 0xF);
	}
	duckLisp->gensym_number++;
	return e;
}

dl_error_t duckLisp_register_label(duckLisp_t *duckLisp,
                                   duckLisp_subCompileState_t *subCompileState,
                                   dl_uint8_t *name,
                                   const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	e = duckLisp_scope_getTop(duckLisp, subCompileState, &scope);
	if (e) goto l_cleanup;

	e = dl_trie_insert(&scope.labels_trie, name, name_length, subCompileState->label_number);
	if (e) goto l_cleanup;
	subCompileState->label_number++;

	e = scope_setTop(subCompileState, &scope);
	if (e) goto l_cleanup;

 l_cleanup:

	return e;
}

/*
  =======
  Compile
  =======
*/

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

dl_error_t duckLisp_astToObject(duckLisp_t *duckLisp,
                                duckVM_t *duckVM,
                                duckVM_object_t *object,
                                duckLisp_ast_compoundExpression_t ast) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	switch (ast.type) {
	case duckLisp_ast_type_expression:
		/* Fall through */
	case duckLisp_ast_type_literalExpression: {
		duckVM_object_t *tailPointer = dl_null;
		DL_DOTIMES(j, ast.value.expression.compoundExpressions_length) {
			duckVM_object_t head;
			e = duckLisp_astToObject(duckLisp,
			                         duckVM,
			                         &head,
			                         (ast.value.expression.compoundExpressions
			                          [ast.value.expression.compoundExpressions_length
			                           - 1
			                           - j]));
			if (e) break;
			duckVM_object_t *headPointer;
			e = duckVM_allocateHeapObject(duckVM, &headPointer, head);
			if (e) break;
			duckVM_object_t tail = duckVM_object_makeCons(headPointer, tailPointer);
			e = duckVM_allocateHeapObject(duckVM, &tailPointer, tail);
			if (e) break;
		}
		object->type = duckVM_object_type_list;
		object->value.list = tailPointer;
		break;
	}
	case duckLisp_ast_type_identifier:
		/* Fall through */
	case duckLisp_ast_type_callback: {
		/* Intern symbol if not already interned. */
		e = duckLisp_symbol_create(duckLisp, ast.value.identifier.value, ast.value.identifier.value_length);
		if (e) break;
		dl_size_t id = duckLisp_symbol_nameToValue(duckLisp,
		                                           ast.value.identifier.value,
		                                           ast.value.identifier.value_length);
		e = duckVM_object_makeSymbol(duckVM, object, id, ast.value.identifier.value, ast.value.identifier.value_length);
		if (e) break;
		break;
	}
	case duckLisp_ast_type_string: {
		e = duckVM_object_makeString(duckVM,
		                             object,
		                             (dl_uint8_t *) ast.value.string.value,
		                             ast.value.string.value_length);
		if (e) break;
		break;
	}
	case duckLisp_ast_type_float:
		object->type = duckVM_object_type_float;
		object->value.floatingPoint = ast.value.floatingPoint.value;
		break;
	case duckLisp_ast_type_int:
		object->type = duckVM_object_type_integer;
		object->value.integer = ast.value.integer.value;
		break;
	case duckLisp_ast_type_bool:
		object->type = duckVM_object_type_bool;
		object->value.boolean = ast.value.boolean.value;
		break;
	default:
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("duckLisp_astToObject: Illegal AST type."));
		if (eError) e = eError;
	}
	if (e) goto cleanup;

 cleanup: return e;
}



dl_error_t duckLisp_compile_compoundExpression(duckLisp_t *duckLisp,
                                               duckLisp_compileState_t *compileState,
                                               dl_array_t *assembly,
                                               dl_uint8_t *functionName,
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

	duckLisp->generators_recursion_depth++;
	if (duckLisp->generators_recursion_depth >= duckLisp->generators_max_recursion_depth) {
		e = dl_error_bufferOverflow;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Max generator recursion depth met."));
		if (eError) e = eError;
		goto cleanup;
	}

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
		                                         compoundExpression->value.identifier.value_length,
		                                         dl_false);
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
			                                             compoundExpression->value.identifier.value_length,
			                                             dl_false);
			if (e) goto cleanup;
			if (!found) {
				/* Attempt to find a global. Only globals registered with the compiler will be found here. */
				e = duckLisp_scope_getGlobalFromName(duckLisp,
				                                     &temp_index,
				                                     compoundExpression->value.identifier.value,
				                                     compoundExpression->value.identifier.value_length,
				                                     (&compileState->comptimeCompileState
				                                      == compileState->currentCompileState));
				if (e) goto cleanup;
				if (temp_index == -1) {
					/* Maybe it's a global that hasn't been defined yet? */
					e = dl_array_pushElements(&eString, DL_STR("compoundExpression: Could not find variable \""));
					if (e) goto cleanup;
					e = dl_array_pushElements(&eString,
					                          compoundExpression->value.identifier.value,
					                          compoundExpression->value.identifier.value_length);
					if (e) goto cleanup;
					e = dl_array_pushElements(&eString, DL_STR("\" in lexical scope. Assuming global scope."));
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
					temp_index = duckLisp_localsLength_get(compileState) - 1;
				}
				else {
					e = duckLisp_emit_pushGlobal(duckLisp, compileState, assembly, temp_index);
					if (e) goto cleanup;
					temp_index = duckLisp_localsLength_get(compileState) - 1;
				}
			}
			else {
				/* Now the trick here is that we need to mirror the free variable as a local variable.
				   Actually, scratch that. We need to simply push the UV. Creating it as a local variable is an
				   optimization that can be done in `duckLisp_compile_expression`. It can't be done here. */
				e = duckLisp_emit_pushUpvalue(duckLisp, compileState, assembly, temp_index);
				if (e) goto cleanup;
				temp_index = duckLisp_localsLength_get(compileState) - 1;
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
		if (temp_index == -1) temp_index = duckLisp_localsLength_get(compileState) - 1;
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
	--duckLisp->generators_recursion_depth;
	return e;
}

/* Figure out what sort of form the current AST node is based on the head of the node.
     Identifier? Then it's a function. Do a functiony thing.
     Anything else? It's a scope. Call the expression generator. */
dl_error_t duckLisp_compile_expression(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       dl_uint8_t *functionName,
                                       const dl_size_t functionName_length,
                                       duckLisp_ast_expression_t *expression,
                                       dl_ptrdiff_t *index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	(void) dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_functionType_t functionType;
	dl_ptrdiff_t functionIndex = -1;

	dl_error_t (*generatorCallback)(duckLisp_t*, duckLisp_compileState_t*, dl_array_t*, duckLisp_ast_expression_t*);

	if (expression->compoundExpressions_length == 0) {
		e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		goto cleanup;
	}

	duckLisp_ast_identifier_t name = expression->compoundExpressions[0].value.identifier;
	switch (expression->compoundExpressions[0].type) {
	case duckLisp_ast_type_bool:
		/* Fall through */
	case duckLisp_ast_type_int:
		/* Fall through */
	case duckLisp_ast_type_float:
		/* Fall through */
	case duckLisp_ast_type_string:
		/* Fall through */
	case duckLisp_ast_type_expression:
		e = duckLisp_generator_expression(duckLisp, compileState, assembly, expression);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_identifier:
		functionType = duckLisp_functionType_none;
		e = duckLisp_scope_getFunctionFromName(duckLisp,
		                                       compileState->currentCompileState,
		                                       &functionType,
		                                       &functionIndex,
		                                       name.value,
		                                       name.value_length);
		if (e) goto cleanup;
		if (functionType == duckLisp_functionType_none) {
			/* A warning, not an error. */
			eError = dl_array_pushElements(&eString, functionName, functionName_length);
			if (eError) e = eError;
			eError = dl_array_pushElements(&eString, DL_STR(": Could not find function \""));
			if (eError) e = eError;
			eError = dl_array_pushElements(&eString,
			                               name.value,
			                               name.value_length);
			if (eError) e = eError;
			eError = dl_array_pushElements(&eString, DL_STR("\". Assuming global scope."));
			if (eError) e = eError;
			eError = duckLisp_error_pushRuntime(duckLisp,
			                                    eString.elements,
			                                    eString.elements_length * eString.element_size);
			if (eError) e = eError;
			if (e) goto cleanup;
			functionType = duckLisp_functionType_ducklisp;
		}
		switch (functionType) {
		case duckLisp_functionType_ducklisp:
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

void duckLisp_assembly_init(duckLisp_t *duckLisp, dl_array_t *assembly) {
	(void) dl_array_init(assembly,
	                     duckLisp->memoryAllocation,
	                     sizeof(duckLisp_instructionObject_t),
	                     dl_array_strategy_double);
}

dl_error_t duckLisp_assembly_quit(duckLisp_t *duckLisp, dl_array_t *assembly) {
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

	duckLisp->generators_recursion_depth = 0;

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

	if (duckLisp_localsLength_get(compileState) > 1) {
		e = duckLisp_emit_move(duckLisp,
		                       compileState,
		                       assembly,
		                       duckLisp_localsLength_get(compileState) - 2,
		                       duckLisp_localsLength_get(compileState) - 1);
		if (e) goto cleanup;
		e = duckLisp_emit_pop(duckLisp, compileState, assembly, 1);
		if (e) goto cleanup;
	}
	e = duckLisp_emit_exit(duckLisp, compileState, assembly);
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

dl_error_t duckLisp_callback_gensym(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_t *duckLisp = duckVM->duckLisp;
	duckLisp_ast_identifier_t identifier;

	e = duckLisp_gensym(duckLisp, &identifier);
	if (e) goto cleanup;

	e = duckLisp_symbol_create(duckLisp, identifier.value, identifier.value_length);
	if (e) goto cleanup;

	e = duckVM_pushSymbol(duckVM,
	                      duckLisp_symbol_nameToValue(duckLisp, identifier.value, identifier.value_length),
	                      (dl_uint8_t *) identifier.value,
	                      identifier.value_length);
	if (e) goto cleanup;
 cleanup:
	if (identifier.value) {
		eError = DL_FREE(duckLisp->memoryAllocation, &identifier.value);
		if (eError) e = eError;
	}
	return e;
}

dl_error_t duckLisp_callback_intern(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_t *duckLisp = duckVM->duckLisp;
	duckVM_object_type_t object_type;

	e = duckVM_typeOf(duckVM, &object_type);
	if (e) goto cleanup;
	if (duckVM_object_type_string != object_type) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM, DL_STR("\"intern\" expects a string argument."));
		if (eError) e = eError;
		goto cleanup;
	}

	{
		dl_uint8_t *string = dl_null;
		dl_size_t string_length = 0;

		e = duckVM_copyString(duckVM, &string, &string_length);
		if (e) goto cleanupString;

		e = duckVM_pop(duckVM);
		if (e) goto cleanupString;

		e = duckLisp_symbol_create(duckLisp, string, string_length);
		if (e) goto cleanupString;

		e = duckVM_pushSymbol(duckVM,
		                      duckLisp_symbol_nameToValue(duckLisp, string, string_length),
		                      string,
		                      string_length);
		if (e) goto cleanupString;

	cleanupString:
		e = DL_FREE(duckVM->memoryAllocation, &string);
		if (e) goto cleanup;
	}

 cleanup: return e;
}

dl_error_t duckLisp_callback_read(duckVM_t *duckVM) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_t *duckLisp = duckVM->duckLisp;
	dl_memoryAllocation_t *memoryAllocation = duckVM->memoryAllocation;

	duckVM_object_type_t booleanObject_type;
	dl_bool_t boolean;

	duckVM_object_type_t stringObject_type;

	e = duckVM_typeOf(duckVM, &booleanObject_type);
	if (e) goto cleanup;
	if (duckVM_object_type_bool != booleanObject_type) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM, DL_STR("Second argument of \"read\" should be a boolean."));
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckVM_copyBoolean(duckVM, &boolean);
	if (e) goto cleanup;
	
	e = duckVM_pop(duckVM);
	if (e) goto cleanup;

#ifndef USE_PARENTHESIS_INFERENCE
	if (boolean) {
		e = dl_error_invalidValue;
		(eError
		 = duckVM_error_pushRuntime(duckVM,
		                            DL_STR("duck-lisp was not built with parenthesis inference, so the second argument of \"read\" must be false.")));
		if (eError) e = eError;
		goto cleanup;
	}
#endif /* USE_PARENTHESIS_INFERENCE */

	e = duckVM_typeOf(duckVM, &stringObject_type);
	if (duckVM_object_type_string != stringObject_type) {
		e = dl_error_invalidValue;
		eError = duckVM_error_pushRuntime(duckVM, DL_STR("First argument of \"read\" should be a string."));
		if (eError) e = eError;
		goto cleanup;
	}

	{
		dl_uint8_t *string = dl_null;
		dl_size_t string_length = 0;

		e = duckVM_copyString(duckVM, &string, &string_length);
		if (e) goto cleanupString;

		e = duckVM_pop(duckVM);
		if (e) goto cleanupString;

		duckVM_object_t astObject;
		dl_error_t status;
		{
			duckLisp_ast_compoundExpression_t ast;
			(void) duckLisp_ast_compoundExpression_init(&ast);

			e = duckLisp_read(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
			                  boolean,
			                  duckLisp->maxInferenceVmObjects,
			                  dl_null,
#endif /* USE_PARENTHESIS_INFERENCE */
			                  DL_STR("<CALLBACK READ>"),
			                  string,
			                  string_length,
			                  &ast,
			                  0,
			                  dl_true);
			if (e) {
				status = e;
				e = dl_error_ok;
				astObject = duckVM_object_makeList(dl_null);
			}
			else {
				status = 0;
				e = duckLisp_astToObject(duckLisp, duckVM, &astObject, ast);
				if (e) goto cleanupAst;
			}

		cleanupAst:
			eError = duckLisp_ast_compoundExpression_quit(memoryAllocation, &ast);
			if (eError) e = eError;
			if (e) goto cleanupString;
		}

		e = duckVM_pushCons(duckVM);
		if (e) goto cleanupString;
		/* stack: (()) */
		e = duckVM_pushInteger(duckVM);
		if (e) goto cleanupString;
		/* stack: (()) 0 */
		e = duckVM_setInteger(duckVM, status);
		if (e) goto cleanupString;
		/* stack: (()) status */
		e = duckVM_setRest(duckVM, -2);
		if (e) goto cleanupString;
		/* stack: (() . status) status */
		e = duckVM_pop(duckVM);
		if (e) goto cleanupString;
		/* stack: (() . status) */
		e = duckVM_object_push(duckVM, &astObject);
		if (e) goto cleanupString;
		/* stack: (() . status) ast */
		e = duckVM_setFirst(duckVM, -2);
		if (e) goto cleanupString;
		/* stack: (ast . status) ast */
		e = duckVM_pop(duckVM);
		if (e) goto cleanupString;
		/* stack: (ast . status) */

	cleanupString:
		eError = DL_FREE(duckVM->memoryAllocation, &string);
		if (eError) e = eError;
		if (e) goto cleanup;
	}

 cleanup: return e;
}

dl_error_t duckLisp_init(duckLisp_t *duckLisp,
                         dl_memoryAllocation_t *memoryAllocation,
                         dl_size_t maxComptimeVmObjects
#ifdef USE_PARENTHESIS_INFERENCE
                         ,
                         dl_size_t maxInferenceVmObjects
#endif /* USE_PARENTHESIS_INFERENCE */
                         ) {
	dl_error_t error = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	(void) dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* All language-defined generators go here. */
	struct {
		dl_uint8_t *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, duckLisp_compileState_t*, dl_array_t*, duckLisp_ast_expression_t*);
		dl_uint8_t *type;
		const dl_size_t type_length;
		dl_uint8_t *declarationScript;
		const dl_size_t declarationScript_length;
	} generators[] = {
		{DL_STR("__declare"), duckLisp_generator_declare, DL_STR("(L L &rest 0 I)"), dl_null, 0},
		{DL_STR("declare"), duckLisp_generator_declare, DL_STR("(L L &rest 0 I)"), dl_null, 0},
		{DL_STR("__nop"), duckLisp_generator_nop, DL_STR("()"), dl_null, 0},
		{DL_STR("nop"), duckLisp_generator_nop, DL_STR("()"), dl_null, 0},
		{DL_STR("__funcall"), duckLisp_generator_funcall2, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("funcall"), duckLisp_generator_funcall2, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("__apply"), duckLisp_generator_apply, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("apply"), duckLisp_generator_apply, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("__var"),
		 duckLisp_generator_createVar_dummy,
		 DL_STR("(L I)"),
		 DL_STR("(__declare-identifier (__infer-and-get-next-argument) (__quote L))")},
		{DL_STR("var"),
		 duckLisp_generator_createVar_dummy,
		 DL_STR("(L I)"),
		 DL_STR("(__declare-identifier (__infer-and-get-next-argument) (__quote L))")},
		{DL_STR("__global"), duckLisp_generator_global, DL_STR("(L I)"), dl_null, 0},
		{DL_STR("global"), duckLisp_generator_global, DL_STR("(L I)"), dl_null, 0},
		{DL_STR("__setq"), duckLisp_generator_setq, DL_STR("(L I)"), dl_null, 0},
		{DL_STR("setq"), duckLisp_generator_setq, DL_STR("(L I)"), dl_null, 0},
		{DL_STR("__not"), duckLisp_generator_not, DL_STR("(I)"), dl_null, 0},
		{DL_STR("not"), duckLisp_generator_not, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__*"), duckLisp_generator_multiply, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("*"), duckLisp_generator_multiply, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__/"), duckLisp_generator_divide, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("/"), duckLisp_generator_divide, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__+"), duckLisp_generator_add, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("+"), duckLisp_generator_add, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__-"), duckLisp_generator_sub, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("-"), duckLisp_generator_sub, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__while"), duckLisp_generator_while, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("while"), duckLisp_generator_while, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("__if"), duckLisp_generator_if, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("if"), duckLisp_generator_if, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("__when"), duckLisp_generator_when, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("when"), duckLisp_generator_when, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("__unless"), duckLisp_generator_unless, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("unless"), duckLisp_generator_unless, DL_STR("(I &rest 1 I)"), dl_null, 0},
		{DL_STR("__="), duckLisp_generator_equal, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("="), duckLisp_generator_equal, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__<"), duckLisp_generator_less, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("<"), duckLisp_generator_less, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__>"), duckLisp_generator_greater, DL_STR("(I I)"), dl_null, 0},
		{DL_STR(">"), duckLisp_generator_greater, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__defun"),
		 duckLisp_generator_defun_dummy,
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
		{DL_STR("defun"),
		 duckLisp_generator_defun_dummy,
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
		{DL_STR("\0defun:lambda"), duckLisp_generator_lambda, dl_null, 0, dl_null, 0},
		{DL_STR("\0defmacro:lambda"), duckLisp_generator_lambda, dl_null, 0, dl_null, 0},
		{DL_STR("__lambda"),
		 duckLisp_generator_lambda,
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
		{DL_STR("lambda"),
		 duckLisp_generator_lambda,
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
		 duckLisp_generator_defmacro,
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
		{DL_STR("defmacro"),
		 duckLisp_generator_defmacro,
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
		{DL_STR("__noscope"), duckLisp_generator_noscope2_dummy, DL_STR("(&rest 0 I)"), dl_null, 0},
		{DL_STR("noscope"), duckLisp_generator_noscope2_dummy, DL_STR("(&rest 0 I)"), dl_null, 0},
		{DL_STR("__comptime"), duckLisp_generator_comptime, DL_STR("(&rest 1 I)"), dl_null, 0},
		{DL_STR("comptime"), duckLisp_generator_comptime, DL_STR("(&rest 1 I)"), dl_null, 0},
		{DL_STR("__quote"), duckLisp_generator_quote, DL_STR("(I)"), dl_null, 0},
		{DL_STR("quote"), duckLisp_generator_quote, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__list"), duckLisp_generator_list, DL_STR("(&rest 0 I)"), dl_null, 0},
		{DL_STR("list"), duckLisp_generator_list, DL_STR("(&rest 0 I)"), dl_null, 0},
		{DL_STR("__vector"), duckLisp_generator_vector, DL_STR("(&rest 0 I)"), dl_null, 0},
		{DL_STR("vector"), duckLisp_generator_vector, DL_STR("(&rest 0 I)"), dl_null, 0},
		{DL_STR("__make-vector"), duckLisp_generator_makeVector, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("make-vector"), duckLisp_generator_makeVector, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__get-vector-element"), duckLisp_generator_getVecElt, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("get-vector-element"), duckLisp_generator_getVecElt, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__set-vector-element"), duckLisp_generator_setVecElt, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("set-vector-element"), duckLisp_generator_setVecElt, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("__cons"), duckLisp_generator_cons, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("cons"), duckLisp_generator_cons, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__car"), duckLisp_generator_car, DL_STR("(I)"), dl_null, 0},
		{DL_STR("car"), duckLisp_generator_car, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__cdr"), duckLisp_generator_cdr, DL_STR("(I)"), dl_null, 0},
		{DL_STR("cdr"), duckLisp_generator_cdr, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__set-car"), duckLisp_generator_setCar, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("set-car"), duckLisp_generator_setCar, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__set-cdr"), duckLisp_generator_setCdr, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("set-cdr"), duckLisp_generator_setCdr, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__null?"), duckLisp_generator_nullp, DL_STR("(I)"), dl_null, 0},
		{DL_STR("null?"), duckLisp_generator_nullp, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__type-of"), duckLisp_generator_typeof, DL_STR("(I)"), dl_null, 0},
		{DL_STR("type-of"), duckLisp_generator_typeof, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__make-type"), duckLisp_generator_makeType, DL_STR("()"), dl_null, 0},
		{DL_STR("make-type"), duckLisp_generator_makeType, DL_STR("()"), dl_null, 0},
		{DL_STR("__make-instance"), duckLisp_generator_makeInstance, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("make-instance"), duckLisp_generator_makeInstance, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("__composite-value"), duckLisp_generator_compositeValue, DL_STR("(I)"), dl_null, 0},
		{DL_STR("composite-value"), duckLisp_generator_compositeValue, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__composite-function"), duckLisp_generator_compositeFunction, DL_STR("(I)"), dl_null, 0},
		{DL_STR("composite-function"), duckLisp_generator_compositeFunction, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__set-composite-value"), duckLisp_generator_setCompositeValue, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("set-composite-value"), duckLisp_generator_setCompositeValue, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__set-composite-function"), duckLisp_generator_setCompositeFunction, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("set-composite-function"), duckLisp_generator_setCompositeFunction, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__make-string"), duckLisp_generator_makeString, DL_STR("(I)"), dl_null, 0},
		{DL_STR("make-string"), duckLisp_generator_makeString, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__concatenate"), duckLisp_generator_concatenate, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("concatenate"), duckLisp_generator_concatenate, DL_STR("(I I)"), dl_null, 0},
		{DL_STR("__substring"), duckLisp_generator_substring, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("substring"), duckLisp_generator_substring, DL_STR("(I I I)"), dl_null, 0},
		{DL_STR("__length"), duckLisp_generator_length, DL_STR("(I)"), dl_null, 0},
		{DL_STR("length"), duckLisp_generator_length, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__symbol-string"), duckLisp_generator_symbolString, DL_STR("(I)"), dl_null, 0},
		{DL_STR("symbol-string"), duckLisp_generator_symbolString, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__symbol-id"), duckLisp_generator_symbolId, DL_STR("(I)"), dl_null, 0},
		{DL_STR("symbol-id"), duckLisp_generator_symbolId, DL_STR("(I)"), dl_null, 0},
		{DL_STR("__error"), duckLisp_generator_error, DL_STR("(I)"), dl_null, 0},
		{dl_null, 0, dl_null, dl_null, 0, dl_null, 0}
	};

	struct {
		dl_uint8_t *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
		dl_uint8_t *typeString;
		const dl_size_t typeString_length;
	} callbacks[] = {
		{DL_STR("gensym"), duckLisp_callback_gensym, DL_STR("()")},
		{DL_STR("intern"), duckLisp_callback_intern, DL_STR("(I)")},
		{DL_STR("read"), duckLisp_callback_read, DL_STR("(I I)")},
		{dl_null, 0, dl_null, dl_null, 0}
	};

	duckLisp->memoryAllocation = memoryAllocation;

#ifdef USE_PARENTHESIS_INFERENCE
	duckLisp->maxInferenceVmObjects = maxInferenceVmObjects;
#endif /* USE_PARENTHESIS_INFERENCE */

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
#ifdef USE_PARENTHESIS_INFERENCE
	(void) dl_array_init(&duckLisp->parenthesisInferrerTypes_array,
	                     duckLisp->memoryAllocation,
	                     sizeof(duckLisp_parenthesisInferrer_declarationPrototype_t),
	                     dl_array_strategy_double);
	(void) dl_array_init(&duckLisp->inferrerLog,
	                     duckLisp->memoryAllocation,
	                     sizeof(dl_uint8_t),
	                     dl_array_strategy_double);
#endif /* USE_PARENTHESIS_INFERENCE */
	/* No error */ dl_trie_init(&duckLisp->comptimeGlobals_trie, duckLisp->memoryAllocation, -1);
	/* No error */ dl_trie_init(&duckLisp->runtimeGlobals_trie, duckLisp->memoryAllocation, -1);
	/* No error */ dl_array_init(&duckLisp->symbols_array,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_ast_identifier_t),
	                             dl_array_strategy_double);
	/* No error */ dl_trie_init(&duckLisp->symbols_trie, duckLisp->memoryAllocation, -1);

	(void) dl_trie_init(&duckLisp->parser_actions_trie, duckLisp->memoryAllocation, -1);
	(void) dl_array_init(&duckLisp->parser_actions_array,
	                     duckLisp->memoryAllocation,
	                     sizeof(dl_error_t (*)(duckLisp_t*, duckLisp_ast_expression_t*)),
	                     dl_array_strategy_double);
	duckLisp->parser_max_recursion_depth = 1000;  /* This is the default. It is OK for the user to change. */
	duckLisp->parser_recursion_depth = 0;  /* Don't change this. */

	duckLisp->generators_max_recursion_depth = 1000;  /* This is the default. It is OK for the user to change. */
	duckLisp->generators_recursion_depth = 0;  /* Don't change this. */

	duckLisp->gensym_number = 0;

	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		error = duckLisp_addGenerator(duckLisp,
		                              generators[i].callback,
		                              generators[i].name,
		                              generators[i].name_length
#ifdef USE_PARENTHESIS_INFERENCE
		                              ,
		                              generators[i].type,
		                              generators[i].type_length,
		                              generators[i].declarationScript,
		                              generators[i].declarationScript_length
#endif /* USE_PARENTHESIS_INFERENCE */
		                              );
		if (error) {
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Could not register generator."));
			if (eError) error = eError;
			goto cleanup;
		}
	}

	error = duckVM_init(&duckLisp->vm, duckLisp->memoryAllocation, maxComptimeVmObjects);
	if (error) goto cleanup;

	duckLisp->vm.duckLisp = duckLisp;

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		error = duckLisp_linkCFunction(duckLisp,
		                               callbacks[i].callback,
		                               callbacks[i].name,
		                               callbacks[i].name_length
#ifdef USE_PARENTHESIS_INFERENCE
		                               ,
		                               callbacks[i].typeString,
		                               callbacks[i].typeString_length
#endif /* USE_PARENTHESIS_INFERENCE */
		                               );
		if (error) {
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Could not create C callback."));
			if (eError) error = eError;
			goto cleanup;
		}
	}

	for (dl_ptrdiff_t i = 0; callbacks[i].name != dl_null; i++) {
		error = duckVM_linkCFunction(&duckLisp->vm,
		                         duckLisp_symbol_nameToValue(duckLisp, callbacks[i].name, callbacks[i].name_length),
		                         callbacks[i].callback);
		if (error) {
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Could not link callback into VM."));
			if (eError) error = eError;
			goto cleanup;
		}
	}

 cleanup:
	eError = dl_array_quit(&eString);
	if (eError) error = eError;
	return error;
}

void duckLisp_quit(duckLisp_t *duckLisp) {
	dl_error_t e;

	dl_memoryAllocation_t *memoryAllocation = duckLisp->memoryAllocation;
	/**/ duckVM_quit(&duckLisp->vm);
	duckLisp->gensym_number = 0;
	e = dl_array_quit(&duckLisp->generators_stack);
	/**/ dl_trie_quit(&duckLisp->generators_trie);
	duckLisp->generators_length = 0;
	/**/ dl_trie_quit(&duckLisp->callbacks_trie);
#ifdef USE_PARENTHESIS_INFERENCE
	DL_DOTIMES(i, duckLisp->parenthesisInferrerTypes_array.elements_length) {
		duckLisp_parenthesisInferrer_declarationPrototype_t prototype;
		e = dl_array_get(&duckLisp->parenthesisInferrerTypes_array, &prototype, i);
		e = DL_FREE(memoryAllocation, &prototype.name);
		e = DL_FREE(memoryAllocation, &prototype.type);
		if (prototype.script != dl_null) {
			e = DL_FREE(memoryAllocation, &prototype.script);
		}
	}
	e = dl_array_quit(&duckLisp->parenthesisInferrerTypes_array);
	e = dl_array_quit(&duckLisp->inferrerLog);
#endif /* USE_PARENTHESIS_INFERENCE */
	e = dl_trie_quit(&duckLisp->comptimeGlobals_trie);
	e = dl_trie_quit(&duckLisp->runtimeGlobals_trie);
	e = dl_trie_quit(&duckLisp->symbols_trie);
	DL_DOTIMES(i, duckLisp->symbols_array.elements_length) {
		e = DL_FREE(memoryAllocation,
		            &DL_ARRAY_GETADDRESS(duckLisp->symbols_array, duckLisp_ast_identifier_t, i).value);
	}
	e = dl_array_quit(&duckLisp->symbols_array);
	DL_DOTIMES(i, duckLisp->errors.elements_length) {
		e = DL_FREE(memoryAllocation,
		            &DL_ARRAY_GETADDRESS(duckLisp->errors, duckLisp_error_t, i).message);
	}
	e = dl_array_quit(&duckLisp->errors);
	e = dl_trie_quit(&duckLisp->parser_actions_trie);
	e = dl_array_quit(&duckLisp->parser_actions_array);
	(void) e;
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
	eError = duckLisp_assembly_quit(duckLisp, &subCompileState->assembly);
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
#ifdef USE_PARENTHESIS_INFERENCE
                               const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                               dl_uint8_t **bytecode,
                               dl_size_t *bytecode_length,
                               const dl_uint8_t *source,
                               const dl_size_t source_length,
                               const dl_uint8_t *fileName,
                               const dl_size_t fileName_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = -1;
	duckLisp_ast_compoundExpression_t ast;
	dl_array_t bytecodeArray;
	dl_bool_t result = dl_false;

	/**/ duckLisp_ast_compoundExpression_init(&ast);

	// Trim whitespace from the beginning of the file.
	do {
		if (dl_string_isSpace(*source)) source++;
	} while (result);

	index = 0;

	/* Parse. */

	e = duckLisp_read(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
	                  parenthesisInferenceEnabled,
	                  duckLisp->maxInferenceVmObjects,
	                  &duckLisp->parenthesisInferrerTypes_array,
#endif /* USE_PARENTHESIS_INFERENCE */
	                  fileName,
	                  fileName_length,
	                  source,
	                  source_length,
	                  &ast,
	                  index,
	                  dl_true);
	if (e) goto cleanup;

	/* printf("AST: "); */
	/* e = ast_print_compoundExpression(*duckLisp, ast); putchar('\n'); */
	if (e) goto cleanup;

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

	eError = duckLisp_ast_compoundExpression_quit(duckLisp->memoryAllocation, &ast);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_scope_addObject(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    const dl_uint8_t *name,
                                    const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	/* Stick name and index in the current scope's trie. */
	e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	e = dl_trie_insert(&scope.locals_trie, name, name_length, duckLisp_localsLength_get(compileState));
	if (e) goto cleanup;

	e = scope_setTop(compileState->currentCompileState, &scope);
	if (e) goto cleanup;

 cleanup:

	return e;
}

dl_error_t duckLisp_addGlobal(duckLisp_t *duckLisp,
                              const dl_uint8_t *name,
                              const dl_size_t name_length,
                              dl_ptrdiff_t *index,
                              const dl_bool_t isComptime) {
	dl_error_t e = dl_error_ok;

	e = duckLisp_symbol_create(duckLisp, name, name_length);
	if (e) goto cleanup;
	dl_ptrdiff_t local_index = duckLisp_symbol_nameToValue(duckLisp, name, name_length);
	e = dl_trie_insert(isComptime ? &duckLisp->comptimeGlobals_trie : &duckLisp->runtimeGlobals_trie,
	                   name,
	                   name_length,
	                   local_index);
	if (e) goto cleanup;
	*index = local_index;

 cleanup: return e;
}

dl_error_t duckLisp_addInterpretedFunction(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           const duckLisp_ast_identifier_t name) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	/* Stick name and index in the current scope's trie. */
	e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	/* Record function type in function trie. */
	e = dl_trie_insert(&scope.functionLocals_trie,
	                   name.value,
	                   name.value_length,
	                   duckLisp_localsLength_get(compileState));
	if (e) goto cleanup;

	e = dl_trie_insert(&scope.functions_trie, name.value, name.value_length, duckLisp_functionType_ducklisp);
	if (e) goto cleanup;

	e = scope_setTop(compileState->currentCompileState, &scope);
	if (e) goto cleanup;

 cleanup: return e;
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
		e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
		if (e) goto cleanup;

		e = dl_trie_insert(&scope.functions_trie,
		                   name.value,
		                   name.value_length,
		                   duckLisp_functionType_macro);
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

dl_error_t duckLisp_addParserAction(duckLisp_t *duckLisp,
                                    dl_error_t (*callback)(duckLisp_t*, duckLisp_ast_compoundExpression_t*),
                                    const dl_uint8_t *name,
                                    const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	/* Record the generator stack index. */
	e = dl_trie_insert(&duckLisp->parser_actions_trie,
	                   name,
	                   name_length,
	                   duckLisp->parser_actions_array.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElement(&duckLisp->parser_actions_array, &callback);
	if (e) goto cleanup;

 cleanup:
	return e;
}

dl_error_t duckLisp_addGenerator(duckLisp_t *duckLisp,
                                 dl_error_t (*callback)(duckLisp_t*,
                                                        duckLisp_compileState_t *,
                                                        dl_array_t*,
                                                        duckLisp_ast_expression_t*),
                                 dl_uint8_t *name,
                                 const dl_size_t name_length
#ifdef USE_PARENTHESIS_INFERENCE
                                 ,
                                 dl_uint8_t *typeString,
                                 const dl_size_t typeString_length,
                                 dl_uint8_t *declarationScript,
                                 const dl_size_t declarationScript_length
#endif /* USE_PARENTHESIS_INFERENCE */
                                 ) {
	dl_error_t e = dl_error_ok;

	/* Record the generator stack index. */
	e = dl_trie_insert(&duckLisp->generators_trie,
	                   name,
	                   name_length,
	                   duckLisp->generators_stack.elements_length);
	if (e) goto cleanup;
	duckLisp->generators_length++;
	e = dl_array_pushElement(&duckLisp->generators_stack, &callback);
	if (e) goto cleanup;

#ifdef USE_PARENTHESIS_INFERENCE
	if (typeString_length > 0) {
		duckLisp_parenthesisInferrer_declarationPrototype_t prototype;
		e = DL_MALLOC(duckLisp->memoryAllocation, &prototype.name, name_length, dl_uint8_t);
		if (e) goto cleanup;
		(void) dl_memcopy_noOverlap(prototype.name, name, name_length);
		prototype.name_length = name_length;
		e = DL_MALLOC(duckLisp->memoryAllocation, &prototype.type, typeString_length, dl_uint8_t);
		if (e) goto cleanup;
		(void) dl_memcopy_noOverlap(prototype.type, typeString, typeString_length);
		prototype.type_length = typeString_length;
		/* Functions can't declare variables, except globals, which I'm ignoring for now. */
		if (declarationScript_length > 0) {
			e = DL_MALLOC(duckLisp->memoryAllocation, &prototype.script, declarationScript_length, dl_uint8_t);
			if (e) goto cleanup;
			(void) dl_memcopy_noOverlap(prototype.script, declarationScript, declarationScript_length);
		}
		else {
			prototype.script = dl_null;
		}
		prototype.script_length = declarationScript_length;
		e = dl_array_pushElement(&duckLisp->parenthesisInferrerTypes_array, &prototype);
		if (e) goto cleanup;
	}
#endif /* USE_PARENTHESIS_INFERENCE */

 cleanup: return e;
}

dl_error_t duckLisp_linkCFunction(duckLisp_t *duckLisp,
                                  dl_error_t (*callback)(duckVM_t *),
                                  dl_uint8_t *name,
                                  const dl_size_t name_length
#ifdef USE_PARENTHESIS_INFERENCE
                                  ,
                                  dl_uint8_t *typeString,
                                  const dl_size_t typeString_length
#endif /* USE_PARENTHESIS_INFERENCE */
                                  ) {
	dl_error_t e = dl_error_ok;
	(void) callback;

	/* Record function type in function trie. */
	/* Keep track of the function by using a symbol as the global's key. */
	e = duckLisp_symbol_create(duckLisp, name, name_length);
	if (e) goto cleanup;
	dl_ptrdiff_t key = duckLisp_symbol_nameToValue(duckLisp, name, name_length);
	e = dl_trie_insert(&duckLisp->callbacks_trie, name, name_length, key);
	if (e) goto cleanup;

#ifdef USE_PARENTHESIS_INFERENCE
	duckLisp_parenthesisInferrer_declarationPrototype_t prototype;
	e = DL_MALLOC(duckLisp->memoryAllocation, &prototype.name, name_length, dl_uint8_t);
	if (e) goto cleanup;
	(void) dl_memcopy_noOverlap(prototype.name, name, name_length);
	prototype.name_length = name_length;
	if (typeString_length > 0) {
		e = DL_MALLOC(duckLisp->memoryAllocation, &prototype.type, typeString_length, dl_uint8_t);
		if (e) goto cleanup;
		(void) dl_memcopy_noOverlap(prototype.type, typeString, typeString_length);
	}
	else {
		prototype.type = dl_null;
	}
	prototype.type_length = typeString_length;
	/* Functions can't declare variables, except globals, which I'm ignoring for now. */
	prototype.script = dl_null;
	prototype.script_length = 0;
	e = dl_array_pushElement(&duckLisp->parenthesisInferrerTypes_array, &prototype);
	if (e) goto cleanup;
#endif /* USE_PARENTHESIS_INFERENCE */

	/* Add to the VM's scope. */
	e = duckVM_linkCFunction(&duckLisp->vm, key, callback);
	if (e) goto cleanup;

 cleanup:
	return e;
}

dl_error_t duckLisp_serialize_errors(dl_memoryAllocation_t *memoryAllocation,
                                     dl_array_t *errorString,
                                     dl_array_t *errors,
                                     dl_array_t *sourceCode) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	(void) dl_array_init(errorString, memoryAllocation, sizeof(char), dl_array_strategy_double);

	char tempChar;
	dl_bool_t firstLoop = dl_true;
	while (errors->elements_length > 0) {
		if (!firstLoop) {
			tempChar = '\n';
			e = dl_array_pushElement(errorString, &tempChar);
			if (e) goto cleanup;
		}
		firstLoop = dl_false;

		duckLisp_error_t error;  /* Compile errors */
		e = dl_array_popElement(errors, (void *) &error);
		if (e) break;

		e = dl_array_pushElements(errorString, error.message, error.message_length);
		if (e) goto cleanup;
		tempChar = '\n';
		e = dl_array_pushElement(errorString, &tempChar);
		if (e) goto cleanup;

		if (error.start_index == -1) {
			goto whileCleanup;
		}

		if (sourceCode) {
			dl_ptrdiff_t line = 1;
			dl_ptrdiff_t start_column = 0;
			dl_ptrdiff_t end_column = 0;
			dl_ptrdiff_t column0Index = 0;

			DL_DOTIMES(i, error.start_index) {
				if (DL_ARRAY_GETADDRESS(*sourceCode, char, i) == '\n') {
					line++;
					start_column = 0;
					column0Index = i + 1;
				}
				else {
					start_column++;
				}
			}
			end_column = start_column;
			for (dl_ptrdiff_t i = column0Index + start_column; i < (dl_ptrdiff_t) sourceCode->elements_length; i++) {
				if (DL_ARRAY_GETADDRESS(*sourceCode, char, i) == '\n') {
					break;
				}
				end_column++;
			}
			e = dl_array_pushElements(errorString, error.fileName, error.fileName_length);
			if (e) goto cleanup;
			tempChar = ':';
			e = dl_array_pushElement(errorString, &tempChar);
			if (e) goto cleanup;
			e = dl_string_fromPtrdiff(errorString, line);
			if (e) goto cleanup;
			tempChar = ':';
			e = dl_array_pushElement(errorString, &tempChar);
			if (e) goto cleanup;
			e = dl_string_fromPtrdiff(errorString, start_column);
			if (e) goto cleanup;
			/* dl_string_ */
			tempChar = '\n';
			e = dl_array_pushElement(errorString, &tempChar);
			if (e) goto cleanup;
			e = dl_array_pushElements(errorString,
			                          (char *) sourceCode->elements + column0Index,
			                          end_column);
			if (e) goto cleanup;

			tempChar = '\n';
			e = dl_array_pushElement(errorString, &tempChar);
			if (e) goto cleanup;
			DL_DOTIMES(i, start_column) {
					tempChar = ' ';
					e = dl_array_pushElement(errorString, &tempChar);
					if (e) goto cleanup;
			}
			if (error.end_index == -1) {
				tempChar = '^';
				e = dl_array_pushElement(errorString, &tempChar);
				if (e) goto cleanup;
			}
			else {
				for (dl_ptrdiff_t i = error.start_index; i < error.end_index; i++) {
					tempChar = '^';
					e = dl_array_pushElement(errorString, &tempChar);
					if (e) goto cleanup;
				}
			}
			tempChar = '\n';
			e = dl_array_pushElement(errorString, &tempChar);
			if (e) goto cleanup;
		}

	whileCleanup:
		error.message_length = 0;
		eError = DL_FREE(memoryAllocation, &error.message);
		if (eError) {
			e = eError;
			break;
		}
	}
 cleanup:return e;
}

dl_error_t duckLisp_ast_type_prettyPrint(dl_array_t *string_array, duckLisp_ast_type_t type) {
	switch (type) {
	case duckLisp_ast_type_none:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_none"));
	case duckLisp_ast_type_expression:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_expression"));
	case duckLisp_ast_type_literalExpression:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_literalExpression"));
	case duckLisp_ast_type_identifier:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_identifier"));
	case duckLisp_ast_type_callback:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_callback"));
	case duckLisp_ast_type_string:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_string"));
	case duckLisp_ast_type_float:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_float"));
	case duckLisp_ast_type_int:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_int"));
	case duckLisp_ast_type_bool:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_ast_type_bool"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}

dl_error_t duckLisp_ast_bool_prettyPrint(dl_array_t *string_array, duckLisp_ast_bool_t boolean) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_ast_bool_t) {"));
	if (e) goto cleanup;

	e = dl_string_fromBool(string_array, boolean.value);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_ast_integer_prettyPrint(dl_array_t *string_array, duckLisp_ast_integer_t integer) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_ast_integer_t) {"));
	if (e) goto cleanup;

	e = dl_string_fromPtrdiff(string_array, integer.value);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_ast_float_prettyPrint(dl_array_t *string_array, duckLisp_ast_float_t floatingPoint) {
	(void) floatingPoint;
	return dl_array_pushElements(string_array, DL_STR("(duckLisp_ast_float_t) {...}"));
}

dl_error_t duckLisp_ast_string_prettyPrint(dl_array_t *string_array, duckLisp_ast_string_t string) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_ast_string_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, string.value, string.value_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_ast_identifier_prettyPrint(dl_array_t *string_array, duckLisp_ast_identifier_t identifier) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_ast_identifier_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, identifier.value, identifier.value_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_ast_expression_prettyPrint(dl_array_t *string_array, duckLisp_ast_expression_t expression) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_ast_expression_t) {"));
	if (e) goto cleanup;

	DL_DOTIMES(i, expression.compoundExpressions_length) {
		e = duckLisp_ast_compoundExpression_prettyPrint(string_array, expression.compoundExpressions[i]);
		if (e) goto cleanup;
		if ((dl_size_t) i == expression.compoundExpressions_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_ast_compoundExpression_prettyPrint(dl_array_t *string_array,
                                                       duckLisp_ast_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_ast_compoundExpression_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("type = "));
	if (e) goto cleanup;
	e = duckLisp_ast_type_prettyPrint(string_array, compoundExpression.type);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("value = "));
	if (e) goto cleanup;
	switch (compoundExpression.type) {
	case duckLisp_ast_type_none:
		e = dl_array_pushElements(string_array, DL_STR("NONE"));
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_expression:
		/* Fall through. */
	case duckLisp_ast_type_literalExpression:
		e = duckLisp_ast_expression_prettyPrint(string_array, compoundExpression.value.expression);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_identifier:
		/* Fall through. */
	case duckLisp_ast_type_callback:
		e = duckLisp_ast_identifier_prettyPrint(string_array, compoundExpression.value.identifier);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_string:
		e = duckLisp_ast_string_prettyPrint(string_array, compoundExpression.value.string);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_float:
		e = duckLisp_ast_float_prettyPrint(string_array, compoundExpression.value.floatingPoint);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_int:
		e = duckLisp_ast_integer_prettyPrint(string_array, compoundExpression.value.integer);
		if (e) goto cleanup;
		break;
	case duckLisp_ast_type_bool:
		e = duckLisp_ast_bool_prettyPrint(string_array, compoundExpression.value.boolean);
		if (e) goto cleanup;
		break;
	default:
		e = dl_array_pushElements(string_array, DL_STR("INVALID"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_error_prettyPrint(dl_array_t *string_array, duckLisp_error_t error) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_error_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("message["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, error.message_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = \""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, error.message, error.message_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("start_index = "));
	if (e) goto cleanup;
	e = dl_string_fromPtrdiff(string_array, error.start_index);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("end_index = "));
	if (e) goto cleanup;
	e = dl_string_fromPtrdiff(string_array, error.end_index);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("fileName["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, error.fileName_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = \""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, error.fileName, error.fileName_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_functionType_prettyPrint(dl_array_t *string_array, duckLisp_functionType_t functionType) {
	switch (functionType) {
	case duckLisp_functionType_none:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_functionType_none"));
	case duckLisp_functionType_c:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_functionType_c"));
	case duckLisp_functionType_ducklisp:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_functionType_ducklisp"));
	case duckLisp_functionType_generator:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_functionType_generator"));
	case duckLisp_functionType_macro:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_functionType_macro"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}

dl_error_t duckLisp_scope_prettyPrint(dl_array_t *string_array, duckLisp_scope_t scope) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_scope_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("locals_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, scope.locals_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("functionLocals_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, scope.functionLocals_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("functions_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, scope.functions_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("functions_length = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, scope.functions_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("macros_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, scope.macros_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("macros_length = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, scope.macros_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("labels_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, scope.labels_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("function_scope = "));
	if (e) goto cleanup;
	e = dl_string_fromBool(string_array, scope.function_scope);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("scope_uvs = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, scope.scope_uvs_length) {
		e = dl_string_fromPtrdiff(string_array, scope.scope_uvs[i]);
		if (e) goto cleanup;
		if ((dl_size_t) i != scope.scope_uvs_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("function_uvs = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, scope.function_uvs_length) {
		e = dl_string_fromPtrdiff(string_array, scope.function_uvs[i]);
		if (e) goto cleanup;
		if ((dl_size_t) i != scope.function_uvs_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_subCompileState_prettyPrint(dl_array_t *string_array, duckLisp_subCompileState_t subCompileState) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_subCompileState_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("scope_stack = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, subCompileState.scope_stack.elements_length) {
		e = duckLisp_scope_prettyPrint(string_array,
		                               DL_ARRAY_GETADDRESS(subCompileState.scope_stack, duckLisp_scope_t, i));
		if (e) goto cleanup;
		if ((dl_size_t) i != subCompileState.scope_stack.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("locals_length = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, subCompileState.locals_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("label_number = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, subCompileState.label_number);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("assembly["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, subCompileState.assembly.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {...}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_compileState_prettyPrint(dl_array_t *string_array, duckLisp_compileState_t compileState) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_compileState_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("currentCompileState = "));
	if (e) goto cleanup;
	if (compileState.currentCompileState == &compileState.comptimeCompileState) {
		e = dl_array_pushElements(string_array, DL_STR("&comptimeCompileState"));
	}
	else if (compileState.currentCompileState == &compileState.runtimeCompileState) {
		e = dl_array_pushElements(string_array, DL_STR("&runtimeCompileState"));
	}
	else if (compileState.currentCompileState == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("INVALID"));
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("runtimeCompileState = "));
	if (e) goto cleanup;
	e = duckLisp_subCompileState_prettyPrint(string_array, compileState.runtimeCompileState);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("comptimeCompileState = "));
	if (e) goto cleanup;
	e = duckLisp_subCompileState_prettyPrint(string_array, compileState.comptimeCompileState);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

#ifdef USE_DATALOGGING
dl_error_t duckLisp_datalog_prettyPrint(dl_array_t *string_array, duckLisp_datalog_t datalog) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_datalog_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("total_instructions_generated = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, datalog.total_instructions_generated);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("total_bytes_generated = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, datalog.total_bytes_generated);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("jumpsize_bytes_removed = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, datalog.jumpsize_bytes_removed);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("pushpop_instructions_removed = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, datalog.pushpop_instructions_removed);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif /* USE_DATALOGGING */

dl_error_t duckLisp_prettyPrint(dl_array_t *string_array, duckLisp_t duckLisp) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("errors["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.errors.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckLisp.errors.elements_length) {
		e = duckLisp_error_prettyPrint(string_array, DL_ARRAY_GETADDRESS(duckLisp.errors, duckLisp_error_t, i));
		if (e) goto cleanup;
		if ((dl_size_t) i != duckLisp.errors.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("generators_stack["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.generators_stack.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {...}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("generators_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, duckLisp.generators_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("generators_length = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.generators_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("callbacks_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, duckLisp.callbacks_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

#ifdef USE_PARENTHESIS_INFERENCE
	e = dl_array_pushElements(string_array, DL_STR("parenthesisInferrerTypes_array["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.parenthesisInferrerTypes_array.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckLisp.parenthesisInferrerTypes_array.elements_length) {
		e = duckLisp_parenthesisInferrer_declarationPrototype_prettyPrint(string_array,
		                                                                  DL_ARRAY_GETADDRESS((duckLisp
		                                                                                       .parenthesisInferrerTypes_array),
		                                                                                      duckLisp_parenthesisInferrer_declarationPrototype_t,
		                                                                                      i));
		if (e) goto cleanup;
		if ((dl_size_t) i != duckLisp.parenthesisInferrerTypes_array.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("maxInferenceVmObjects = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.maxInferenceVmObjects);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("inferrerLog["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.inferrerLog.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = \""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, duckLisp.inferrerLog.elements, duckLisp.inferrerLog.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;
#endif /* USE_PARENTHESIS_INFERENCE */

	e = dl_array_pushElements(string_array, DL_STR("gensym_number = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.gensym_number);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("comptimeGlobals_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, duckLisp.comptimeGlobals_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("runtimeGlobals_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, duckLisp.runtimeGlobals_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("symbols_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, duckLisp.symbols_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("symbols_array["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.symbols_array.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, duckLisp.symbols_array.elements_length) {
		e = duckLisp_ast_identifier_prettyPrint(string_array,
		                                        DL_ARRAY_GETADDRESS(duckLisp.symbols_array,
		                                                            duckLisp_ast_identifier_t,
		                                                            i));
		if (e) goto cleanup;
		if ((dl_size_t) i != duckLisp.symbols_array.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("parser_actions_trie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, duckLisp.parser_actions_trie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("parser_actions_array["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.parser_actions_array.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {...}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("parser_recursion_depth = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.parser_recursion_depth);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("parser_max_recursion_depth = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.parser_max_recursion_depth);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("generators_recursion_depth = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.generators_recursion_depth);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("generators_max_recursion_depth = "));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, duckLisp.generators_max_recursion_depth);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("vm = "));
	if (e) goto cleanup;
	e = duckVM_prettyPrint(string_array, duckLisp.vm);
	if (e) goto cleanup;

#ifdef USE_DATALOGGING
	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("datalog = "));
	if (e) goto cleanup;
	e = duckLisp_datalog_prettyPrint(string_array, duckLisp.datalog);
	if (e) goto cleanup;
#endif /* USE_DATALOGGING */

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_instructionArgClass_type_prettyPrint(dl_array_t *string_array,
                                                         duckLisp_instructionArgClass_type_t type) {
	switch (type) {
	case duckLisp_instructionArgClass_type_none:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionArgClass_type_none"));
	case duckLisp_instructionArgClass_type_integer:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionArgClass_type_integer"));
	case duckLisp_instructionArgClass_type_doubleFloat:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionArgClass_type_doubleFloat"));
	case duckLisp_instructionArgClass_type_index:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionArgClass_type_index"));
	case duckLisp_instructionArgClass_type_string:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionArgClass_type_string"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}

dl_error_t duckLisp_instructionArgClass_prettyPrint(dl_array_t *string_array,
                                                    duckLisp_instructionArgClass_t instructionArgClass) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_instructionArgClass_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("type = "));
	if (e) goto cleanup;
	e = duckLisp_instructionArgClass_type_prettyPrint(string_array, instructionArgClass.type);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("value = "));
	if (e) goto cleanup;
	switch (instructionArgClass.type) {
	case duckLisp_instructionArgClass_type_integer:
		e = dl_string_fromPtrdiff(string_array, instructionArgClass.value.integer);
		if (e) goto cleanup;
		break;
	case duckLisp_instructionArgClass_type_index:
		e = dl_string_fromPtrdiff(string_array, instructionArgClass.value.index);
		if (e) goto cleanup;
		break;
	case duckLisp_instructionArgClass_type_doubleFloat:
		e = dl_array_pushElements(string_array, DL_STR("..."));
		if (e) goto cleanup;
		break;
	case duckLisp_instructionArgClass_type_string:
		e = dl_array_pushElements(string_array,
		                          instructionArgClass.value.string.value,
		                          instructionArgClass.value.string.value_length);
		if (e) goto cleanup;
		break;
	default:
		e = dl_array_pushElements(string_array, DL_STR("INVALID"));
		if (e) goto cleanup;
	}

 cleanup: return e;
}

dl_error_t duckLisp_instructionClass_prettyPrint(dl_array_t *string_array,
                                                 duckLisp_instructionClass_t instructionClass) {
	switch (instructionClass) {
	case duckLisp_instructionClass_nop:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_nop"));
	case duckLisp_instructionClass_pushString:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushString"));
	case duckLisp_instructionClass_pushBoolean:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushBoolean"));
	case duckLisp_instructionClass_pushInteger:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushInteger"));
	case duckLisp_instructionClass_pushDoubleFloat:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushDoubleFloat"));
	case duckLisp_instructionClass_pushIndex:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushIndex"));
	case duckLisp_instructionClass_pushSymbol:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushSymbol"));
	case duckLisp_instructionClass_pushUpvalue:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushUpvalue"));
	case duckLisp_instructionClass_pushClosure:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushClosure"));
	case duckLisp_instructionClass_pushVaClosure:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushVaClosure"));
	case duckLisp_instructionClass_pushGlobal:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pushGlobal"));
	case duckLisp_instructionClass_setUpvalue:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_setUpvalue"));
	case duckLisp_instructionClass_setStatic:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_setStatic"));
	case duckLisp_instructionClass_releaseUpvalues:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_releaseUpvalues"));
	case duckLisp_instructionClass_funcall:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_funcall"));
	case duckLisp_instructionClass_apply:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_apply"));
	case duckLisp_instructionClass_call:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_call"));
	case duckLisp_instructionClass_ccall:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_ccall"));
	case duckLisp_instructionClass_acall:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_acall"));
	case duckLisp_instructionClass_jump:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_jump"));
	case duckLisp_instructionClass_brz:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_brz"));
	case duckLisp_instructionClass_brnz:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_brnz"));
	case duckLisp_instructionClass_move:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_move"));
	case duckLisp_instructionClass_not:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_not"));
	case duckLisp_instructionClass_mul:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_mul"));
	case duckLisp_instructionClass_div:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_div"));
	case duckLisp_instructionClass_add:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_add"));
	case duckLisp_instructionClass_sub:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_sub"));
	case duckLisp_instructionClass_equal:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_equal"));
	case duckLisp_instructionClass_less:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_less"));
	case duckLisp_instructionClass_greater:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_greater"));
	case duckLisp_instructionClass_cons:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_cons"));
	case duckLisp_instructionClass_vector:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_vector"));
	case duckLisp_instructionClass_makeVector:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_makeVector"));
	case duckLisp_instructionClass_getVecElt:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_getVecElt"));
	case duckLisp_instructionClass_setVecElt:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_setVecElt"));
	case duckLisp_instructionClass_car:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_car"));
	case duckLisp_instructionClass_cdr:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_cdr"));
	case duckLisp_instructionClass_setCar:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_setCar"));
	case duckLisp_instructionClass_setCdr:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_setCdr"));
	case duckLisp_instructionClass_nullp:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_nullp"));
	case duckLisp_instructionClass_typeof:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_typeof"));
	case duckLisp_instructionClass_makeType:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_makeType"));
	case duckLisp_instructionClass_makeInstance:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_makeInstance"));
	case duckLisp_instructionClass_compositeValue:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_compositeValue"));
	case duckLisp_instructionClass_compositeFunction:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_compositeFunction"));
	case duckLisp_instructionClass_setCompositeValue:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_setCompositeValue"));
	case duckLisp_instructionClass_setCompositeFunction:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_setCompositeFunction"));
	case duckLisp_instructionClass_makeString:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_makeString"));
	case duckLisp_instructionClass_concatenate:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_concatenate"));
	case duckLisp_instructionClass_substring:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_substring"));
	case duckLisp_instructionClass_length:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_length"));
	case duckLisp_instructionClass_symbolString:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_symbolString"));
	case duckLisp_instructionClass_symbolId:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_symbolId"));
	case duckLisp_instructionClass_pop:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pop"));
	case duckLisp_instructionClass_return:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_return"));
	case duckLisp_instructionClass_yield:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_yield"));
	case duckLisp_instructionClass_halt:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_halt"));
	case duckLisp_instructionClass_nil:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_nil"));
	case duckLisp_instructionClass_pseudo_label:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_pseudo_label"));
	case duckLisp_instructionClass_internalNop:
		return dl_array_pushElements(string_array, DL_STR("duckLisp_instructionClass_internalNop"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}

dl_error_t duckLisp_instructionObject_prettyPrint(dl_array_t *string_array,
                                                  duckLisp_instructionObject_t instructionObject) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_instructionObject_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("instructionClass = "));
	if (e) goto cleanup;
	e = duckLisp_instructionClass_prettyPrint(string_array, instructionObject.instructionClass);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("args["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, instructionObject.args.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, instructionObject.args.elements_length) {
		e = duckLisp_instructionArgClass_prettyPrint(string_array,
		                                             DL_ARRAY_GETADDRESS(instructionObject.args,
		                                                                 duckLisp_instructionArgClass_t,
		                                                                 i));
		if (e) goto cleanup;
		if ((dl_size_t) i != instructionObject.args.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
