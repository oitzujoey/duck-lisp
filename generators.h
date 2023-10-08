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

#ifndef DUCKLISP_GENERATORS_H
#define DUCKLISP_GENERATORS_H

#include "duckLisp.h"

dl_error_t duckLisp_generator_nullaryArithmeticOperator(duckLisp_t *duckLisp,
                                                        duckLisp_compileState_t *compileState,
                                                        dl_array_t *assembly,
                                                        duckLisp_ast_expression_t *expression,
                                                        dl_error_t (*emitter)(duckLisp_t *,
                                                                              duckLisp_compileState_t *,
                                                                              dl_array_t *));

dl_error_t duckLisp_generator_unaryArithmeticOperator(duckLisp_t *duckLisp,
                                                      duckLisp_compileState_t *compileState,
                                                      dl_array_t *assembly,
                                                      duckLisp_ast_expression_t *expression,
                                                      dl_error_t (*emitter)(duckLisp_t *,
                                                                            duckLisp_compileState_t *,
                                                                            dl_array_t *,
                                                                            dl_ptrdiff_t));

dl_error_t duckLisp_generator_binaryArithmeticOperator(duckLisp_t *duckLisp,
                                                       duckLisp_compileState_t *compileState,
                                                       dl_array_t *assembly,
                                                       duckLisp_ast_expression_t *expression,
                                                       dl_error_t (*emitter)(duckLisp_t *,
                                                                             duckLisp_compileState_t *,
                                                                             dl_array_t *,
                                                                             dl_ptrdiff_t,
                                                                             dl_ptrdiff_t));

dl_error_t duckLisp_generator_ternaryArithmeticOperator(duckLisp_t *duckLisp,
                                                        duckLisp_compileState_t *compileState,
                                                        dl_array_t *assembly,
                                                        duckLisp_ast_expression_t *expression,
                                                        dl_error_t (*emitter)(duckLisp_t *,
                                                                              duckLisp_compileState_t *,
                                                                              dl_array_t *,
                                                                              dl_ptrdiff_t,
                                                                              dl_ptrdiff_t,
                                                                              dl_ptrdiff_t));

dl_error_t duckLisp_generator_declare(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_makeString(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_concatenate(duckLisp_t *duckLisp,
                                          duckLisp_compileState_t *compileState,
                                          dl_array_t *assembly,
                                          duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_substring(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_length(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_symbolString(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_symbolId(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_typeof(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_makeType(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_makeInstance(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                           duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_compositeValue(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_compositeFunction(duckLisp_t *duckLisp,
                                                duckLisp_compileState_t *compileState,
                                                dl_array_t *assembly,
                                                duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_setCompositeValue(duckLisp_t *duckLisp,
                                                duckLisp_compileState_t *compileState,
                                                dl_array_t *assembly,
                                                duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_setCompositeFunction(duckLisp_t *duckLisp,
                                                   duckLisp_compileState_t *compileState,
                                                   dl_array_t *assembly,
                                                   duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_nullp(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_setCar(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_setCdr(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_car(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_cdr(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_cons(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_list(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_vector(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_makeVector(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_getVecElt(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_setVecElt(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_quote_helper(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           dl_ptrdiff_t *stack_index,
                                           duckLisp_ast_compoundExpression_t *tree);

dl_error_t duckLisp_generator_quote(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_noscope(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_noscope2(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_noscope2_dummy(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_comptime(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_defmacro(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_lambda_raw(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression,
                                         dl_bool_t *pure);

dl_error_t duckLisp_generator_lambda(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_createVar_raw(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            dl_array_t *assembly,
                                            duckLisp_ast_expression_t *expression,
                                            dl_bool_t *pure);
dl_error_t duckLisp_generator_createVar(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_createVar_dummy(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_global(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_defun(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_generator_defun_dummy(duckLisp_t *duckLisp,
                                          duckLisp_compileState_t *compileState,
                                          dl_array_t *assembly,
                                          duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_error(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_not(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_multiply(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_divide(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_add(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_sub(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_equal(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_greater(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_less(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_while(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_unless(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_when(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_if(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_setq(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_nop(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_label(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_goto(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_acall(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_funcall(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_funcall2(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_apply(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_callback(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression);

dl_error_t duckLisp_generator_macro(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    duckLisp_ast_expression_t *expression,
                                    dl_ptrdiff_t *index);

dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression);

#endif /* DUCKLISP_GENERATORS_H */
