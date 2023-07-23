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

#ifndef DUCKLISP_EMITTERS_H
#define DUCKLISP_EMITTERS_H

#include "duckLisp.h"

dl_error_t duckLisp_emit_nullaryOperator(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_instructionClass_t instructionClass);

dl_error_t duckLisp_emit_unaryOperator(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_instructionClass_t instructionClass,
                                       duckLisp_instructionArgClass_t argument);

dl_error_t duckLisp_emit_binaryOperator(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_instructionClass_t instructionClass,
                                        duckLisp_instructionArgClass_t argument0,
                                        duckLisp_instructionArgClass_t argument1);

dl_error_t duckLisp_emit_ternaryOperator(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_instructionClass_t instructionClass,
                                        duckLisp_instructionArgClass_t argument0,
                                        duckLisp_instructionArgClass_t argument1,
                                         duckLisp_instructionArgClass_t argument2);

dl_error_t duckLisp_emit_unaryStackOperator(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            dl_array_t *assembly,
                                            duckLisp_instructionClass_t instructionClass,
                                            dl_ptrdiff_t index);

dl_error_t duckLisp_emit_binaryStackOperator(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_instructionClass_t instructionClass,
                                             dl_ptrdiff_t index0,
                                             dl_ptrdiff_t index1);

dl_error_t duckLisp_emit_ternaryStackOperator(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              duckLisp_instructionClass_t instructionClass,
                                              dl_ptrdiff_t index0,
                                              dl_ptrdiff_t index1,
                                              dl_ptrdiff_t index2);


dl_error_t duckLisp_emit_nil(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly);

dl_error_t duckLisp_emit_makeString(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_concatenate(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     const dl_ptrdiff_t vec_index,
                                     const dl_ptrdiff_t index_index);

dl_error_t duckLisp_emit_substring(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t string_index,
                                   const dl_ptrdiff_t start_index_index,
                                   const dl_ptrdiff_t end_index_index);

dl_error_t duckLisp_emit_length(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_symbolString(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_symbolId(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_typeof(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_makeType(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly);

dl_error_t duckLisp_emit_makeInstance(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t type_index,
                                    const dl_ptrdiff_t value_index,
                                      const dl_ptrdiff_t function_index);

dl_error_t duckLisp_emit_compositeValue(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_compositeFunction(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_setCompositeValue(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           const dl_ptrdiff_t destination_index,
                                           const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_setCompositeFunction(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              const dl_ptrdiff_t destination_index,
                                              const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_nullp(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_setCar(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t destination_index,
                                const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_setCdr(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t destination_index,
                                const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_car(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_cdr(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_cons(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_vector(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t *indexes,
                                const dl_size_t indexes_length);

dl_error_t duckLisp_emit_makeVector(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t length_index,
                                    const dl_ptrdiff_t fill_index);

dl_error_t duckLisp_emit_getVecElt(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t vec_index,
                                   const dl_ptrdiff_t index_index);

dl_error_t duckLisp_emit_setVecElt(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t vec_index,
                                   const dl_ptrdiff_t index_index,
                                   const dl_ptrdiff_t value_index);

dl_error_t duckLisp_emit_return(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_size_t count);

dl_error_t duckLisp_emit_pop(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_size_t count);

dl_error_t duckLisp_emit_greater(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 const dl_ptrdiff_t source_index1,
                                 const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_equal(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t source_index1,
                               const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_less(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_not(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t index);

dl_error_t duckLisp_emit_multiply(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  const dl_ptrdiff_t source_index1,
                                  const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_divide(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index1,
                                const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_add(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_sub(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2);

dl_error_t duckLisp_emit_nop(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly);

dl_error_t duckLisp_emit_setStatic(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t destination_static_index,
                                   const dl_ptrdiff_t source_stack_index);

dl_error_t duckLisp_emit_pushGlobal(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t global_key);

dl_error_t duckLisp_emit_move(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t destination_index,
                              const dl_ptrdiff_t source_index);

dl_error_t duckLisp_emit_pushBoolean(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer);

dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer);

dl_error_t duckLisp_emit_pushDoubleFloat(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         dl_ptrdiff_t *stackIndex,
                                         const double doubleFloat);

dl_error_t duckLisp_emit_pushString(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    dl_ptrdiff_t *stackIndex,
                                    char *string,
                                    dl_size_t string_length);

dl_error_t duckLisp_emit_pushSymbol(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    dl_ptrdiff_t *stackIndex,
                                    dl_size_t id,
                                    char *string,
                                    dl_size_t string_length);

dl_error_t duckLisp_emit_pushClosure(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_bool_t variadic,
                                     const dl_ptrdiff_t function_label_index,
                                     const dl_size_t arity,
                                     const dl_ptrdiff_t *captures,
                                     const dl_size_t captures_length);

dl_error_t duckLisp_emit_releaseUpvalues(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         const dl_ptrdiff_t *upvalues,
                                         const dl_size_t upvalues_length);

dl_error_t duckLisp_emit_ccall(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               dl_ptrdiff_t callback_index);

dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t index);

dl_error_t duckLisp_emit_pushUpvalue(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     const dl_ptrdiff_t index);

dl_error_t duckLisp_emit_setUpvalue(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t upvalueIndex,
                                    const dl_ptrdiff_t index);

dl_error_t duckLisp_emit_funcall(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 dl_ptrdiff_t index,
                                 dl_uint8_t arity);

dl_error_t duckLisp_emit_apply(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               dl_ptrdiff_t index,
                               dl_uint8_t arity);

dl_error_t duckLisp_emit_acall(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t function_index,
                               const dl_size_t count);

dl_error_t duckLisp_emit_call(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              const dl_size_t label_length,
                              const dl_size_t count);

dl_error_t duckLisp_emit_brz(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             char *label,
                             dl_size_t label_length,
                             int pops);

dl_error_t duckLisp_emit_brnz(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length,
                              int pops);

dl_error_t duckLisp_emit_jump(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length);

dl_error_t duckLisp_emit_label(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               char *label,
                               dl_size_t label_length);
#endif /* DUCKLISP_EMITTERS_H */

