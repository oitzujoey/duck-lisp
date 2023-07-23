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
#include <stdio.h>

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

dl_error_t scope_getFunctionFromName(duckLisp_t *duckLisp,
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

dl_error_t duckLisp_scope_getLabelFromName(duckLisp_subCompileState_t *subCompileState,
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

void duckLisp_localsLength_increment(duckLisp_compileState_t *compileState) {
	compileState->currentCompileState->locals_length++;
}

void duckLisp_localsLength_decrement(duckLisp_compileState_t *compileState) {
	--compileState->currentCompileState->locals_length;
}

dl_size_t duckLisp_localsLength_get(duckLisp_compileState_t *compileState) {
	return compileState->currentCompileState->locals_length;
}

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
	                         ((duckLisp_localsLength_get(compileState) == 0) ? 0 : duckLisp_localsLength_get(compileState) - 1));
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
	e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	e = dl_trie_insert(&scope.locals_trie, name, name_length, duckLisp_localsLength_get(compileState));
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
	e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
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
		e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
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
