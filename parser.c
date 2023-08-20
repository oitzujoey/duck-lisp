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
#ifdef USE_PARENTHESIS_INFERENCE
#include "parenthesis-inferrer.h"
#endif /* USE_PARENTHESIS_INFERENCE */
#ifdef USE_STDLIB
#include <stdio.h>
#endif /* USE_STDLIB */
#include "DuckLib/string.h"

static dl_error_t duckLisp_error_pushSyntax(duckLisp_t *duckLisp,
                                            const char *message,
                                            const dl_size_t message_length,
                                            const char *fileName,
                                            const dl_size_t fileName_length,
                                            const dl_ptrdiff_t start_index,
                                            const dl_ptrdiff_t end_index,
                                            dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;

	duckLisp_error_t error;

	if (!throwErrors) goto cleanup;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &error.message, message_length * sizeof(char));
	if (e) goto cleanup;
	e = dl_memcopy((void *) error.message, (void *) message, message_length * sizeof(char));
	if (e) goto cleanup;

	error.message_length = message_length;
	error.fileName = fileName;
	error.fileName_length = fileName_length;
	error.start_index = start_index;
	error.end_index = end_index;

	e = dl_array_pushElement(&duckLisp->errors, &error);
	if (e) goto cleanup;

 cleanup: return e;
}

dl_error_t duckLisp_ast_compoundExpression_quit(dl_memoryAllocation_t *memoryAllocation,
                                                duckLisp_ast_compoundExpression_t *compoundExpression);
static dl_error_t parse_compoundExpression(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                                           const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                                           const char *fileName,
                                           const dl_size_t fileName_length,
                                           const char *source,
                                           const dl_size_t source_length,
                                           duckLisp_ast_compoundExpression_t *compoundExpression,
                                           dl_ptrdiff_t *index,
                                           dl_bool_t throwErrors);
dl_error_t ast_print_compoundExpression(duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression);

static dl_bool_t isIdentifierSymbol(const char character) {
	if (dl_string_isSpace(character)) return dl_false;
	switch (character) {
	case '"': /* Fall through */
	case '(': /* Fall through */
	case ')': /* Fall through */
	case ';':
		return dl_false;
	default:
		return dl_true;
	}
}


/* Does not consume the line endings. */
static dl_error_t parse_comment(duckLisp_t *duckLisp,
                                const char *source,
                                const dl_size_t source_length,
                                duckLisp_ast_compoundExpression_t *compoundExpression,
                                dl_ptrdiff_t *index,
                                dl_bool_t throwErrors) {
	(void) duckLisp;
	(void) compoundExpression;
	(void) throwErrors;

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t stop_index = start_index;

	if (source[stop_index] != ';') {
		return dl_error_invalidValue;
	}

	while ((stop_index < (dl_ptrdiff_t) source_length)
	       && (source[stop_index] != '\r')
	       && (source[stop_index] != '\n')) stop_index++;

	*index = stop_index;

	return dl_error_ok;
}

dl_error_t parse_irrelevant(duckLisp_t *duckLisp,
                            const char *source,
                            const dl_size_t source_length,
                            duckLisp_ast_compoundExpression_t *compoundExpression,
                            dl_ptrdiff_t *index,
                            dl_bool_t throwErrors) {
	(void) duckLisp;
	(void) compoundExpression;
	(void) throwErrors;
	dl_ptrdiff_t indexCopy = *index;
	while (dl_true) {
		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			return dl_error_invalidValue;
		}
		while (dl_string_isSpace(source[indexCopy])) indexCopy++;
		dl_error_t e = parse_comment(dl_null, source, source_length, dl_null, &indexCopy, dl_false);
		if (e) break;
	}
	*index = indexCopy;
	return dl_error_ok;
}


void ast_expression_init(duckLisp_ast_expression_t *expression) {
	expression->compoundExpressions = dl_null;
	expression->compoundExpressions_length = 0;
}

static dl_error_t ast_expression_quit(dl_memoryAllocation_t *memoryAllocation, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		e = duckLisp_ast_compoundExpression_quit(memoryAllocation, &expression->compoundExpressions[i]);
		if (e) goto cleanup;
	}
	if (expression->compoundExpressions != dl_null) {
		e = dl_free(memoryAllocation, (void **) &expression->compoundExpressions);
	}
 cleanup:
	return e;
}

#ifdef USE_PARENTHESIS_INFERENCE
static dl_error_t parse_literalExpression(duckLisp_t *duckLisp,
                                          const dl_bool_t parenthesisInferenceEnabled,
                                          const char *fileName,
                                          const dl_size_t fileName_length,
                                          const char *source,
                                          const dl_size_t source_length,
                                          duckLisp_ast_compoundExpression_t *compoundExpression,
                                          dl_ptrdiff_t *index,
                                          dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;

	duckLisp_ast_expression_t expression;
	dl_size_t expressionMemorySize = 0;
	expression.compoundExpressions = dl_null;
	expression.compoundExpressions_length = 0;

	/* Basic syntax checks. Need space for two parentheses and the first character must be a parenthesis. */
	if (indexCopy + 3 >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Not an expression: file too short."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	if (!parenthesisInferenceEnabled) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if (source[indexCopy] != '#') {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Expected first character to be '#'."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}
	start_index++;
	indexCopy++;

	if (source[indexCopy] != '(') {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unbalanced parenthesis."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	indexCopy++;

	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Not an expression: file too short."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	while (dl_true) {
		duckLisp_ast_compoundExpression_t subCompoundExpression;
		dl_ptrdiff_t subIndex = indexCopy;
		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			e = dl_error_invalidValue;
			/* Definitely an error. Always push the error. */
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Unmatched parenthesis."),
			                                   fileName,
			                                   fileName_length,
			                                   -1,
			                                   -1,
			                                   dl_true);
			if (eError) e = eError;
			goto cleanup;
		}
		if (source[indexCopy] == ')') break;
		e = parse_compoundExpression(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
		                             parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
		                             fileName,
		                             fileName_length,
		                             source,
		                             source_length,
		                             &subCompoundExpression,
		                             &subIndex,
		                             throwErrors);
		if (e) goto cleanup;

		dl_size_t newLength = expression.compoundExpressions_length + 1;
		if (newLength > expressionMemorySize) {
			expressionMemorySize = 2 * newLength;
		}
		e = DL_REALLOC(duckLisp->memoryAllocation,
		               &expression.compoundExpressions,
		               expressionMemorySize,
		               duckLisp_ast_compoundExpression_t);
		if (e) goto cleanup;

		expression.compoundExpressions[expression.compoundExpressions_length] = subCompoundExpression;
		expression.compoundExpressions_length++;

		indexCopy = subIndex;
		(void) parse_irrelevant(dl_null, source, source_length, dl_null, &indexCopy, dl_false);
	}
	indexCopy++;

	*index = indexCopy;

 cleanup:
	compoundExpression->type = duckLisp_ast_type_literalExpression;
	compoundExpression->value.expression = expression;

	return e;
}
#endif /* USE_PARENTHESIS_INFERENCE */

static dl_error_t parse_expression(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                                   const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                                   const char *fileName,
                                   const dl_size_t fileName_length,
                                   const char *source,
                                   const dl_size_t source_length,
                                   duckLisp_ast_compoundExpression_t *compoundExpression,
                                   dl_ptrdiff_t *index,
                                   dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t indexCopy = *index;

	duckLisp_ast_expression_t expression;
	dl_size_t expressionMemorySize = 0;
	expression.compoundExpressions = dl_null;
	expression.compoundExpressions_length = 0;

	/* Basic syntax checks. Need space for two parentheses and the first character must be a parenthesis. */
	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Not an expression: file too short."),
		                                   fileName,
		                                   fileName_length,
		                                   *index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	if (source[indexCopy] != '(') {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unbalanced parenthesis."),
		                                   fileName,
		                                   fileName_length,
		                                   *index,
		                                   indexCopy,
		                                   dl_true);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	indexCopy++;

	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Not an expression: file too short."),
		                                   fileName,
		                                   fileName_length,
		                                   *index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	while (dl_true) {
		duckLisp_ast_compoundExpression_t subCompoundExpression;
		dl_ptrdiff_t subIndex = indexCopy;
		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			e = dl_error_invalidValue;
			/* Definitely an error. Always push the error. */
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Unmatched parenthesis."),
			                                   fileName,
			                                   fileName_length,
			                                   -1,
			                                   -1,
			                                   dl_true);
			if (eError) e = eError;
			goto cleanup;
		}
		if (source[indexCopy] == ')') break;
		e = parse_compoundExpression(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
		                             parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
		                             fileName,
		                             fileName_length,
		                             source,
		                             source_length,
		                             &subCompoundExpression,
		                             &subIndex,
		                             throwErrors);
		if (e) goto cleanup;

		dl_size_t newLength = expression.compoundExpressions_length + 1;
		if (newLength > expressionMemorySize) {
			expressionMemorySize = 2 * newLength;
		}
		e = DL_REALLOC(duckLisp->memoryAllocation,
		               &expression.compoundExpressions,
		               expressionMemorySize,
		               duckLisp_ast_compoundExpression_t);
		if (e) goto cleanup;

		expression.compoundExpressions[expression.compoundExpressions_length] = subCompoundExpression;
		expression.compoundExpressions_length++;

		indexCopy = subIndex;
		(void) parse_irrelevant(dl_null, source, source_length, dl_null, &indexCopy, dl_false);
	}
	indexCopy++;

	*index = indexCopy;

 cleanup:
	compoundExpression->type = duckLisp_ast_type_expression;
	compoundExpression->value.expression = expression;

	return e;
}

dl_error_t ast_print_expression(duckLisp_t duckLisp, duckLisp_ast_expression_t expression) {
	dl_error_t e = dl_error_ok;

	if (expression.compoundExpressions_length == 0) {
		printf("NIL");
		goto cleanup;
	}
	putchar('(');
	for (dl_size_t i = 0; i < expression.compoundExpressions_length; i++) {
		e = ast_print_compoundExpression(duckLisp, expression.compoundExpressions[i]);
		if (i == expression.compoundExpressions_length - 1) {
			putchar(')');
		}
		else {
			putchar(' ');
		}
		if (e) goto cleanup;
	}

 cleanup: return e;
}

#ifdef USE_PARENTHESIS_INFERENCE
static dl_error_t ast_print_literalExpression(duckLisp_t duckLisp, duckLisp_ast_expression_t expression) {
	putchar('#');
	return ast_print_expression(duckLisp, expression);
}
#endif /* USE_PARENTHESIS_INFERENCE */


void ast_identifier_init(duckLisp_ast_identifier_t *identifier) {
	identifier->value = dl_null;
	identifier->value_length = 0;
}

static dl_error_t ast_identifier_quit(dl_memoryAllocation_t *memoryAllocation, duckLisp_ast_identifier_t *identifier) {
	(void) memoryAllocation;  /* Unused when using native malloc */
	dl_error_t e = dl_error_ok;

	identifier->value_length = 0;

	e = dl_free(memoryAllocation, (void **) &identifier->value);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

static dl_error_t parse_identifier(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                                   const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                                   const char *fileName,
                                   const dl_size_t fileName_length,
                                   const char *source,
                                   const dl_size_t source_length,
                                   duckLisp_ast_compoundExpression_t *compoundExpression,
                                   dl_ptrdiff_t *index,
                                   dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
#ifdef USE_PARENTHESIS_INFERENCE
	(void) parenthesisInferenceEnabled;
#endif /* USE_PARENTHESIS_INFERENCE */

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;
	dl_ptrdiff_t stop_index = start_index;
	dl_bool_t tempBool;

	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unexpected end of file in identifier."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	tempBool = isIdentifierSymbol(source[indexCopy]);
	if (!tempBool) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Expected an alpha or allowed symbol in identifier."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}
	indexCopy++;

	while ((indexCopy < (dl_ptrdiff_t) source_length) && isIdentifierSymbol(source[indexCopy])) {
		indexCopy++;
	}
	stop_index = indexCopy;

	duckLisp_ast_identifier_t identifier;
	identifier.value_length = 0;
	dl_size_t length = stop_index - start_index;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &identifier.value, length * sizeof(char));
	if (e) goto cleanup;

	(void) dl_memcopy_noOverlap(identifier.value, &source[start_index], length);

	identifier.value_length = length;

	compoundExpression->type = duckLisp_ast_type_identifier;
	compoundExpression->value.identifier = identifier;
	*index = stop_index;

 cleanup: return e;
}

static void ast_print_identifier(duckLisp_t duckLisp, duckLisp_ast_identifier_t identifier) {
	(void) duckLisp;

	putchar('\'');
	if (identifier.value_length == 0) {
		printf("{NULL}");
		return;
	}

	for (dl_size_t i = 0; i < identifier.value_length; i++) {
		putchar(identifier.value[i]);
	}
}

#ifdef USE_PARENTHESIS_INFERENCE
static void ast_print_callback(duckLisp_t duckLisp, duckLisp_ast_identifier_t identifier) {
	(void) duckLisp;

	putchar('#');
	if (identifier.value_length == 0) {
		printf("{NULL}");
		return;
	}

	for (dl_size_t i = 0; i < identifier.value_length; i++) {
		putchar(identifier.value[i]);
	}
}

static dl_error_t parse_callback(duckLisp_t *duckLisp,
                                 const dl_bool_t parenthesisInferenceEnabled,
                                 const char *fileName,
                                 const dl_size_t fileName_length,
                                 const char *source,
                                 const dl_size_t source_length,
                                 duckLisp_ast_compoundExpression_t *compoundExpression,
                                 dl_ptrdiff_t *index,
                                 dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;
	dl_ptrdiff_t stop_index = start_index;

	if (!parenthesisInferenceEnabled) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unexpected end of file in identifier."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	if (source[indexCopy] != '#') {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Expected first character to be '#'."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}
	start_index++;
	indexCopy++;

	while ((indexCopy < (dl_ptrdiff_t) source_length)
	       && (dl_string_isAlpha(source[indexCopy])
	           || dl_string_isDigit(source[indexCopy])
	           || isIdentifierSymbol(source[indexCopy]))) {
		indexCopy++;
	}
	stop_index = indexCopy;

	duckLisp_ast_identifier_t identifier;
	identifier.value_length = 0;
	dl_size_t length = stop_index - start_index;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &identifier.value, length * sizeof(char));
	if (e) goto cleanup;

	(void) dl_memcopy_noOverlap(identifier.value, &source[start_index], length);

	identifier.value_length = length;

	compoundExpression->type = duckLisp_ast_type_identifier;
	compoundExpression->value.identifier = identifier;
	*index = stop_index;

 cleanup: return e;
}
#endif /* USE_PARENTHESIS_INFERENCE */

void ast_bool_init(duckLisp_ast_bool_t *boolean) {
	boolean->value = dl_false;
}

static void ast_bool_quit(dl_memoryAllocation_t *memoryAllocation, duckLisp_ast_bool_t *boolean) {
	(void) memoryAllocation;

	boolean->value = dl_false;
}

static dl_error_t parse_bool(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                             const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                             const char *fileName,
                             const dl_size_t fileName_length,
                             const char *source,
                             const dl_size_t source_length,
                             duckLisp_ast_compoundExpression_t *compoundExpression,
                             dl_ptrdiff_t *index,
                             dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
#ifdef USE_PARENTHESIS_INFERENCE
	(void) parenthesisInferenceEnabled;
#endif /* USE_PARENTHESIS_INFERENCE */

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;
	dl_ptrdiff_t stop_index = start_index;
	dl_bool_t tempBool;
	const size_t sizeOfTrue = sizeof("true") - 1;
	const size_t sizeOfFalse = sizeof("false") - 1;

	if (source_length <= indexCopy + sizeOfTrue) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Encountered EOF"),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}
	(void) dl_string_compare_partial(&tempBool, &source[indexCopy], DL_STR("true"));
	if (tempBool) {
		stop_index += sizeOfTrue;
	}
	else {
		if (source_length <= indexCopy + sizeOfFalse) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Encountered EOF"),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
		(void) dl_string_compare_partial(&tempBool, &source[indexCopy], DL_STR("false"));
		if (tempBool) {
			stop_index += sizeOfFalse;
		}
		else {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a \"true\" or \"false\" in boolean."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
	}

	duckLisp_ast_bool_t boolean;
	e = dl_string_toBool(&boolean.value, &source[start_index], stop_index - start_index);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Could not convert token to bool."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	compoundExpression->type = duckLisp_ast_type_bool;
	compoundExpression->value.boolean = boolean;
	*index = stop_index;

 cleanup:
	return e;
}

static void ast_print_bool(duckLisp_t duckLisp, duckLisp_ast_bool_t boolean) {
	(void) duckLisp;

	printf("%s", boolean.value ? "true" : "false");
}


void ast_int_init(duckLisp_ast_integer_t *integer) {
	integer->value = 0;
}

static void ast_int_quit(dl_memoryAllocation_t *memoryAllocation, duckLisp_ast_integer_t *integer) {
	(void) memoryAllocation;

	integer->value = 0;
}

static dl_error_t parse_int(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                            const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                            const char *fileName,
                            const dl_size_t fileName_length,
                            const char *source,
                            const dl_size_t source_length,
                            duckLisp_ast_compoundExpression_t *compoundExpression,
                            dl_ptrdiff_t *index,
                            dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
#ifdef USE_PARENTHESIS_INFERENCE
	(void) parenthesisInferenceEnabled;
#endif /* USE_PARENTHESIS_INFERENCE */

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;
	dl_ptrdiff_t stop_index = start_index;
	dl_bool_t tempBool;
	dl_bool_t hexadecimal = dl_false;
	duckLisp_ast_integer_t integer;
	integer.value = 0;

	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unexpected end of file in integer."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	if (source[indexCopy] == '-') {
		indexCopy++;

		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Unexpected end of file in integer."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
	}

	if (!dl_string_isDigit(source[indexCopy])) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Expected a digit in integer."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}
	indexCopy++;

	/* Hexadecimal */
	if (indexCopy < (dl_ptrdiff_t) source_length) {
		if ((source[indexCopy - 1] == '0') && ((source[indexCopy] == 'x') || (source[indexCopy] == 'X'))) {
			indexCopy++;
			hexadecimal = dl_true;

			if (!dl_string_isHexadecimalDigit(source[indexCopy])) {
				eError = duckLisp_error_pushSyntax(duckLisp,
				                                   DL_STR("Expected a digit in integer."),
				                                   fileName,
				                                   fileName_length,
				                                   start_index,
				                                   indexCopy,
				                                   throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto cleanup;
			}
			indexCopy++;
		}
	}

	while (indexCopy < (dl_ptrdiff_t) source_length) {
		if (!isIdentifierSymbol(source[indexCopy])) break;
		if (hexadecimal) {
			tempBool = dl_string_isHexadecimalDigit(source[indexCopy]);
		}
		else {
			tempBool = dl_string_isDigit(source[indexCopy]);
		}
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto cleanup;
		}
		indexCopy++;
	}
	stop_index = indexCopy;

	e = dl_string_toPtrdiff(&integer.value, &source[start_index], stop_index - start_index);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Could not convert token to int."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	compoundExpression->type = duckLisp_ast_type_int;
	compoundExpression->value.integer = integer;
	*index = stop_index;

 cleanup: return e;
}

static void ast_print_int(duckLisp_t duckLisp, duckLisp_ast_integer_t integer) {
	(void) duckLisp;

	printf("%li", integer.value);
}


void ast_float_init(duckLisp_ast_float_t *floatingPoint) {
	floatingPoint->value = 0.0;
}

static void ast_float_quit(dl_memoryAllocation_t *memoryAllocation, duckLisp_ast_float_t *floatingPoint) {
	(void) memoryAllocation;

	floatingPoint->value = 0.0;
}

static dl_error_t parse_float(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                              const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                              const char *fileName,
                              const dl_size_t fileName_length,
                              const char *source,
                              const dl_size_t source_length,
                              duckLisp_ast_compoundExpression_t *compoundExpression,
                              dl_ptrdiff_t *index,
                              dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
#ifdef USE_PARENTHESIS_INFERENCE
	(void) parenthesisInferenceEnabled;
#endif /* USE_PARENTHESIS_INFERENCE */

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;
	dl_ptrdiff_t stop_index = start_index;
	dl_bool_t tempBool;
	dl_bool_t hasDecimalPointOrExponent = dl_false;

	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unexpected end of fragment in float."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	if (source[indexCopy] == '-') {
		indexCopy++;

		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a digit after minus sign."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
	}

	/* Try .1 */
	if (source[indexCopy] == '.') {
		hasDecimalPointOrExponent = dl_true;
		indexCopy++;

		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a digit after decimal point."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}

		tempBool = dl_string_isDigit(source[indexCopy]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected digit in float."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
		indexCopy++;

		while ((indexCopy < (dl_ptrdiff_t) source_length)
		       && isIdentifierSymbol(source[indexCopy])
		       && (dl_string_toLower(source[indexCopy]) != 'e')) {

			tempBool = dl_string_isDigit(source[indexCopy]);
			if (!tempBool) break;
			indexCopy++;
		}
	}
	/* Try 1.2 and 1. */
	else {
		if (!dl_string_isDigit(source[indexCopy])) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected digit in float."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
		indexCopy++;

		while ((indexCopy < (dl_ptrdiff_t) source_length)
		       && isIdentifierSymbol(source[indexCopy])
		       && (dl_string_toLower(source[indexCopy]) != 'e')
		       && (source[indexCopy] != '.')) {

			tempBool = dl_string_isDigit(source[indexCopy]);
			if (!tempBool) break;
			indexCopy++;
		}

		if (source[indexCopy] == '.') {
			hasDecimalPointOrExponent = dl_true;
			indexCopy++;

			if (indexCopy >= (dl_ptrdiff_t) source_length) {
				// eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit after decimal point."), index);
				// e = eError ? eError : dl_error_bufferOverflow;
				/* This is expected. 1. 234.e61 435. for example. */
				goto cleanup;
			}
		}

		while ((indexCopy < (dl_ptrdiff_t) source_length)
		       && isIdentifierSymbol(source[indexCopy])
		       && (dl_string_toLower(source[indexCopy]) != 'e')) {
			if (!dl_string_isDigit(source[indexCopy])) break;
			indexCopy++;
		}
	}

	/* …e3 */
	if (dl_string_toLower(source[indexCopy]) == 'e') {
		hasDecimalPointOrExponent = dl_true;
		indexCopy++;

		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected an integer in exponent of float."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}

		if (source[indexCopy] == '-') {
			indexCopy++;

			if (indexCopy >= (dl_ptrdiff_t) source_length) {
				eError = duckLisp_error_pushSyntax(duckLisp,
				                                   DL_STR("Expected a digit after minus sign."),
				                                   fileName,
				                                   fileName_length,
				                                   start_index,
				                                   indexCopy,
				                                   throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto cleanup;
			}
		}

		if (!dl_string_isDigit(source[indexCopy])) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a digit in exponent of float."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   indexCopy,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
		indexCopy++;

		while ((indexCopy < (dl_ptrdiff_t) source_length)
		       && isIdentifierSymbol(source[indexCopy])) {
			if (!dl_string_isDigit(source[indexCopy])) break;
			indexCopy++;
		}
	}

	if (!hasDecimalPointOrExponent) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	stop_index = indexCopy;

	duckLisp_ast_float_t floatingPoint;
	e = dl_string_toDouble(&floatingPoint.value, &source[start_index], stop_index - start_index);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Could not convert token to float."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	compoundExpression->type = duckLisp_ast_type_float;
	compoundExpression->value.floatingPoint = floatingPoint;
	*index = stop_index;

 cleanup: return e;
}

static void ast_print_float(duckLisp_t duckLisp, duckLisp_ast_float_t floatingPoint) {
	(void) duckLisp;

	printf("%e", floatingPoint.value);
}


void ast_string_init(duckLisp_ast_string_t *string) {
	string->value = dl_null;
	string->value_length = 0;
}

static dl_error_t ast_string_quit(dl_memoryAllocation_t *memoryAllocation, duckLisp_ast_string_t *string) {
	(void) memoryAllocation;  /* Unused when using native malloc */
	dl_error_t e = dl_error_ok;

	string->value_length = 0;

	e = dl_free(memoryAllocation, (void **) &string->value);
	if (e) goto cleanup;

 cleanup:
	return e;
}

static dl_error_t parse_string(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                               const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                               const char *fileName,
                               const dl_size_t fileName_length,
                               const char *source,
                               const dl_size_t source_length,
                               duckLisp_ast_compoundExpression_t *compoundExpression,
                               dl_ptrdiff_t *index,
                               dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
#ifdef USE_PARENTHESIS_INFERENCE
	(void) parenthesisInferenceEnabled;
#endif /* USE_PARENTHESIS_INFERENCE */

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;
	dl_ptrdiff_t stop_index = start_index;

	if (indexCopy >= (dl_ptrdiff_t) source_length) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Zero length fragment."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}

	if (source[indexCopy] == '"') {
		indexCopy++;

		while (indexCopy < (dl_ptrdiff_t) source_length) {
			if (source[indexCopy] == '\\') {
				/* Eat character. */
				indexCopy++;

				if (indexCopy >= (dl_ptrdiff_t) source_length) {
					eError = duckLisp_error_pushSyntax(duckLisp,
					                                   DL_STR("Expected character in string escape sequence."),
					                                   fileName,
					                                   fileName_length,
					                                   start_index,
					                                   indexCopy,
					                                   throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto cleanup;
				}
			}
			else if (source[indexCopy] == '"') {
				indexCopy++;
				break;
			}

			indexCopy++;
		}

		if (indexCopy >= (dl_ptrdiff_t) source_length) {
			e = dl_error_invalidValue;
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("String missing closing quote."),
			                                   fileName,
			                                   fileName_length,
			                                   start_index,
			                                   -1,
			                                   dl_true);
			e = eError ? eError : dl_error_invalidValue;
			goto cleanup;
		}
	}
	else {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Not a string."),
		                                   fileName,
		                                   fileName_length,
		                                   start_index,
		                                   indexCopy,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto cleanup;
	}
	stop_index = indexCopy;

	dl_ptrdiff_t token_index = start_index + 1;
	dl_ptrdiff_t token_length = stop_index - start_index - 2;

	void *destination = dl_null;
	const char *s = dl_null;
	dl_bool_t escape = dl_false;

	duckLisp_ast_string_t string;
	string.value_length = 0;

	if (token_length) {
		e = DL_MALLOC(duckLisp->memoryAllocation, &string.value, token_length, char);
		if (e) goto cleanup;
	}
	else {
		string.value = dl_null;
	}

	string.value_length = token_length;

	destination = string.value;
	s = &source[token_index];
	for (char *d = destination; s < &source[token_index] + token_length; s++) {
		if (escape) {
			escape = dl_false;
			if (*s == 'n') {
				*d++ = '\n';
				continue;
			}
		}
		else if (*s == '\\') {
			escape = dl_true;
			--string.value_length;
			continue;
		}
		*d++ = *s;
	}

	compoundExpression->type = duckLisp_ast_type_string;
	compoundExpression->value.string = string;
	*index = stop_index;

 cleanup: return e;
}

static void ast_print_string(duckLisp_t duckLisp, duckLisp_ast_string_t string) {
	(void) duckLisp;

	if (string.value_length == 0) {
		printf("{NULL}");
		return;
	}

	putchar('"');
	for (dl_size_t i = 0; i < string.value_length; i++) {
		if (string.value[i] == '\n') {
			putchar ('\\');
			putchar ('n');
		}
		else {
			switch (string.value[i]) {
			case '"':
			case '\\':
				putchar('\\');
			}
			putchar(string.value[i]);
		}
	}
	putchar('"');
}


void duckLisp_ast_compoundExpression_init(duckLisp_ast_compoundExpression_t *compoundExpression) {
	compoundExpression->type = duckLisp_ast_type_none;
}

dl_error_t duckLisp_ast_compoundExpression_quit(dl_memoryAllocation_t *memoryAllocation,
                                                duckLisp_ast_compoundExpression_t *compoundExpression) {
	dl_error_t e = dl_error_ok;

	switch (compoundExpression->type) {
	case duckLisp_ast_type_string:
		(void) ast_string_quit(memoryAllocation, &compoundExpression->value.string);
		break;
	case duckLisp_ast_type_bool:
		(void) ast_bool_quit(memoryAllocation, &compoundExpression->value.boolean);
		break;
	case duckLisp_ast_type_int:
		(void) ast_int_quit(memoryAllocation, &compoundExpression->value.integer);
		break;
	case duckLisp_ast_type_float:
		(void) ast_float_quit(memoryAllocation, &compoundExpression->value.floatingPoint);
		break;
	case duckLisp_ast_type_identifier:
		e = ast_identifier_quit(memoryAllocation, &compoundExpression->value.identifier);
		break;
	case duckLisp_ast_type_expression:
		e = ast_expression_quit(memoryAllocation, &compoundExpression->value.expression);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) goto cleanup;

 cleanup:

	compoundExpression->type = duckLisp_ast_type_none;

	return e;
}

static dl_error_t parse_compoundExpression(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                                           const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
                                           const char *fileName,
                                           const dl_size_t fileName_length,
                                           const char *source,
                                           const dl_size_t source_length,
                                           duckLisp_ast_compoundExpression_t *compoundExpression,
                                           dl_ptrdiff_t *index,
                                           dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	(void) throwErrors;

	dl_ptrdiff_t start_index = *index;
	dl_ptrdiff_t indexCopy = start_index;

	typedef struct {
		dl_error_t (*reader) (duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
		                      const dl_bool_t parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
		                      const char *fileName,
		                      const dl_size_t fileName_length,
		                      const char *source,
		                      const dl_size_t source_length,
		                      duckLisp_ast_compoundExpression_t *compoundExpression,
		                      dl_ptrdiff_t *index,
		                      dl_bool_t throwErrors);
		duckLisp_ast_type_t type;
	} readerStruct_t;

	readerStruct_t readerStruct[] = {
		{.reader = parse_bool,              .type = duckLisp_ast_type_bool},
		{.reader = parse_float,             .type = duckLisp_ast_type_float},
		{.reader = parse_int,               .type = duckLisp_ast_type_int},
		{.reader = parse_string,            .type = duckLisp_ast_type_string},
#ifdef USE_PARENTHESIS_INFERENCE
		{.reader = parse_literalExpression, .type = duckLisp_ast_type_literalExpression},
		{.reader = parse_callback,          .type = duckLisp_ast_type_callback},
#endif /* USE_PARENTHESIS_INFERENCE */
		{.reader = parse_identifier,        .type = duckLisp_ast_type_identifier},
		{.reader = parse_expression,        .type = duckLisp_ast_type_expression},
	};

	(void) parse_irrelevant(dl_null,
	                        source,
	                        source_length,
	                        dl_null,
	                        &indexCopy,
	                        dl_false);
	DL_DOTIMES(i, sizeof(readerStruct)/sizeof(readerStruct_t)) {
		dl_ptrdiff_t localIndex = indexCopy;
		e = readerStruct[i].reader(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
		                           parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
		                           fileName,
		                           fileName_length,
		                           source,
		                           source_length,
		                           compoundExpression,
		                           &localIndex,
		                           dl_false);
		if (!e) {
			/* Success */
			*index = localIndex;
			compoundExpression->type = readerStruct[i].type;
			goto cleanup;
		}
		if (e != dl_error_invalidValue) {
			/* Unexpected error */
			goto cleanup;
		}
		/* if (duckLisp->errors.elements_length) break; */
	}
	/* Error */

 cleanup: return e;
}

dl_error_t ast_print_compoundExpression(duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;

	switch (compoundExpression.type) {
	case duckLisp_ast_type_bool:
		/**/ ast_print_bool(duckLisp, compoundExpression.value.boolean);
		break;
	case duckLisp_ast_type_int:
		/**/ ast_print_int(duckLisp, compoundExpression.value.integer);
		break;
	case duckLisp_ast_type_float:
		/**/ ast_print_float(duckLisp, compoundExpression.value.floatingPoint);
		break;
	case duckLisp_ast_type_string:
		/**/ ast_print_string(duckLisp, compoundExpression.value.string);
		break;
#ifdef USE_PARENTHESIS_INFERENCE
	case duckLisp_ast_type_callback:
		/**/ ast_print_callback(duckLisp, compoundExpression.value.identifier);
		break;
#endif /* USE_PARENTHESIS_INFERENCE */
	case duckLisp_ast_type_identifier:
		/**/ ast_print_identifier(duckLisp, compoundExpression.value.identifier);
		break;
#ifdef USE_PARENTHESIS_INFERENCE
	case duckLisp_ast_type_literalExpression:
		e = ast_print_literalExpression(duckLisp, compoundExpression.value.expression);
		break;
#endif /* USE_PARENTHESIS_INFERENCE */
	case duckLisp_ast_type_expression:
		e = ast_print_expression(duckLisp, compoundExpression.value.expression);
		break;
	default:
		printf("Compound expression: Type %u\n", compoundExpression.type);
		e = dl_error_shouldntHappen;
	}

	return e;
}


dl_error_t duckLisp_read(duckLisp_t *duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
                         const dl_bool_t parenthesisInferenceEnabled,
                         const dl_size_t maxComptimeVmObjects,
#endif /* USE_PARENTHESIS_INFERENCE */
                         const char *fileName,
                         const dl_size_t fileName_length,
                         const char *source,
                         const dl_size_t source_length,
                         duckLisp_ast_compoundExpression_t *ast,
                         dl_ptrdiff_t index,
                         dl_bool_t throwErrors) {
	dl_error_t e = parse_compoundExpression(duckLisp,
#ifdef USE_PARENTHESIS_INFERENCE
	                                        parenthesisInferenceEnabled,
#endif /* USE_PARENTHESIS_INFERENCE */
	                                        fileName,
	                                        fileName_length,
	                                        source,
	                                        source_length,
	                                        ast,
	                                        &index,
	                                        throwErrors);
	if (e) goto cleanup;

#ifdef USE_PARENTHESIS_INFERENCE
	if (parenthesisInferenceEnabled) {
		e = inferParentheses(duckLisp->memoryAllocation,
		                     maxComptimeVmObjects,
		                     &duckLisp->errors,
		                     fileName,
		                     fileName_length,
		                     ast);
		if (e) goto cleanup;
	}
#endif /* USE_PARENTHESIS_INFERENCE */

 cleanup: return e;
}
