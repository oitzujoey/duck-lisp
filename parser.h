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

#ifndef DUCKLISP_PARSER_H
#define DUCKLISP_PARSER_H
#include "duckLisp.h"
void duckLisp_ast_compoundExpression_init(duckLisp_ast_compoundExpression_t *compoundExpression);
dl_error_t duckLisp_ast_compoundExpression_quit(dl_memoryAllocation_t *memoryAllocation,
                                                duckLisp_ast_compoundExpression_t *compoundExpression);
dl_error_t duckLisp_ast_expression_quit(dl_memoryAllocation_t *memoryAllocation, duckLisp_ast_expression_t *expression);
dl_error_t duckLisp_read(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                         const dl_bool_t parenthesisInferenceEnabled,
                         const dl_size_t maxComptimeVmObjects,
                         dl_array_t *externalDeclarations,  /* dl_array_t:duckLisp_parenthesisInferrer_declarationPrototype_t */
#endif /* USE_PARENTHESIS_INFERENCE */
                         const dl_uint8_t *fileName,
                         const dl_size_t fileName_length,
                         const dl_uint8_t *source,
                         const dl_size_t source_length,
                         duckLisp_ast_compoundExpression_t *ast,
                         dl_ptrdiff_t index,
                         dl_bool_t throwErrors);
dl_error_t duckLisp_parse_compoundExpression(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                                             const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                                             const dl_uint8_t *fileName,
                                             const dl_size_t fileName_length,
                                             const dl_uint8_t *source,
                                             const dl_size_t source_length,
                                             duckLisp_ast_compoundExpression_t *compoundExpression,
                                             dl_ptrdiff_t *index,
                                             dl_bool_t throwErrors);
dl_error_t ast_print_compoundExpression(duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression);
dl_error_t ast_print_expression(duckLisp_t duckLisp, duckLisp_ast_expression_t expression);
#endif /* DUCKLISP_PARSER_H */
