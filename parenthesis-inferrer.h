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

#ifndef PARENTHESIS_INFERRER_H
#define PARENTHESIS_INFERRER_H

#include "duckLisp.h"

typedef struct {
	dl_uint8_t *name;
	dl_size_t name_length;
	dl_uint8_t *type;
	dl_size_t type_length;
	dl_uint8_t *script;
	dl_size_t script_length;
} duckLisp_parenthesisInferrer_declarationPrototype_t;

dl_error_t duckLisp_parenthesisInferrer_declarationPrototype_prettyPrint(dl_array_t *string_array,
                                                                         duckLisp_parenthesisInferrer_declarationPrototype_t declarationPrototype);

dl_error_t duckLisp_inferParentheses(dl_memoryAllocation_t *memoryAllocation,
                                     const dl_size_t maxComptimeVmObjects,
                                     dl_array_t *errors,
                                     dl_array_t *log,
                                     const dl_uint8_t *fileName,
                                     const dl_size_t fileName_length,
                                     duckLisp_ast_compoundExpression_t *ast,
                                     dl_array_t *externalDeclarations  /* dl_array_t:duckLisp_parenthesisInferrer_declarationPrototype_t */);

#endif /* PARENTHESIS_INFERRER_H */
