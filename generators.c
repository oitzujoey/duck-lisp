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

#include "duckLisp.h"
#include "generators.h"
#include "DuckLib/string.h"
#include "parser.h"
#include "emitters.h"

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


/* Generator for the `__declare` keyword that is recognized by the parenthesis inferrer. Expands to nil, no matter what
   arguments are passed to it. */
dl_error_t duckLisp_generator_declare(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression) {
	(void) expression;
	return duckLisp_emit_nil(duckLisp, compileState, assembly);
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
	cons_index = duckLisp_localsLength_get(compileState) - 1;

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
		cons_index = duckLisp_localsLength_get(compileState) - 1;
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
			last_temp_index = duckLisp_localsLength_get(compileState) - 1;
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
				                       duckLisp_localsLength_get(compileState) - 1,
				                       last_temp_index);
				if (e) goto cleanup;
				last_temp_index = duckLisp_localsLength_get(compileState) - 1;
			}
			*stack_index = duckLisp_localsLength_get(compileState) - 1;
		}
		else {
			e = duckLisp_emit_nil(duckLisp, compileState, assembly);
			if (e) goto cleanup;
			*stack_index = duckLisp_localsLength_get(compileState) - 1;
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
	dl_uint8_t *functionName = expression->compoundExpressions[0].value.identifier.value;
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
		                  &temp_index,
		                  tree->value.identifier.value,
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
			last_temp_index = duckLisp_localsLength_get(compileState) - 1;
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
				                       duckLisp_localsLength_get(compileState) - 1,
				                       last_temp_index);
				if (e) goto cleanup;
				last_temp_index = duckLisp_localsLength_get(compileState) - 1;
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
	(void) dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_size_t startStack_length;
	/* Only one of these can return true. */
	dl_bool_t foundVar = dl_false;
	dl_bool_t foundDefun = dl_false;
	dl_bool_t foundNoscope = dl_false;
	dl_ptrdiff_t pops = 0;

	/* Compile */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		duckLisp_ast_compoundExpression_t currentExpression;
		startStack_length = duckLisp_localsLength_get(compileState);
		/* Always compile the form. This works with `__var`, `__defun` and `__noscope` because global dummy generators
		   are defined that do nothing. The reason for always compiling is so that those keywords can be returned from
		   macros. So this statement can be thought of as "compile form" or as "macroexpand all". */
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        compileState,
		                                        assembly,
		                                        DL_STR("noscope"),
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto cleanup;
		foundDefun = dl_false;
		foundVar = dl_false;
		foundNoscope = dl_false;
		currentExpression = expression->compoundExpressions[i];
		if ((currentExpression.type == duckLisp_ast_type_expression)
		    && (currentExpression.value.expression.compoundExpressions_length > 0)
		    && (currentExpression.value.expression.compoundExpressions[0].type == duckLisp_ast_type_identifier)) {
			/* Only one of these can return true. */
			(void) dl_string_compare(&foundVar,
			                         currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                         (currentExpression.value.expression.compoundExpressions[0]
			                          .value.identifier.value_length),
			                         DL_STR("__var"));
			if (!foundVar) {
				(void) dl_string_compare(&foundVar,
				                         (currentExpression.value.expression.compoundExpressions[0]
				                          .value.identifier.value),
				                         (currentExpression.value.expression.compoundExpressions[0]
				                          .value.identifier.value_length),
				                         DL_STR("var"));
			}
			(void) dl_string_compare(&foundDefun,
			                         currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                         (currentExpression.value.expression.compoundExpressions[0]
			                          .value.identifier.value_length),
			                         DL_STR("__defun"));
			if (!foundDefun) {
				(void) dl_string_compare(&foundDefun,
				                         (currentExpression.value.expression.compoundExpressions[0]
				                          .value.identifier.value),
				                         (currentExpression.value.expression.compoundExpressions[0]
				                          .value.identifier.value_length),
				                         DL_STR("defun"));
			}
			(void) dl_string_compare(&foundNoscope,
			                         currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                         (currentExpression.value.expression.compoundExpressions[0]
			                          .value.identifier.value_length),
			                         DL_STR("__noscope"));
			if (!foundNoscope) {
				(void) dl_string_compare(&foundNoscope,
				                         (currentExpression.value.expression.compoundExpressions[0]
				                          .value.identifier.value),
				                         (currentExpression.value.expression.compoundExpressions[0]
				                          .value.identifier.value_length),
				                         DL_STR("noscope"));
			}
		}
		/* Now, since `__var`, `__defun`, and `__noscope` are dummy generators, they have to be handled here. */
		if (foundNoscope) e = duckLisp_generator_noscope2(duckLisp,
		                                                  compileState,
		                                                  assembly,
		                                                  &expression->compoundExpressions[i].value.expression);
		if (foundVar) e = duckLisp_generator_createVar(duckLisp,
		                                               compileState,
		                                               assembly,
		                                               &expression->compoundExpressions[i].value.expression);
		if (foundDefun) e = duckLisp_generator_defun(duckLisp,
		                                             compileState,
		                                             assembly,
		                                             &expression->compoundExpressions[i].value.expression);
		if (e) goto cleanup;
		if (!(foundNoscope
		      || foundVar
		      || foundDefun)) {
			pops = (duckLisp_localsLength_get(compileState)
			        - startStack_length
			        - ((dl_size_t) i == expression->compoundExpressions_length - 1));
			if (pops > 0) {
				if ((dl_size_t) i == expression->compoundExpressions_length - 1) {
					e = duckLisp_emit_move(duckLisp,
					                       compileState,
					                       assembly,
					                       duckLisp_localsLength_get(compileState) - 1 - pops,
					                       duckLisp_localsLength_get(compileState) - 1);
					if (e) goto cleanup;
				}
				e = duckLisp_emit_pop(duckLisp, compileState, assembly, pops);
				if (e) goto cleanup;
			}
			else if (pops < 0) {
				DL_DOTIMES(l, (dl_size_t) (-pops)) {
					e = duckLisp_emit_pushIndex(duckLisp, compileState, assembly, duckLisp_localsLength_get(compileState) - 1);
					if (e) goto cleanup;
				}
			}
		}
		else if ((foundNoscope || foundVar || foundDefun)
		         && !((dl_size_t) i == expression->compoundExpressions_length - 1)) {
			e = duckLisp_emit_pop(duckLisp, compileState, assembly, 1);
			if (e) goto cleanup;
		}
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
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_ast_expression_t subExpression;
	e = DL_MALLOC(duckLisp->memoryAllocation,
	              &subExpression.compoundExpressions,
	              expression->compoundExpressions_length - 1,
	              duckLisp_ast_compoundExpression_t);
	if (e) return e;  /* sic. */
	(void) dl_memcopy_noOverlap(subExpression.compoundExpressions,
	                            &expression->compoundExpressions[1],
	                            ((expression->compoundExpressions_length - 1)
	                             * sizeof(duckLisp_ast_compoundExpression_t)));
	subExpression.compoundExpressions_length = expression->compoundExpressions_length - 1;

	e = duckLisp_generator_noscope(duckLisp, compileState, assembly, &subExpression);
	if (e) goto cleanup;

	(void) dl_memcopy_noOverlap(&expression->compoundExpressions[1],
	                            subExpression.compoundExpressions,
	                            ((expression->compoundExpressions_length - 1)
	                             * sizeof(duckLisp_ast_compoundExpression_t)));

 cleanup:
	eError = DL_FREE(duckLisp->memoryAllocation, &subExpression.compoundExpressions);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_noscope2_dummy(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_ast_expression_t *expression) {
	(void) duckLisp;
	(void) compileState;
	(void) assembly;
	(void) expression;
	/* Handle this in `duckLisp_generator_noscope`, which calls the real generator. */
	return dl_error_ok;
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
	dl_error_t eError = dl_error_ok;

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
				e = dl_error_invalidValue;
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Cannot return cons with a non-nil CDR."));
				if (eError) e = eError;
				goto cleanup;
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
	duckLisp_ast_compoundExpression_t returnCompoundExpression;
	dl_uint8_t haltInstruction = duckLisp_instruction_halt;

	duckLisp_subCompileState_t *lastSubCompileState = compileState->currentCompileState;
	compileState->currentCompileState = &compileState->comptimeCompileState;

	if (lastSubCompileState == &compileState->comptimeCompileState) {
		e = duckLisp_generator_noscope2(duckLisp, compileState, assembly, expression);
		if (e) goto cleanup;
	}
	else {
		e = duckLisp_generator_noscope2(duckLisp, compileState, &compAssembly, expression);
		if (e) goto cleanup;

		e = dl_array_pushElements(&compileState->currentCompileState->assembly,
		                          compAssembly.elements,
		                          compAssembly.elements_length);
		if (e) goto cleanup;

		e = duckLisp_assemble(duckLisp, compileState, &bytecode, &compileState->currentCompileState->assembly);
		if (e) goto cleanup;

		e = dl_array_pushElement(&bytecode, &haltInstruction);
		if (e) goto cleanup;

		e = duckLisp_assembly_quit(duckLisp, &compileState->currentCompileState->assembly);
		if (e) goto cleanup;
		(void) duckLisp_assembly_init(duckLisp, &compileState->currentCompileState->assembly);

		/* puts(duckLisp_disassemble(duckLisp->memoryAllocation, bytecode.elements, bytecode.elements_length)); */

		e = duckVM_execute(&duckLisp->vm, &returnValue, bytecode.elements, bytecode.elements_length);
		eError = dl_array_pushElements(&duckLisp->errors,
		                               duckLisp->vm.errors.elements,
		                               duckLisp->vm.errors.elements_length);
		if (eError) e = eError;
		eError = dl_array_popElements(&duckLisp->vm.errors, dl_null, duckLisp->vm.errors.elements_length);
		if (eError) e = eError;
		if (e) goto cleanup;

		e = duckLisp_objectToAST(duckLisp, &returnCompoundExpression, &returnValue, dl_false);
		if (e) goto cleanup;

		/**/ duckLisp_localsLength_decrement(compileState);

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

		e = duckLisp_ast_compoundExpression_quit(duckLisp->memoryAllocation, &returnCompoundExpression);
		if (e) goto cleanup;
	}

 cleanup:
	compileState->currentCompileState = lastSubCompileState;

	eError = dl_array_quit(&compAssembly);
	if (eError) e = eError;

	eError = dl_array_quit(&bytecode);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

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

	if (compileState->currentCompileState == &compileState->comptimeCompileState) {
		e = dl_error_invalidValue;
		(eError
		 = duckLisp_error_pushRuntime(duckLisp,
		                              DL_STR("__defmacro: \"__defmacro\" may only be used in the runtime environment.")));
		if (eError) e = eError;
		goto cleanup;
	}

	/* Compile */

	compileState->currentCompileState = &compileState->comptimeCompileState;

	e = duckLisp_generator_defun(duckLisp, compileState, &compileState->comptimeCompileState.assembly, expression);
	if (e) goto cleanup;

	e = duckLisp_assemble(duckLisp, compileState, &macroBytecode, &compileState->comptimeCompileState.assembly);
	if (e) goto cleanup;

	e = dl_array_pushElement(&macroBytecode, &yieldInstruction);
	if (e) goto cleanup;

	e = duckLisp_assembly_quit(duckLisp, &compileState->comptimeCompileState.assembly);
	if (e) goto cleanup;
	(void) duckLisp_assembly_init(duckLisp, &compileState->comptimeCompileState.assembly);

	/* puts(duckLisp_disassemble(duckLisp->memoryAllocation, */
	/*                           macroBytecode.elements, */
	/*                           macroBytecode.elements_length)); */

	e = duckVM_execute(&duckLisp->vm, dl_null, macroBytecode.elements, macroBytecode.elements_length);
	eError = dl_array_pushElements(&duckLisp->errors,
	                               duckLisp->vm.errors.elements,
	                               duckLisp->vm.errors.elements_length);
	if (eError) e = eError;
	eError = dl_array_popElements(&duckLisp->vm.errors, dl_null, duckLisp->vm.errors.elements_length);
	if (eError) e = eError;
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
                                         duckLisp_ast_expression_t *expression) {
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

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3, dl_true);
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

		{
			duckLisp_ast_identifier_t identifier = {DL_STR("self")};
			/* Since this is effectively a single pass compiler, I don't see a good way to determine purity before
			   compilation of the body. */
			e = duckLisp_addInterpretedFunction(duckLisp, compileState, identifier);
			if (e) goto cleanup;
		}
		duckLisp_localsLength_increment(compileState);

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
		if (e) goto cleanup_gensym;

		e = duckLisp_register_label(duckLisp,
		                            compileState->currentCompileState,
		                            selfGensym.value,
		                            selfGensym.value_length);
		if (e) goto cleanup_gensym;

		/* (label function_name) */
		e = duckLisp_emit_label(duckLisp, compileState, &bodyAssembly, selfGensym.value, selfGensym.value_length);
		if (e) goto cleanup_gensym;

		/* `label_index` should never equal -1 after this function exits. */
		e = duckLisp_scope_getLabelFromName(compileState->currentCompileState,
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

		startStack_length = duckLisp_localsLength_get(compileState);

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
				duckLisp_localsLength_increment(compileState);
			}
		}

		/* Body */

		{
			duckLisp_ast_expression_t progn;
			e = DL_MALLOC(duckLisp->memoryAllocation,
			              &progn.compoundExpressions,
			              expression->compoundExpressions_length - 2,
			              duckLisp_ast_compoundExpression_t);
			if (e) goto cleanup_gensym; /* sic. */
			(void) dl_memcopy_noOverlap(progn.compoundExpressions,
			                            &expression->compoundExpressions[2],
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;

			e = duckLisp_generator_expression(duckLisp, compileState, &bodyAssembly, &progn);
			if (e) goto cleanupProgn;

			(void) dl_memcopy_noOverlap(&expression->compoundExpressions[2],
			                            progn.compoundExpressions,
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));

		cleanupProgn:
			eError = DL_FREE(duckLisp->memoryAllocation, &progn.compoundExpressions);
			if (eError) e = eError;
			if (e) goto cleanup_gensym;
		}

		/* Footer */

		{
			duckLisp_scope_t scope;
			e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
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
		                             duckLisp_localsLength_get(compileState) - startStack_length - 1,
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
			e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
			if (e) goto cleanup_gensym;
			duckLisp_localsLength_decrement(compileState);
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
		}

		{
			/* Release the `self` upvalue. */
			duckLisp_scope_t scope = DL_ARRAY_GETADDRESS(compileState->currentCompileState->scope_stack,
			                                             duckLisp_scope_t,
			                                             (compileState->currentCompileState->scope_stack.elements_length
			                                              - 2));

			if (scope.scope_uvs_length) {
				/* Manual intervention with the stack length is OK here since the only upvalue here can be `self`. */
				e = duckLisp_emit_pushIndex(duckLisp,
				                            compileState,
				                            assembly,
				                            duckLisp_localsLength_get(compileState) - 1);
				if (e) goto cleanup_gensym;
				e = duckLisp_emit_releaseUpvalues(duckLisp,
				                                  compileState,
				                                  assembly,
				                                  scope.scope_uvs,
				                                  scope.scope_uvs_length);
				if (e) goto cleanup_gensym;
				e = duckLisp_emit_move(duckLisp,
				                       compileState,
				                       assembly,
				                       duckLisp_localsLength_get(compileState) - 2,
				                       duckLisp_localsLength_get(compileState) - 1);
				if (e) goto cleanup_gensym;
				e = duckLisp_emit_pop(duckLisp, compileState, assembly, 1);
				if (e) goto cleanup_gensym;
			}
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
	return duckLisp_generator_lambda_raw(duckLisp, compileState, assembly, expression);
}

dl_error_t duckLisp_generator_createVar_raw(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            dl_array_t *assembly,
                                            duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_size_t startStack_length = duckLisp_localsLength_get(compileState);

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
	dl_size_t startLocals_length = duckLisp_localsLength_get(compileState);
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
	dl_size_t endLocals_length = duckLisp_localsLength_get(compileState);
	compileState->currentCompileState->locals_length = startLocals_length;
	e = duckLisp_scope_addObject(duckLisp,
	                             compileState,
	                             expression->compoundExpressions[1].value.identifier.value,
	                             expression->compoundExpressions[1].value.identifier.value_length);
	if (e) goto cleanup;
	compileState->currentCompileState->locals_length = endLocals_length;

	e = duckLisp_emit_move(duckLisp, compileState, assembly, startStack_length, duckLisp_localsLength_get(compileState) - 1);
	if (e) goto cleanup;
	if (duckLisp_localsLength_get(compileState) > startStack_length + 1) {
		e = duckLisp_emit_pop(duckLisp, compileState, assembly, duckLisp_localsLength_get(compileState) - startStack_length - 1);
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
	return duckLisp_generator_createVar_raw(duckLisp, compileState, assembly, expression);
}

dl_error_t duckLisp_generator_createVar_dummy(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              duckLisp_ast_expression_t *expression) {
	(void) duckLisp;
	(void) compileState;
	(void) assembly;
	(void) expression;
	/* Handle this in `duckLisp_generator_noscope`, which calls the real generator. */
	return dl_error_ok;
}

dl_error_t duckLisp_generator_global(duckLisp_t *duckLisp,
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
	e = duckLisp_addGlobal(duckLisp,
	                       expression->compoundExpressions[1].value.identifier.value,
	                       expression->compoundExpressions[1].value.identifier.value_length,
	                       &static_index,
	                       &compileState->comptimeCompileState == compileState->currentCompileState);
	if (e) goto cleanup;

	e = duckLisp_emit_setStatic(duckLisp, compileState, assembly, static_index, duckLisp_localsLength_get(compileState) - 1);

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
	lambda.compoundExpressions[0].value.identifier.value = (dl_uint8_t *) "\0defun:lambda";
	lambda.compoundExpressions[0].value.identifier.value_length = sizeof("\0defun:lambda") - 1;
	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		lambda.compoundExpressions[i - 1] = expression->compoundExpressions[i];
	}
	duckLisp_ast_expression_t var;
	e = DL_MALLOC(duckLisp->memoryAllocation, (void **) &var.compoundExpressions, 3, duckLisp_ast_compoundExpression_t);
	if (e) goto cleanup;
	var.compoundExpressions_length = 3;
	var.compoundExpressions[0].type = duckLisp_ast_type_identifier;
	var.compoundExpressions[0].value.identifier.value = (dl_uint8_t *) "\0defun:var";
	var.compoundExpressions[0].value.identifier.value_length = sizeof("\0defun:var") - 1;
	var.compoundExpressions[1] = expression->compoundExpressions[1];
	var.compoundExpressions[2].type = duckLisp_ast_type_expression;
	var.compoundExpressions[2].value.expression = lambda;
	{
		e = duckLisp_generator_createVar_raw(duckLisp, compileState, assembly, &var);
		if (e) goto cleanup;

		for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
			expression->compoundExpressions[i] = lambda.compoundExpressions[i - 1];
		}
		expression->compoundExpressions[1] = var.compoundExpressions[1];

		e = DL_FREE(duckLisp->memoryAllocation, (void **) &var.compoundExpressions);
		if (e) goto cleanup;
		e = DL_FREE(duckLisp->memoryAllocation, (void **) &lambda.compoundExpressions);
		if (e) goto cleanup;

		/* HACK */
		/* `duckLisp_addInterpretedFunction` needs to know the position of the closure. */
		(void) duckLisp_localsLength_decrement(compileState);
		(void) duckLisp_localsLength_decrement(compileState);
		e = duckLisp_addInterpretedFunction(duckLisp,
		                                    compileState,
		                                    expression->compoundExpressions[1].value.identifier);
		if (e) goto cleanup;
		(void) duckLisp_localsLength_increment(compileState);
		(void) duckLisp_localsLength_increment(compileState);
	}

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_generator_defun_dummy(duckLisp_t *duckLisp,
                                          duckLisp_compileState_t *compileState,
                                          dl_array_t *assembly,
                                          duckLisp_ast_expression_t *expression) {
	(void) duckLisp;
	(void) compileState;
	(void) assembly;
	(void) expression;
	/* Handle this in `duckLisp_generator_noscope`, which calls the real generator. */
	return dl_error_ok;
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
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
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
			e = duckLisp_pushScope(duckLisp, compileState, dl_null, dl_false);
			if (e) goto free_gensym_start;

			/* Arguments */

			startStack_length = duckLisp_localsLength_get(compileState);

			{
				duckLisp_ast_expression_t progn;
				e = DL_MALLOC(duckLisp->memoryAllocation,
				              &progn.compoundExpressions,
				              expression->compoundExpressions_length - 2,
				              duckLisp_ast_compoundExpression_t);
				if (e) goto free_gensym_start;
				(void) dl_memcopy_noOverlap(progn.compoundExpressions,
				                            &expression->compoundExpressions[2],
				                            ((expression->compoundExpressions_length - 2)
				                             * sizeof(duckLisp_ast_compoundExpression_t)));
				progn.compoundExpressions_length = expression->compoundExpressions_length - 2;

				e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
				if (e) goto cleanupProgn1;

				(void) dl_memcopy_noOverlap(&expression->compoundExpressions[2],
				                            progn.compoundExpressions,
				                            ((expression->compoundExpressions_length - 2)
				                             * sizeof(duckLisp_ast_compoundExpression_t)));

			cleanupProgn1:
				e = DL_FREE(duckLisp->memoryAllocation, &progn.compoundExpressions);
				if (e) goto free_gensym_start;
			}

			if (duckLisp_localsLength_get(compileState) > startStack_length) {
				e = duckLisp_emit_pop(duckLisp,
				                      compileState,
				                      assembly,
				                      duckLisp_localsLength_get(compileState) - startStack_length);
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
			e = duckLisp_pushScope(duckLisp, compileState, dl_null, dl_false);
			if (e) goto free_gensym_end;

			// Arguments

			startStack_length = duckLisp_localsLength_get(compileState);

			{
				duckLisp_ast_expression_t progn;
				e = DL_MALLOC(duckLisp->memoryAllocation,
				              &progn.compoundExpressions,
				              expression->compoundExpressions_length - 2,
				              duckLisp_ast_compoundExpression_t);
				if (e) goto free_gensym_end;
				(void) dl_memcopy_noOverlap(progn.compoundExpressions,
				                            &expression->compoundExpressions[2],
				                            ((expression->compoundExpressions_length - 2)
				                             * sizeof(duckLisp_ast_compoundExpression_t)));
				progn.compoundExpressions_length = expression->compoundExpressions_length - 2;

				e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
				if (e) goto cleanupProgn2;

				(void) dl_memcopy_noOverlap(&expression->compoundExpressions[2],
				                            progn.compoundExpressions,
				                            ((expression->compoundExpressions_length - 2)
				                             * sizeof(duckLisp_ast_compoundExpression_t)));

			cleanupProgn2:
				e = DL_FREE(duckLisp->memoryAllocation, &progn.compoundExpressions);
				if (e) goto free_gensym_end;
			}

			if (duckLisp_localsLength_get(compileState) > startStack_length) {
				e = duckLisp_emit_pop(duckLisp,
				                      compileState,
				                      assembly,
				                      duckLisp_localsLength_get(compileState) - startStack_length);
				if (e) goto free_gensym_end;
			}

			e = duckLisp_popScope(duckLisp, compileState, dl_null);
			if (e) goto free_gensym_end;
		}

		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_start.value, gensym_start.value_length);
		if (e) goto free_gensym_end;
		startStack_length = duckLisp_localsLength_get(compileState);
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
		                       duckLisp_localsLength_get(compileState) - startStack_length);
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
	startStack_length = duckLisp_localsLength_get(compileState);

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
		pops = duckLisp_localsLength_get(compileState) - startStack_length;
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
			e = DL_MALLOC(duckLisp->memoryAllocation,
			              &progn.compoundExpressions,
			              expression->compoundExpressions_length - 2,
			              duckLisp_ast_compoundExpression_t);
			if (e) goto cleanup;
			(void) dl_memcopy_noOverlap(progn.compoundExpressions,
			                            &expression->compoundExpressions[2],
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;

			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto cleanupProgn1;

			(void) dl_memcopy_noOverlap(&expression->compoundExpressions[2],
			                            progn.compoundExpressions,
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));

		cleanupProgn1:
			e = DL_FREE(duckLisp->memoryAllocation, &progn.compoundExpressions);
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
		startStack_length = duckLisp_localsLength_get(compileState);
		{
			duckLisp_ast_expression_t progn;
			e = DL_MALLOC(duckLisp->memoryAllocation,
			              &progn.compoundExpressions,
			              expression->compoundExpressions_length - 2,
			              duckLisp_ast_compoundExpression_t);
			if (e) goto free_gensym_end;
			(void) dl_memcopy_noOverlap(progn.compoundExpressions,
			                            &expression->compoundExpressions[2],
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;

			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto cleanupProgn2;

			(void) dl_memcopy_noOverlap(&expression->compoundExpressions[2],
			                            progn.compoundExpressions,
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));

		cleanupProgn2:
			e = DL_FREE(duckLisp->memoryAllocation, &progn.compoundExpressions);
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
	startStack_length = duckLisp_localsLength_get(compileState);

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
		pops = duckLisp_localsLength_get(compileState) - startStack_length;
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
			e = DL_MALLOC(duckLisp->memoryAllocation,
			              &progn.compoundExpressions,
			              expression->compoundExpressions_length - 2,
			              duckLisp_ast_compoundExpression_t);
			if (e) goto cleanup;
			(void) dl_memcopy_noOverlap(progn.compoundExpressions,
			                            &expression->compoundExpressions[2],
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;

			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto cleanupProgn1;

			(void) dl_memcopy_noOverlap(&expression->compoundExpressions[2],
			                            progn.compoundExpressions,
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));

		cleanupProgn1:
			e = DL_FREE(duckLisp->memoryAllocation, &progn.compoundExpressions);
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
		startStack_length = duckLisp_localsLength_get(compileState);
		e = duckLisp_emit_nil(duckLisp, compileState, assembly);
		if (e) goto free_gensym_end;
		compileState->currentCompileState->locals_length = startStack_length;
		e = duckLisp_emit_jump(duckLisp, compileState, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto free_gensym_end;
		e = duckLisp_emit_label(duckLisp, compileState, assembly, gensym_then.value, gensym_then.value_length);
		if (e) goto free_gensym_end;
		{
			duckLisp_ast_expression_t progn;
			e = DL_MALLOC(duckLisp->memoryAllocation,
			              &progn.compoundExpressions,
			              expression->compoundExpressions_length - 2,
			              duckLisp_ast_compoundExpression_t);
			if (e) goto free_gensym_end;
			(void) dl_memcopy_noOverlap(progn.compoundExpressions,
			                            &expression->compoundExpressions[2],
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;

			e = duckLisp_generator_expression(duckLisp, compileState, assembly, &progn);
			if (e) goto cleanupProgn2;

			(void) dl_memcopy_noOverlap(&expression->compoundExpressions[2],
			                            progn.compoundExpressions,
			                            ((expression->compoundExpressions_length - 2)
			                             * sizeof(duckLisp_ast_compoundExpression_t)));

		cleanupProgn2:
			e = DL_FREE(duckLisp->memoryAllocation, &progn.compoundExpressions);
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
		startStack_length = duckLisp_localsLength_get(compileState);
		e = duckLisp_compile_expression(duckLisp,
		                                compileState,
		                                assembly,
		                                expression->compoundExpressions[0].value.identifier.value,
		                                expression->compoundExpressions[0].value.identifier.value_length,
		                                &expression->compoundExpressions[1].value.expression,
		                                &temp_index);
		if (e) goto cleanup;
		pops = duckLisp_localsLength_get(compileState) - startStack_length;
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

		startStack_length = duckLisp_localsLength_get(compileState);
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
		pops = duckLisp_localsLength_get(compileState) - startStack_length - 1;
		if (pops < 0) {
			e = dl_array_pushElements(&eString, DL_STR("if: \"else\" part of expression contains an invalid form"));
		}
		else {
			e = duckLisp_emit_move(duckLisp,
			                       compileState,
			                       assembly,
			                       startStack_length,
			                       duckLisp_localsLength_get(compileState) - 1);
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
		pops = duckLisp_localsLength_get(compileState) - startStack_length - 1;
		if (pops < 0) {
			e = dl_array_pushElements(&eString, DL_STR("if: \"then\" part of expression contains an invalid form"));
		}
		else {
			e = duckLisp_emit_move(duckLisp,
			                       compileState,
			                       assembly,
			                       startStack_length,
			                       duckLisp_localsLength_get(compileState) - 1);
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
	                                         expression->compoundExpressions[1].value.identifier.value_length,
	                                         dl_false);
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
		                                             expression->compoundExpressions[1].value.identifier.value_length,
		                                             dl_false);
		if (e) goto cleanup;
		if (!found) {
			e = duckLisp_scope_getGlobalFromName(duckLisp,
			                                     &identifier_index,
			                                     expression->compoundExpressions[1].value.identifier.value,
			                                     expression->compoundExpressions[1].value.identifier.value_length,
			                                     (&compileState->comptimeCompileState
			                                      == compileState->currentCompileState));
			if (e) goto cleanup;
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
				                            duckLisp_localsLength_get(compileState) - 1);
				if (e) goto cleanup;
			}
			else {
				e = duckLisp_emit_setStatic(duckLisp,
				                            compileState,
				                            assembly,
				                            identifier_index,
				                            duckLisp_localsLength_get(compileState) - 1);
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
			                             duckLisp_localsLength_get(compileState) - 1);
			if (e) goto cleanup;
			identifier_index = duckLisp_localsLength_get(compileState) - 1;
		}
	}
	else {
		e = duckLisp_emit_move(duckLisp, compileState, assembly, identifier_index, duckLisp_localsLength_get(compileState) - 1);
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

/* I believe this is obsolete. */
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

/* I believe this is obsolete. */
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

/* I believe this is obsolete. */
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
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
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

	outerStartStack_length = duckLisp_localsLength_get(compileState);

	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = duckLisp_localsLength_get(compileState);
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
		                       duckLisp_localsLength_get(compileState) - 1);
		if (e) goto cleanup;
		if (duckLisp_localsLength_get(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      duckLisp_localsLength_get(compileState) - innerStartStack_length - 1);
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

/* This might be good to use for pure functions, so I'm keeping it for now. */

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

/* Not a real generator since it has the wrong type. It is called only by `duckLisp_compile_expression`. If you are
   looking for `funcall`, it is elsewhere. This one can only call functions defined using `defun`. No error checking is
   done because this "generator" is expected to be called by another function that does the checking for it. */
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

	{
		duckLisp_ast_compoundExpression_t compoundExpression = expression->compoundExpressions[0];
		e = duckLisp_scope_getLocalIndexFromName(compileState->currentCompileState,
		                                         &identifier_index,
		                                         compoundExpression.value.identifier.value,
		                                         compoundExpression.value.identifier.value_length,
		                                         dl_true);
		if (e) goto cleanup;
		if (identifier_index == -1) {
			dl_ptrdiff_t scope_index;
			dl_bool_t found;
			e = duckLisp_scope_getFreeLocalIndexFromName(duckLisp,
			                                             compileState->currentCompileState,
			                                             &found,
			                                             &identifier_index,
			                                             &scope_index,
			                                             compoundExpression.value.identifier.value,
			                                             compoundExpression.value.identifier.value_length,
			                                             dl_true);
			if (e) goto cleanup;
			if (!found) {
				/* Register global (symbol) and then use it. */
				e = duckLisp_symbol_create(duckLisp,
				                           compoundExpression.value.identifier.value,
				                           compoundExpression.value.identifier.value_length);
				if (e) goto cleanup;
				dl_ptrdiff_t key = duckLisp_symbol_nameToValue(duckLisp,
				                                               compoundExpression.value.identifier.value,
				                                               compoundExpression.value.identifier.value_length);
				e = duckLisp_emit_pushGlobal(duckLisp, compileState, assembly, key);
				if (e) goto cleanup;
				identifier_index = duckLisp_localsLength_get(compileState) - 1;
			}
			else {
				e = duckLisp_emit_pushUpvalue(duckLisp, compileState, assembly, identifier_index);
				if (e) goto cleanup;
				identifier_index = duckLisp_localsLength_get(compileState) - 1;
			}
		}
		else {
			e = duckLisp_emit_pushIndex(duckLisp, compileState, assembly, identifier_index);
			if (e) goto cleanup;
		}
	}

	outerStartStack_length = duckLisp_localsLength_get(compileState);

	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = duckLisp_localsLength_get(compileState);
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
		                       duckLisp_localsLength_get(compileState) - 1);
		if (e) goto cleanup;
		if (duckLisp_localsLength_get(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      duckLisp_localsLength_get(compileState) - innerStartStack_length - 1);
			if (e) goto cleanup;
		}
	}

	/* The zeroth argument is the function name, which also happens to be a label. This fact is irrelevant for now. */
	e = duckLisp_emit_funcall(duckLisp,
	                          compileState,
	                          assembly,
	                          identifier_index,
	                          expression->compoundExpressions_length - 1);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length = outerStartStack_length + 1;

	/* Labels aren't mentioned here because they are dealt with during assembly. */

 cleanup:
	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

/* This is the *real* `funcall`. This can call any normal variable as a function, including functions defined using
   `defun`. */
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

	outerStartStack_length = duckLisp_localsLength_get(compileState);

	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = duckLisp_localsLength_get(compileState);
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
		                       innerStartStack_length, duckLisp_localsLength_get(compileState) - 1);
		if (e) goto cleanup;
		if (duckLisp_localsLength_get(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      duckLisp_localsLength_get(compileState) - innerStartStack_length - 1);
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

	outerStartStack_length = duckLisp_localsLength_get(compileState);

	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = duckLisp_localsLength_get(compileState);
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
		                       duckLisp_localsLength_get(compileState) - 1);
		if (e) goto cleanup;
		if (duckLisp_localsLength_get(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      duckLisp_localsLength_get(compileState) - innerStartStack_length - 1);
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

	outerStartStack_length = duckLisp_localsLength_get(compileState);

	/* Push all arguments onto the stack. */
	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = duckLisp_localsLength_get(compileState);
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
		                       duckLisp_localsLength_get(compileState) - 1);
		if (e) goto cleanup;
		if (duckLisp_localsLength_get(compileState) - innerStartStack_length - 1 > 0) {
			e = duckLisp_emit_pop(duckLisp,
			                      compileState,
			                      assembly,
			                      duckLisp_localsLength_get(compileState) - innerStartStack_length - 1);
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
	duckLisp_instruction_t haltInstruction = duckLisp_instruction_halt;
	duckLisp_subCompileState_t *lastSubCompileState = compileState->currentCompileState;
	dl_ptrdiff_t functionIndex = -1;
	/**/ dl_array_init(&bytecode, duckLisp->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);
	dl_array_t argumentAssembly;
	/**/ dl_array_init(&argumentAssembly,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionObject_t),
	                   dl_array_strategy_double);

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 1, dl_true);
	if (e) goto cleanupArrays;
	e = duckLisp_checkTypeAndReportError(duckLisp,
	                                     expression->compoundExpressions[0].value.identifier,
	                                     expression->compoundExpressions[0],
	                                     duckLisp_ast_type_identifier);
	if (e) goto cleanupArrays;

	/* Get macro index. */

	compileState->currentCompileState = &compileState->comptimeCompileState;
	dl_size_t lastLocalsLength = duckLisp_localsLength_get(compileState);
	compileState->currentCompileState->locals_length = duckLisp->vm.stack.elements_length;

	e = duckLisp_scope_getMacroFromName(compileState->currentCompileState,
	                                    &functionIndex,
	                                    expression->compoundExpressions[0].value.identifier.value,
	                                    expression->compoundExpressions[0].value.identifier.value_length);
	if (e) goto cleanupArrays;

	/* Generate bytecode for arguments. */

	{
		dl_size_t outerStartStack_length = duckLisp_localsLength_get(compileState);
		duckLisp_ast_compoundExpression_t quote;
		e = DL_MALLOC(duckLisp->memoryAllocation,
		              &quote.value.expression.compoundExpressions,
		              2,
		              duckLisp_ast_compoundExpression_t);
		if (e) goto cleanupArrays;
		for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
			dl_size_t innerStartStack_length = duckLisp_localsLength_get(compileState);
			quote.type = duckLisp_ast_type_expression;
			quote.value.expression.compoundExpressions_length = 2;
			quote.value.expression.compoundExpressions[0].type = duckLisp_ast_type_identifier;
			quote.value.expression.compoundExpressions[0].value.identifier.value = (dl_uint8_t *) "__quote";
			quote.value.expression.compoundExpressions[0].value.identifier.value_length = sizeof("__quote") - 1;
			quote.value.expression.compoundExpressions[1] = expression->compoundExpressions[i];
			/* call.value.expression.compoundExpressions[i] = quote; */

			e = duckLisp_compile_compoundExpression(duckLisp,
			                                        compileState,
			                                        &argumentAssembly,
			                                        expression->compoundExpressions[0].value.identifier.value,
			                                        expression->compoundExpressions[0].value.identifier.value_length,
			                                        &quote,
			                                        dl_null,
			                                        dl_null,
			                                        dl_true);
			if (e) goto cleanupArguments;

			e = duckLisp_emit_move(duckLisp,
			                       compileState,
			                       &argumentAssembly,
			                       innerStartStack_length,
			                       duckLisp_localsLength_get(compileState) - 1);
			if (e) goto cleanupArguments;
			if (duckLisp_localsLength_get(compileState) - innerStartStack_length - 1 > 0) {
				e = duckLisp_emit_pop(duckLisp,
				                      compileState,
				                      &argumentAssembly,
				                      duckLisp_localsLength_get(compileState) - innerStartStack_length - 1);
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
		                          &argumentAssembly,
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

	e = duckLisp_assemble(duckLisp, compileState, &bytecode, &argumentAssembly);
	if (e) goto cleanupArrays;

	e = dl_array_pushElement(&bytecode, &haltInstruction);
	if (e) goto cleanupArrays;

	/* Execute macro. */

	/* { */
	/* 	char *string = dl_null; */
	/* 	dl_size_t string_length = 0; */
	/* 	duckLisp_disassemble(&string, */
	/* 	                     &string_length, */
	/* 	                     duckLisp->memoryAllocation, */
	/* 	                     bytecode.elements, */
	/* 	                     bytecode.elements_length); */
	/* 	puts(string); */
	/* } */

	e = duckVM_execute(&duckLisp->vm, &return_value, bytecode.elements, bytecode.elements_length);
	eError = dl_array_pushElements(&duckLisp->errors,
	                               duckLisp->vm.errors.elements,
	                               duckLisp->vm.errors.elements_length);
	if (eError) e = eError;
	eError = dl_array_popElements(&duckLisp->vm.errors, dl_null, duckLisp->vm.errors.elements_length);
	if (eError) e = eError;
	if (e) goto cleanupArrays;

	/* Compile macro expansion. */

	e = duckLisp_objectToAST(duckLisp, &ast, &return_value, dl_true);
	if (e) goto cleanupArrays;

	/* duckLisp_ast_print(duckLisp, ast); */

	/**/ duckLisp_localsLength_decrement(compileState);

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

	/* HACK: We can't pass a compound expression up, but we can pass an expression. This is so the noscope generator
	   can inspect the returned expression and act if it sees a `__var`, `__defun`, or `__noscope`. */
	if (ast.type == duckLisp_ast_type_expression) {
		e = duckLisp_ast_expression_quit(duckLisp->memoryAllocation, expression);
		if (e) goto cleanupAST;
		*expression = ast.value.expression;
	}

 cleanupAST:

	if (ast.type != duckLisp_ast_type_expression) {
		eError = duckLisp_ast_compoundExpression_quit(duckLisp->memoryAllocation, &ast);
		if (eError) e = eError;
	}

 cleanupArrays:

	eError = duckLisp_assembly_quit(duckLisp, &argumentAssembly);
	if (eError) e = eError;

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

	dl_size_t startStack_length = duckLisp_localsLength_get(compileState);

	e = duckLisp_generator_noscope(duckLisp, compileState, assembly, expression);
	if (e) goto cleanup;

	duckLisp_scope_t scope;
	e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	if (scope.scope_uvs_length) {
		e = duckLisp_emit_releaseUpvalues(duckLisp, compileState, assembly, scope.scope_uvs, scope.scope_uvs_length);
		if (e) goto cleanup;
	}

	dl_ptrdiff_t source = duckLisp_localsLength_get(compileState) - 1;
	dl_ptrdiff_t destination = startStack_length - 1 + 1;
	if (destination < source) {
		e = duckLisp_emit_move(duckLisp, compileState, assembly, destination, source);
		if (e) goto cleanup;
	}
	dl_ptrdiff_t pops = (duckLisp_localsLength_get(compileState) - (startStack_length + 1));
	if (pops > 0) {
		e = duckLisp_emit_pop(duckLisp, compileState, assembly, pops);
		if (e) goto cleanup;
	}

	e = duckLisp_popScope(duckLisp, compileState, dl_null);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}
