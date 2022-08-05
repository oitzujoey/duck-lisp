
#include "duckLisp.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "DuckLib/sort.h"
#include "DuckLib/trie.h"
#include <stdio.h>

dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp, dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression);

/*
  ===============
  Error reporting
  ===============
*/

static dl_error_t duckLisp_error_pushSyntax(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length,
                                            const dl_ptrdiff_t index, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;

	duckLisp_error_t error;

	if (!throwErrors) {
		goto l_cleanup;
	}

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &error.message, message_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}
	e = dl_memcopy((void *) error.message, (void *) message, message_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}

	error.message_length = message_length;
	error.index = index;

	e = dl_array_pushElement(&duckLisp->errors, &error);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_error_pushRuntime(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_error_t error;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &error.message, message_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}
	e = dl_memcopy((void *) error.message, (void *) message, message_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}

	error.message_length = message_length;
	error.index = -1;

	e = dl_array_pushElement(&duckLisp->errors, &error);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_checkArgsAndReportError(duckLisp_t *duckLisp,
                                            duckLisp_ast_expression_t astExpression,
                                            const dl_size_t numArgs) {
	dl_error_t e = dl_error_ok;

	dl_array_t string;
	char *fewMany = dl_null;
	dl_size_t fewMany_length = 0;

	/**/ dl_array_init(&string, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	if (astExpression.compoundExpressions_length == 0) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	if (astExpression.compoundExpressions[0].type != duckLisp_ast_type_identifier) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	if (astExpression.compoundExpressions_length != numArgs) {
		e = dl_array_pushElements(&string, DL_STR("Too "));
		if (e) {
			goto l_cleanup;
		}

		if (astExpression.compoundExpressions_length < numArgs) {
			fewMany = "few";
			fewMany_length = sizeof("few") - 1;
		}
		else {
			fewMany = "many";
			fewMany_length = sizeof("many") - 1;
		}

		e = dl_array_pushElements(&string, fewMany, fewMany_length);
		if (e) {
			goto l_cleanup;
		}

		e = dl_array_pushElements(&string, DL_STR(" arguments for function \""));
		if (e) {
			goto l_cleanup;
		}

		e = dl_array_pushElements(&string, astExpression.compoundExpressions[0].value.identifier.value,
		                          astExpression.compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}

		e = dl_array_pushElements(&string, DL_STR("\"."));
		if (e) {
			goto l_cleanup;
		}

		e = duckLisp_error_pushRuntime(duckLisp,
		                               (char *) string.elements,
		                               string.elements_length * string.element_size);
		if (e) {
			goto l_cleanup;
		}

		e = dl_error_invalidValue;
	}

 l_cleanup:

	return e;
}

/*
  ======
  Parser
  ======
*/

static dl_error_t cst_parse_compoundExpression(duckLisp_t *duckLisp,
                                               duckLisp_cst_compoundExpression_t *compoundExpression,
                                               char *source,
                                               const dl_ptrdiff_t start_index,
                                               const dl_size_t length,
                                               dl_bool_t throwErrors);
static dl_error_t cst_print_compoundExpression(duckLisp_t duckLisp,
                                               duckLisp_cst_compoundExpression_t compoundExpression);
static dl_error_t ast_generate_compoundExpression(duckLisp_t *duckLisp,
                                                  duckLisp_ast_compoundExpression_t *compoundExpression,
                                                  duckLisp_cst_compoundExpression_t compoundExpressionCST,
                                                  dl_bool_t throwErrors);

static void cst_isIdentifierSymbol(dl_bool_t *result, const char character) {
	/* switch (character) { */
	/* case '~': */
	/* case '`': */
	/* case '!': */
	/* case '@': */
	/* case '$': */
	/* case '%': */
	/* case '^': */
	/* case '&': */
	/* case '*': */
	/* case '_': */
	/* case '-': */
	/* case '+': */
	/* case '=': */
	/* case '[': */
	/* case '{': */
	/* case ']': */
	/* case '}': */
	/* case '|': */
	/* case '\\': */
	/* case ':': */
	/* case ';': */
	/* case '<': */
	/* case ',': */
	/* case '>': */
	/* case '.': */
	/* case '?': */
	/* case '/': */
	/*		*result = dl_true; */
	/*		break; */
	/* default: */
	/*		*result = dl_false; */
	/* } */
	dl_bool_t isSpace;
	/**/ dl_string_isSpace(&isSpace, character);
	if (!isSpace
	    && (character != '(')
	    && (character != ')')) {
		*result = dl_true;
	}
	else *result = dl_false;
}


static void cst_expression_init(duckLisp_cst_expression_t *expression) {
	expression->compoundExpressions = dl_null;
	expression->compoundExpressions_length = 0;
}

static dl_error_t cst_parse_expression(duckLisp_t *duckLisp,
                                       duckLisp_cst_compoundExpression_t *compoundExpression,
                                       char *source,
                                       const dl_ptrdiff_t start_index,
                                       const dl_size_t length,
                                       dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eCleanup = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	struct {
		dl_bool_t bracketStack;
	} d = {0};

	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;

	dl_ptrdiff_t child_start_index;
	dl_ptrdiff_t child_length;

	dl_array_t bracketStack;
	char topChar;

	dl_bool_t justPopped = dl_false;
	dl_bool_t wasWhitespace = dl_false;

	duckLisp_cst_expression_t *expression = &compoundExpression->value.expression;

	/**/ cst_expression_init(expression);

	// Quick syntax checks.
	if (stop_index - index < 2) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Not an expression: too short."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	if ((source[start_index] != '(') || (source[stop_index - 1] != ')')) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Not an expression: no parentheses."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	(--stop_index,index++); // Syntax just for fun.
	if (index == stop_index) {
		expression->compoundExpressions = dl_null;
		expression->compoundExpressions_length = 0;
		goto l_cleanup;
	}

	/*
	  Unfortunately, this function needs to know a bit about the structure of the
	  leaves. This is where all the magic happens.
	*/

	/* No error */ dl_array_init(&bracketStack, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	d.bracketStack = dl_true;

	child_start_index = index;

	while (index < stop_index) {

		/**/ dl_string_isSpace(&tempBool, source[index]);
		if ((bracketStack.elements_length == 0) && ((!tempBool && wasWhitespace) || justPopped)) {
			// Set start index.
			child_start_index = index;
		}
		wasWhitespace = tempBool;

		justPopped = dl_false;

		// Manage brackets.
		if (source[index] == '(') {
			e = dl_array_pushElement(&bracketStack, (void *) &source[index]);
			if (e) {
				goto l_cleanup;
			}
		}
		else if (source[index] == ')') {
			if (bracketStack.elements_length != 0) {
				// Check for opening parenthesis.
				e = dl_array_getTop(&bracketStack, (void *) &topChar);
				if (e) {
					goto l_cleanup;
				}
				if (topChar != '(') {
					eError = duckLisp_error_pushSyntax(duckLisp,
					                                   DL_STR("No open parenthesis for closing parenthesis."),
					                                   index, throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}

				// Pop opening parenthesis.
				e = dl_array_popElement(&bracketStack, dl_null);
				if (e) {
					goto l_cleanup;
				}

				justPopped = dl_true;
			}
			else {
				eError = duckLisp_error_pushSyntax(duckLisp,
				                                   DL_STR("No open parenthesis for closing parenthesis."),
				                                   index,
				                                   throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
		}
		else if (source[index] == '"') {
			if (bracketStack.elements_length != 0) {
				e = dl_array_getTop(&bracketStack, (void *) &topChar);
				if (e) {
					goto l_cleanup;
				}
			}
			if ((bracketStack.elements_length == 0) || (topChar != source[index])) {
				e = dl_array_pushElement(&bracketStack, (void *) &source[index]);
				if (e) {
					goto l_cleanup;
				}
			}
			else {
				e = dl_array_popElement(&bracketStack, dl_null);
				if (e) {
					goto l_cleanup;
				}
			}

			// justPopped = dl_true;
		}

		index++;

		if (index < stop_index) /**/ dl_string_isSpace(&tempBool, source[index]);

		if ((bracketStack.elements_length == 0)
		    && ((index >= stop_index) || (tempBool && !wasWhitespace) || justPopped)) {
			// if (index >= stop_index) {
			//		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("File ended in expression or string brackets."), index);
			//		e = eError ? eError : dl_error_invalidValue;
			//		goto l_cleanup;
			// }

			if (index >= stop_index) {
				/**/ dl_string_isSpace(&tempBool, source[index - 1]);
				// tempBool = dl_true;
				tempBool = !tempBool;
			}
			else {
				/**/ dl_string_isSpace(&tempBool, source[index]);
			}
			if (tempBool) {
				child_length = index - child_start_index;

				// We have you now!

				// Allocate an expression.
				e = dl_realloc(duckLisp->memoryAllocation,
				               (void **) &expression->compoundExpressions,
				               (expression->compoundExpressions_length + 1)
				               * sizeof(duckLisp_cst_compoundExpression_t));
				if (e) {
					goto l_cleanup;
				}
				/**/ cst_compoundExpression_init(&expression->compoundExpressions[expression->compoundExpressions_length]);

				// putchar('[');
				// for (dl_size_t i = child_start_index; i < child_start_index + child_length; i++) {
				//		putchar(source[i]);
				// }
				// putchar(']');
				// putchar('\n');

				e = cst_parse_compoundExpression(duckLisp,
				                                 &expression->compoundExpressions[expression->compoundExpressions_length],
				                                 source,
				                                 child_start_index,
				                                 child_length, throwErrors);
				if (e) {
					goto l_cleanup;
				}

				expression->compoundExpressions_length++;
			}
		}
	}

	if (bracketStack.elements_length != 0) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("No closing parenthesis for opening parenthesis."),
		                                   index,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

 l_cleanup:

	if (d.bracketStack) {
		eCleanup = dl_array_quit(&bracketStack);
		if (!e) {
			e = eCleanup;
		}
	}

	return e;
}

static dl_error_t cst_print_expression(duckLisp_t duckLisp, duckLisp_cst_expression_t expression) {
	dl_error_t e = dl_error_ok;

	if (expression.compoundExpressions_length == 0) {
		printf("{NULL}");
		goto l_cleanup;
	}
	putchar('(');
	for (dl_size_t i = 0; i < expression.compoundExpressions_length; i++) {
		e = cst_print_compoundExpression(duckLisp, expression.compoundExpressions[i]);
		if (i == expression.compoundExpressions_length - 1) {
			putchar(')');
		}
		else {
			putchar(' ');
		}
		if (e) {
			goto l_cleanup;
		}
	}

 l_cleanup:

	return e;
}

void ast_expression_init(duckLisp_ast_expression_t *expression) {
	expression->compoundExpressions = dl_null;
	expression->compoundExpressions_length = 0;
}

static dl_error_t ast_generate_expression(duckLisp_t *duckLisp, duckLisp_ast_expression_t *expression,
                                          const duckLisp_cst_expression_t expressionCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &expression->compoundExpressions,
	              expressionCST.compoundExpressions_length * sizeof(duckLisp_ast_compoundExpression_t));
	if (e) {
		goto l_cleanup;
	}

	for (dl_size_t i = 0; i < expressionCST.compoundExpressions_length; i++) {
		e = ast_generate_compoundExpression(duckLisp,
		                                    &expression->compoundExpressions[i],
		                                    expressionCST.compoundExpressions[i],
		                                    throwErrors);
		if (e) {
			goto l_cleanup;
		}
	}

	expression->compoundExpressions_length = expressionCST.compoundExpressions_length;

 l_cleanup:

	return e;
}

static dl_error_t ast_print_expression(duckLisp_t duckLisp, duckLisp_ast_expression_t expression) {
	dl_error_t e = dl_error_ok;

	if (expression.compoundExpressions_length == 0) {
		printf("{NULL}");
		goto l_cleanup;
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
		if (e) {
			goto l_cleanup;
		}
	}

 l_cleanup:

	return e;
}


void cst_identifier_init(duckLisp_cst_identifier_t *identifier) {
	identifier->token_index = 0;
	identifier->token_length = 0;
}

static void cst_identifier_quit(duckLisp_t *duckLisp, duckLisp_cst_identifier_t *identifier) {
	(void) duckLisp;
	identifier->token_index = 0;
	identifier->token_length = 0;
}

static dl_error_t cst_parse_identifier(duckLisp_t *duckLisp,
                                       duckLisp_cst_compoundExpression_t *compoundExpression,
                                       char *source,
                                       const dl_ptrdiff_t start_index,
                                       const dl_size_t length,
                                       dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;

	if (index >= stop_index) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unexpected end of file in identifier."),
		                                   index,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	/* No error */ dl_string_isAlpha(&tempBool, source[index]);
	if (!tempBool) {
		/* No error */ cst_isIdentifierSymbol(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a alpha or allowed symbol in identifier."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}
	index++;

	while (index < stop_index) {

		/* No error */ dl_string_isAlpha(&tempBool, source[index]);
		if (!tempBool) {
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				/* No error */ cst_isIdentifierSymbol(&tempBool, source[index]);
				if (!tempBool) {
					eError = duckLisp_error_pushSyntax(duckLisp,
					                                   DL_STR("Expected a alpha, digit, or allowed symbol in identifier."),
					                                   index,
					                                   throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}
			}
		}
		index++;
	}

	compoundExpression->value.identifier.token_index = start_index;
	compoundExpression->value.identifier.token_length = length;

 l_cleanup:

	return e;
}

static void cst_print_identifier(duckLisp_t duckLisp, duckLisp_cst_identifier_t identifier) {
	if (identifier.token_length == 0) {
		puts("{NULL}");
		return;
	}

	for (dl_size_t i = identifier.token_index; i < identifier.token_index + identifier.token_length; i++) {
		putchar(((char *) duckLisp.source.elements)[i]);
	}
}

void ast_identifier_init(duckLisp_ast_identifier_t *identifier) {
	identifier->value = dl_null;
	identifier->value_length = 0;
}

static dl_error_t ast_identifier_quit(duckLisp_t *duckLisp, duckLisp_ast_identifier_t *identifier) {
	dl_error_t e = dl_error_ok;

	identifier->value_length = 0;

	e = dl_free(duckLisp->memoryAllocation, (void **) &identifier->value);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

static dl_error_t ast_generate_identifier(duckLisp_t *duckLisp, duckLisp_ast_identifier_t *identifier,
                                          const duckLisp_cst_identifier_t identifierCST, dl_bool_t throwErrors) {
	(void) throwErrors;
	dl_error_t e = dl_error_ok;

	identifier->value_length = 0;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &identifier->value, identifierCST.token_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}

	/**/ dl_memcopy_noOverlap(identifier->value,
	                          &((char *) duckLisp->source.elements)[identifierCST.token_index],
	                          identifierCST.token_length);

	identifier->value_length = identifierCST.token_length;

 l_cleanup:

	return e;
}

static void ast_print_identifier(duckLisp_t duckLisp, duckLisp_ast_identifier_t identifier) {
	(void) duckLisp;

	if (identifier.value_length == 0) {
		printf("{NULL}");
		return;
	}

	for (dl_size_t i = 0; i < identifier.value_length; i++) {
		putchar(identifier.value[i]);
	}
}


void cst_bool_init(duckLisp_cst_bool_t *boolean) {
	boolean->token_length = 0;
	boolean->token_index = 0;
}

void cst_bool_quit(duckLisp_t *duckLisp, duckLisp_cst_bool_t *boolean) {
	(void) duckLisp;

	boolean->token_index = 0;
	boolean->token_length = 0;
}

static dl_error_t cst_parse_bool(duckLisp_t *duckLisp,
                                 duckLisp_cst_compoundExpression_t *compoundExpression,
                                 char *source,
                                 const dl_ptrdiff_t start_index,
                                 const dl_size_t length,
                                 dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = start_index;
	dl_bool_t tempBool;

	/* No error */ dl_string_compare(&tempBool, &source[start_index], length, DL_STR("true"));
	if (!tempBool) {
		/* No error */ dl_string_compare(&tempBool, &source[start_index], length, DL_STR("false"));
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a \"true\" or \"false\" in boolean."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}

	compoundExpression->value.boolean.token_index = start_index;
	compoundExpression->value.boolean.token_length = length;

 l_cleanup:

	return e;
}

static void cst_print_bool(duckLisp_t duckLisp, duckLisp_cst_bool_t boolean) {
	if (boolean.token_length == 0) {
		puts("(NULL)");
		return;
	}

	for (dl_size_t i = boolean.token_index; i < boolean.token_index + boolean.token_length; i++) {
		putchar(((char *) duckLisp.source.elements)[i]);
	}
}

void ast_bool_init(duckLisp_ast_bool_t *boolean) {
	boolean->value = dl_false;
}

static void ast_bool_quit(duckLisp_t *duckLisp, duckLisp_ast_bool_t *boolean) {
	(void) duckLisp;

	boolean->value = dl_false;
}

static dl_error_t ast_generate_bool(duckLisp_t *duckLisp, duckLisp_ast_bool_t *boolean,
                                    const duckLisp_cst_bool_t booleanCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	e = dl_string_toBool(&boolean->value,
	                     &((char *) duckLisp->source.elements)[booleanCST.token_index],
	                     booleanCST.token_length);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Could not convert token to bool."),
		                                   booleanCST.token_index,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

static void ast_print_bool(duckLisp_t duckLisp, duckLisp_ast_bool_t boolean) {
	(void) duckLisp;

	printf("%s", boolean.value ? "true" : "false");
}


void cst_int_init(duckLisp_cst_integer_t *integer) {
	integer->token_length = 0;
	integer->token_index = 0;
}

void cst_int_quit(duckLisp_t *duckLisp, duckLisp_cst_integer_t *integer) {
	(void) duckLisp;

	integer->token_index = 0;
	integer->token_length = 0;
}

static dl_error_t cst_parse_int(duckLisp_t *duckLisp,
                                duckLisp_cst_compoundExpression_t *compoundExpression,
                                char *source,
                                const dl_ptrdiff_t start_index,
                                const dl_size_t length,
                                dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;

	if (index >= stop_index) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Unexpected end of file in integer."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	if (source[index] == '-') {
		index++;

		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Unexpected end of file in integer."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}

	/* No error */ dl_string_isDigit(&tempBool, source[index]);
	if (!tempBool) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit in integer."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	while (index < stop_index) {
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit in integer."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
	}

	compoundExpression->value.integer.token_index = start_index;
	compoundExpression->value.integer.token_length = length;

 l_cleanup:

	return e;
}

static void cst_print_int(duckLisp_t duckLisp, duckLisp_cst_integer_t integer) {
	if (integer.token_length == 0) {
		puts("{NULL}");
		return;
	}

	for (dl_size_t i = integer.token_index; i < integer.token_index + integer.token_length; i++) {
		putchar(((char *) duckLisp.source.elements)[i]);
	}
}

void ast_int_init(duckLisp_ast_integer_t *integer) {
	integer->value = 0;
}

static void ast_int_quit(duckLisp_t *duckLisp, duckLisp_ast_integer_t *integer) {
	(void) duckLisp;

	integer->value = 0;
}

static dl_error_t ast_generate_int(duckLisp_t *duckLisp,
                                   duckLisp_ast_integer_t *integer,
                                   const duckLisp_cst_integer_t integerCST,
                                   dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	e = dl_string_toPtrdiff(&integer->value,
	                        &((char *) duckLisp->source.elements)[integerCST.token_index],
	                        integerCST.token_length);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Could not convert token to int."),
		                                   integerCST.token_index,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

static void ast_print_int(duckLisp_t duckLisp, duckLisp_ast_integer_t integer) {
	(void) duckLisp;

	printf("%lli", integer.value);
}


void cst_float_init(duckLisp_cst_float_t *floatingPoint) {
	floatingPoint->token_length = 0;
	floatingPoint->token_index = 0;
}

void cst_float_quit(duckLisp_t *duckLisp, duckLisp_cst_float_t *floatingPoint) {
	(void) duckLisp;

	floatingPoint->token_index = 0;
	floatingPoint->token_length = 0;
}

static dl_error_t cst_parse_float(duckLisp_t *duckLisp,
                                  duckLisp_cst_compoundExpression_t *compoundExpression,
                                  char *source,
                                  const dl_ptrdiff_t start_index,
                                  const dl_size_t length,
                                  dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;

	if (index >= stop_index) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Unexpected end of fragment in float."),
		                                   index,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	if (source[index] == '-') {
		index++;

		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a digit after minus sign."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}

	// Try .1
	if (source[index] == '.') {
		index++;

		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a digit after decimal point."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}

		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;

		while ((index < stop_index) && (dl_string_toLower(source[index]) != 'e')) {

			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}

			index++;
		}
	}
	// Try 1.2, 1., and 1
	else {
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;

		while ((index < stop_index) && (dl_string_toLower(source[index]) != 'e') && (source[index] != '.')) {

			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}

			index++;
		}

		if (source[index] == '.') {
			index++;

			if (index >= stop_index) {
				// eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit after decimal point."), index);
				// e = eError ? eError : dl_error_bufferOverflow;
				// This is expected. 1. 234.e61			 435. for example.
				goto l_cleanup;
			}
		}

		while ((index < stop_index) && (dl_string_toLower(source[index]) != 'e')) {

			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit in float."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}

			index++;
		}
	}

	// …e3
	if (dl_string_toLower(source[index]) == 'e') {
		index++;

		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected an integer in exponent of float."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}

		if (source[index] == '-') {
			index++;

			if (index >= stop_index) {
				eError = duckLisp_error_pushSyntax(duckLisp,
				                                   DL_STR("Expected a digit after minus sign."),
				                                   index,
				                                   throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
		}

		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected a digit in exponent of float."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;

		while (index < stop_index) {
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_pushSyntax(duckLisp,
				                                   DL_STR("Expected a digit in exponent of float."),
				                                   index,
				                                   throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}

			index++;
		}
	}

	if (index != stop_index) {
		// eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("."), index);
		// e = eError ? eError : dl_error_cantHappen;
		e = dl_error_cantHappen;
		goto l_cleanup;
	}

	compoundExpression->value.floatingPoint.token_index = start_index;
	compoundExpression->value.floatingPoint.token_length = length;

 l_cleanup:

	return e;
}

static void cst_print_float(duckLisp_t duckLisp, duckLisp_cst_float_t floatingPoint) {
	if (floatingPoint.token_length == 0) {
		puts("{NULL}");
		return;
	}

	for (dl_size_t i = floatingPoint.token_index; i < floatingPoint.token_index + floatingPoint.token_length; i++) {
		putchar(((char *) duckLisp.source.elements)[i]);
	}
}

void ast_float_init(duckLisp_ast_float_t *floatingPoint) {
	floatingPoint->value = 0.0;
}

static void ast_float_quit(duckLisp_t *duckLisp, duckLisp_ast_float_t *floatingPoint) {
	(void) duckLisp;

	floatingPoint->value = 0.0;
}

static dl_error_t ast_generate_float(duckLisp_t *duckLisp,
                                     duckLisp_ast_float_t *floatingPoint,
                                     const duckLisp_cst_float_t floatingPointCST,
                                     dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	e = dl_string_toDouble(&floatingPoint->value,
	                       &((char *) duckLisp->source.elements)[floatingPointCST.token_index],
	                       floatingPointCST.token_length);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp,
		                                   DL_STR("Could not convert token to float."),
		                                   floatingPointCST.token_index,
		                                   throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

static void ast_print_float(duckLisp_t duckLisp, duckLisp_ast_float_t floatingPoint) {
	(void) duckLisp;

	printf("%e", floatingPoint.value);
}


void cst_string_init(duckLisp_cst_string_t *string) {
	string->token_length = 0;
	string->token_index = 0;
}

static void cst_string_quit(duckLisp_t *duckLisp, duckLisp_cst_string_t *string) {
	(void) duckLisp;

	string->token_index = 0;
	string->token_length = 0;
}

static dl_error_t cst_parse_string(duckLisp_t *duckLisp,
                                   duckLisp_cst_compoundExpression_t *compoundExpression,
                                   char *source,
                                   const dl_ptrdiff_t start_index,
                                   const dl_size_t length,
                                   dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	//	dl_bool_t tempBool;

	if (index >= stop_index) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Zero length fragment."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	if (source[index] == '#') {
		index++;

		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected characters after stringify operator."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}

		/*
		  You know what? I'm feeling lazy, and I think I can get away with making
		  everything after the `'` a string.
		*/
	}
	else if (source[index] == '"') {
		index++;

		while (index < stop_index) {
			if (source[index] == '\\') {
				// Eat character.
				index++;

				if (index >= stop_index) {
					eError = duckLisp_error_pushSyntax(duckLisp,
					                                   DL_STR("Expected character in string escape sequence."),
					                                   index,
					                                   throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}
			}
			else if (source[index] == '"') {
				index++;
				break;
			}

			index++;
		}

		if (index != stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp,
			                                   DL_STR("Expected end of fragment after quote."),
			                                   index,
			                                   throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}
	else {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Not a string."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

	// @TODO: Allow stringified strings instead of just quoted strings.
	compoundExpression->value.string.token_index = start_index + 1;
	compoundExpression->value.string.token_length = length - 2;

 l_cleanup:

	return e;
}

static void cst_print_string(duckLisp_t duckLisp, duckLisp_cst_string_t string) {
	if (string.token_length == 0) {
		puts("{NULL}");
		return;
	}

	putchar('"');
	for (dl_size_t i = string.token_index; i < string.token_index + string.token_length; i++) {
		putchar(((char *) duckLisp.source.elements)[i]);
	}
	putchar('"');
}

void ast_string_init(duckLisp_ast_string_t *string) {
	string->value = dl_null;
	string->value_length = 0;
}

static dl_error_t ast_string_quit(duckLisp_t *duckLisp, duckLisp_ast_string_t *string) {
	dl_error_t e = dl_error_ok;

	string->value_length = 0;

	e = dl_free(duckLisp->memoryAllocation, (void **) &string->value);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

static dl_error_t ast_generate_string(duckLisp_t *duckLisp,
                                      duckLisp_ast_string_t *string,
                                      const duckLisp_cst_string_t stringCST,
                                      dl_bool_t throwErrors) {
	(void) throwErrors;
	dl_error_t e = dl_error_ok;

	void *destination = dl_null;
	void *source = dl_null;
	char *s = dl_null;
	dl_bool_t escape = dl_false;

	string->value_length = 0;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &string->value, stringCST.token_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}

	string->value_length = stringCST.token_length;

	destination = string->value;
	source = &((char *) duckLisp->source.elements)[stringCST.token_index];
	s = source;
	for (char *d = destination; s < (char *) source + stringCST.token_length; s++) {
		if (escape) {
			escape = dl_false;
			if (*s == 'n') {
				*d++ = '\n';
				continue;
			}
		}
		else if (*s == '\\') {
			escape = dl_true;
			--string->value_length;
			continue;
		}
		*d++ = *s;
	}

 l_cleanup:

	return e;
}

static void ast_print_string(duckLisp_t duckLisp, duckLisp_ast_string_t string) {
	(void) duckLisp;

	if (string.value_length == 0) {
		puts("{NULL}");
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


void cst_compoundExpression_init(duckLisp_cst_compoundExpression_t *compoundExpression) {
	compoundExpression->type = duckLisp_ast_type_none;
}

dl_error_t cst_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression) {
	dl_error_t e = dl_error_ok;

	switch (compoundExpression->type) {
	case duckLisp_ast_type_float:
		compoundExpression->value.floatingPoint.token_index = -1;
		compoundExpression->value.floatingPoint.token_length = 0;
		break;
	case duckLisp_ast_type_int:
		compoundExpression->value.integer.token_index = -1;
		compoundExpression->value.integer.token_length = 0;
		break;
	case duckLisp_ast_type_bool:
		compoundExpression->value.boolean.token_index = -1;
		compoundExpression->value.boolean.token_length = 0;
		break;
	case duckLisp_ast_type_string:
		/**/ cst_string_quit(duckLisp, &compoundExpression->value.string);
		break;
	case duckLisp_ast_type_identifier:
		/**/ cst_identifier_quit(duckLisp, &compoundExpression->value.identifier);
		break;
	case duckLisp_ast_type_expression:
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < compoundExpression->value.expression.compoundExpressions_length; i++) {
			e = cst_compoundExpression_quit(duckLisp, &compoundExpression->value.expression.compoundExpressions[i]);
			if (e) break;
		}
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	compoundExpression->type = duckLisp_ast_type_none;

	return e;
}

static dl_error_t cst_parse_compoundExpression(duckLisp_t *duckLisp,
                                               duckLisp_cst_compoundExpression_t *compoundExpression,
                                               char *source,
                                               const dl_ptrdiff_t start_index,
                                               const dl_size_t length,
                                               dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t index = start_index;

	cst_compoundExpression_init(compoundExpression);

	typedef struct {
		dl_error_t (*reader) (duckLisp_t *,
		                      duckLisp_cst_compoundExpression_t *,
		                      char *,
		                      const dl_ptrdiff_t start_index,
		                      const dl_size_t,
		                      dl_bool_t);
		duckLisp_ast_type_t type;
	} readerStruct_t;

	readerStruct_t readerStruct[] = {
		{.reader = cst_parse_bool,		 .type = duckLisp_ast_type_bool},
		{.reader = cst_parse_int,		 .type = duckLisp_ast_type_int},
		{.reader = cst_parse_float,				 .type = duckLisp_ast_type_float},
		{.reader = cst_parse_string,	 .type = duckLisp_ast_type_string},
		{.reader = cst_parse_identifier, .type = duckLisp_ast_type_identifier},
		{.reader = cst_parse_expression, .type = duckLisp_ast_type_expression},
	};

	// We have a reader! I'll need to make it generate AST though.
	for (dl_ptrdiff_t i = 0; (unsigned long) i < sizeof(readerStruct)/sizeof(readerStruct_t); i++) {
		e = readerStruct[i].reader(duckLisp, compoundExpression, source, start_index, length, dl_false);
		if (!e) {
			compoundExpression->type = readerStruct[i].type;
			goto l_cleanup;
		}
		if (e != dl_error_invalidValue) {
			goto l_cleanup;
		}
	}
	eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Unrecognized form."), index, throwErrors);
	e = eError ? eError : dl_error_invalidValue;

 l_cleanup:

	return e;
}

static dl_error_t cst_print_compoundExpression(duckLisp_t duckLisp,
                                               duckLisp_cst_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;

	switch (compoundExpression.type) {
	case duckLisp_ast_type_bool:
		/**/ cst_print_bool(duckLisp, compoundExpression.value.boolean);
		break;
	case duckLisp_ast_type_int:
		/**/ cst_print_int(duckLisp, compoundExpression.value.integer);
		break;
	case duckLisp_ast_type_float:
		/**/ cst_print_float(duckLisp, compoundExpression.value.floatingPoint);
		break;
	case duckLisp_ast_type_string:
		/**/ cst_print_string(duckLisp, compoundExpression.value.string);
		break;
	case duckLisp_ast_type_identifier:
		/**/ cst_print_identifier(duckLisp, compoundExpression.value.identifier);
		break;
	case duckLisp_ast_type_expression:
		e = cst_print_expression(duckLisp, compoundExpression.value.expression);
		break;
	default:
		printf("Compound expression: Type %u\n", compoundExpression.type);
		e = dl_error_shouldntHappen;
	}

	return e;
}

void ast_compoundExpression_init(duckLisp_ast_compoundExpression_t *compoundExpression) {
	compoundExpression->type = duckLisp_ast_type_none;
}

dl_error_t ast_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression) {
	dl_error_t e = dl_error_ok;

	switch (compoundExpression->type) {
	case duckLisp_ast_type_string:
		/**/ ast_string_quit(duckLisp, &compoundExpression->value.string);
		break;
	case duckLisp_ast_type_bool:
		/**/ ast_bool_quit(duckLisp, &compoundExpression->value.boolean);
		break;
	case duckLisp_ast_type_int:
		/**/ ast_int_quit(duckLisp, &compoundExpression->value.integer);
		break;
	case duckLisp_ast_type_float:
		/**/ ast_float_quit(duckLisp, &compoundExpression->value.floatingPoint);
		break;
	case duckLisp_ast_type_identifier:
		e = ast_identifier_quit(duckLisp, &compoundExpression->value.identifier);
		break;
	case duckLisp_ast_type_expression:
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < compoundExpression->value.expression.compoundExpressions_length; i++) {
			e = ast_compoundExpression_quit(duckLisp, &compoundExpression->value.expression.compoundExpressions[i]);
			if (e) break;
		}
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	compoundExpression->type = duckLisp_ast_type_none;

	return e;
}

static dl_error_t ast_generate_compoundExpression(duckLisp_t *duckLisp,
                                                  duckLisp_ast_compoundExpression_t *compoundExpression,
                                                  duckLisp_cst_compoundExpression_t compoundExpressionCST,
                                                  dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;

	switch (compoundExpressionCST.type) {
	case duckLisp_ast_type_bool:
		compoundExpression->type = duckLisp_ast_type_bool;
		e = ast_generate_bool(duckLisp,
		                      &compoundExpression->value.boolean,
		                      compoundExpressionCST.value.boolean,
		                      throwErrors);
		break;
	case duckLisp_ast_type_int:
		compoundExpression->type = duckLisp_ast_type_int;
		e = ast_generate_int(duckLisp,
		                     &compoundExpression->value.integer,
		                     compoundExpressionCST.value.integer,
		                     throwErrors);
		break;
	case duckLisp_ast_type_float:
		compoundExpression->type = duckLisp_ast_type_float;
		e = ast_generate_float(duckLisp,
		                       &compoundExpression->value.floatingPoint,
		                       compoundExpressionCST.value.floatingPoint,
		                       throwErrors);
		break;
	case duckLisp_ast_type_string:
		compoundExpression->type = duckLisp_ast_type_string;
		e = ast_generate_string(duckLisp,
		                        &compoundExpression->value.string,
		                        compoundExpressionCST.value.string,
		                        throwErrors);
		break;
	case duckLisp_ast_type_identifier:
		compoundExpression->type = duckLisp_ast_type_identifier;
		e = ast_generate_identifier(duckLisp,
		                            &compoundExpression->value.identifier,
		                            compoundExpressionCST.value.identifier,
		                            throwErrors);
		break;
	case duckLisp_ast_type_expression:
		compoundExpression->type = duckLisp_ast_type_expression;
		// // This declares `()` == `0`
		if (compoundExpressionCST.value.expression.compoundExpressions_length == 0) {
			/* compoundExpression->type = duckLisp_ast_type_int; */
			/* compoundExpression->value.integer.value = 0; */
			compoundExpression->value.expression.compoundExpressions = dl_null;
			compoundExpression->value.expression.compoundExpressions_length = 0;
		}
		else e = ast_generate_expression(duckLisp, &compoundExpression->value.expression,
		                                 compoundExpressionCST.value.expression, throwErrors);
		break;
	default:
		compoundExpression->type = duckLisp_ast_type_none;
		e = dl_error_shouldntHappen;
	}

	//	l_cleanup:

	return e;
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
	case duckLisp_ast_type_identifier:
		/**/ ast_print_identifier(duckLisp, compoundExpression.value.identifier);
		break;
	case duckLisp_ast_type_expression:
		e = ast_print_expression(duckLisp, compoundExpression.value.expression);
		break;
	default:
		printf("Compound expression: Type %u\n", compoundExpression.type);
		e = dl_error_shouldntHappen;
	}

	return e;
}


dl_error_t duckLisp_cst_append(duckLisp_t *duckLisp,
                               duckLisp_cst_compoundExpression_t *cst,
                               const dl_ptrdiff_t index,
                               dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	char *source = duckLisp->source.elements;
	dl_size_t source_length = duckLisp->source.elements_length;
	dl_bool_t isSpace;

	// e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->cst.compoundExpressions,
	//				  (duckLisp->cst.compoundExpressions_length + 1) * sizeof(duckLisp_cst_compoundExpression_t));
	// if (e) {
	//		goto l_cleanup;
	// }
	// duckLisp->cst.compoundExpressions_length++;

	// e = cst_parse_compoundExpression(duckLisp, &duckLisp->cst.compoundExpressions[duckLisp->cst.compoundExpressions_length - 1], source, index,
	//									source_length - index, throwErrors);

	// Trim whitespace off the end.
	while (source_length > 0) {
		/**/ dl_string_isSpace(&isSpace, source[source_length - 1]);
		if (isSpace) --source_length; else break;
	}

	e = cst_parse_compoundExpression(duckLisp, cst, source, index, source_length - index, throwErrors);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Error parsing expression."), 0, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_ast_append(duckLisp_t *duckLisp,
                               duckLisp_ast_compoundExpression_t *ast,
                               duckLisp_cst_compoundExpression_t *cst,
                               const dl_ptrdiff_t index,
                               dl_bool_t throwErrors) {
	(void) index;
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	e = ast_generate_compoundExpression(duckLisp, ast, *cst, throwErrors);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Error converting CST to AST."), 0, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
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
	/**/ dl_trie_init(&scope->statics_trie, duckLisp->memoryAllocation, -1);
	/**/ dl_trie_init(&scope->generators_trie, duckLisp->memoryAllocation, -1);
	scope->generators_length = 0;
	/**/ dl_trie_init(&scope->functions_trie, duckLisp->memoryAllocation, -1);
	scope->functions_length = 0;
	/**/ dl_trie_init(&scope->labels_trie, duckLisp->memoryAllocation, -1);
	scope->function_scope = is_function;
	scope->scope_uvs = dl_null;
	scope->scope_uvs_length = 0;
	scope->function_uvs = dl_null;
	scope->function_uvs_length = 0;
}

dl_error_t duckLisp_pushScope(duckLisp_t *duckLisp, duckLisp_scope_t *scope, dl_bool_t is_function) {
	if (scope == dl_null) {
		duckLisp_scope_t localScope;
		/**/ scope_init(duckLisp, &localScope, is_function);
		return dl_array_pushElement(&duckLisp->scope_stack, &localScope);
	}
	else {
		return dl_array_pushElement(&duckLisp->scope_stack, scope);
	}
}

dl_error_t duckLisp_popScope(duckLisp_t *duckLisp, duckLisp_scope_t *scope) {
	return dl_array_popElement(&duckLisp->scope_stack, scope);
}

static dl_error_t scope_getTop(duckLisp_t *duckLisp, duckLisp_scope_t *scope) {
	dl_error_t e = dl_error_ok;

	e = dl_array_getTop(&duckLisp->scope_stack, scope);
	if (e == dl_error_bufferUnderflow) {
		// Push a scope if we don't have one yet.
		/**/ scope_init(duckLisp, scope, dl_true);
		e = dl_array_pushElement(&duckLisp->scope_stack, scope);
		if (e) {
			goto l_cleanup;
		}
	}

 l_cleanup:

	return e;
}

static dl_error_t scope_setTop(duckLisp_t *duckLisp, duckLisp_scope_t *scope) {
	return dl_array_set(&duckLisp->scope_stack, scope, duckLisp->scope_stack.elements_length - 1);
}

/*
  Failure if return value is set or index is -1.
  "Local" is defined as remaining inside the current function.
*/
dl_error_t duckLisp_scope_getLocalIndexFromName(duckLisp_t *duckLisp,
                                                dl_ptrdiff_t *index,
                                                const char *name,
                                                const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = duckLisp->scope_stack.elements_length;

	*index = -1;

	do {
		e = dl_array_get(&duckLisp->scope_stack, (void *) &scope, --(scope_index));
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

dl_error_t duckLisp_scope_getFreeLocalIndexFromName(duckLisp_t *duckLisp,
                                                    dl_ptrdiff_t *index,
                                                    dl_ptrdiff_t *scope_index,
                                                    const char *name,
                                                    const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t function_scope;
	dl_ptrdiff_t function_scope_index = duckLisp->scope_stack.elements_length;

	*index = -1;

	/* Skip the current function. */
	do {
		e = dl_array_get(&duckLisp->scope_stack, (void *) &function_scope, --function_scope_index);
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
			}
			break;
		}
	} while (!function_scope.function_scope);

	duckLisp_scope_t scope;
	*scope_index = function_scope_index;
	while (dl_true) {
		e = dl_array_get(&duckLisp->scope_stack, (void *) &scope, --(*scope_index));
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
	}

	if (*index != -1) {
		/* We found it, which means it's an upvalue. Check to make sure it has been registered. */
		dl_bool_t found_upvalue = dl_false;
		DL_DOTIMES(i, function_scope.function_uvs_length) {
			if (function_scope.function_uvs[i] == *index) {
				found_upvalue = dl_true;
				break;
			}
		}
		if (!found_upvalue) {
			/* Not registered. Register. */
			e = dl_realloc(duckLisp->memoryAllocation,
			               (void **) &function_scope.function_uvs,
			               (function_scope.function_uvs_length + 1) * sizeof(dl_ptrdiff_t));
			if (e) goto cleanup;
			function_scope.function_uvs_length++;
			function_scope.function_uvs[function_scope.function_uvs_length - 1] = *index;
			e = dl_array_set(&duckLisp->scope_stack, (void *) &function_scope, function_scope_index);
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
			e = dl_realloc(duckLisp->memoryAllocation,
			               (void **) &scope.scope_uvs,
			               (scope.scope_uvs_length + 1) * sizeof(dl_ptrdiff_t));
			if (e) goto cleanup;
			scope.scope_uvs_length++;
			scope.scope_uvs[scope.scope_uvs_length - 1] = *index;
			e = dl_array_set(&duckLisp->scope_stack, (void *) &scope, *scope_index);
			if (e) goto cleanup;
		}
		*index = function_scope.function_uvs_length - 1;
	}

 cleanup:
	return e;
}

dl_error_t duckLisp_scope_getStaticIndexFromName(duckLisp_t *duckLisp,
                                                 dl_ptrdiff_t *index,
                                                 const char *name,
                                                 const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = duckLisp->scope_stack.elements_length;

	*index = -1;

	while (dl_true) {
		e = dl_array_get(&duckLisp->scope_stack, (void *) &scope, --scope_index);
		if (e) {
			if (e == dl_error_invalidValue) {
				e = dl_error_ok;
			}
			break;
		}

		/**/ dl_trie_find(scope.statics_trie, index, name, name_length);
		if (*index != -1) {
			break;
		}
	}

	return e;
}

static dl_error_t scope_getFunctionFromName(duckLisp_t *duckLisp,
                                            duckLisp_functionType_t *functionType,
                                            dl_ptrdiff_t *index,
                                            const char *name,
                                            const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = duckLisp->scope_stack.elements_length;
	dl_ptrdiff_t tempPtrdiff = -1;
	*index = -1;
	*functionType = duckLisp_functionType_none;

	while (dl_true) {
		e = dl_array_get(&duckLisp->scope_stack, (void *) &scope, --scope_index);
		if (e) {
			if (e == dl_error_invalidValue) {
				// We've gone though all the scopes.
				e = dl_error_ok;
			}
			break;
		}

		/**/ dl_trie_find(scope.functions_trie, &tempPtrdiff, name, name_length);
		if (tempPtrdiff != duckLisp_functionType_generator) {
			/**/ dl_trie_find(scope.statics_trie, index, name, name_length);
		}
		else {
			/**/ dl_trie_find(scope.generators_trie, index, name, name_length);
		}
		// Return the function in the nearest scope.
		if (tempPtrdiff != -1) {
			break;
		}
	}

	if (tempPtrdiff == -1) {
		*functionType = duckLisp_functionType_none;
	}
	else {
		*functionType = tempPtrdiff;
	}

	return e;
}

static dl_error_t scope_getLabelFromName(duckLisp_t *duckLisp,
                                         dl_ptrdiff_t *index,
                                         const char *name,
                                         dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = duckLisp->scope_stack.elements_length;

	*index = -1;

	while (dl_true) {
		e = dl_array_get(&duckLisp->scope_stack, (void *) &scope, --scope_index);
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

/*
  ========
  Emitters
  ========
*/

dl_error_t duckLisp_emit_nil(duckLisp_t *duckLisp, dl_array_t *assembly) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_nil;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_typeof(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_typeof;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_nullp(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_nullp;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_car(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_car;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_cdr(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_cdr;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_cons(duckLisp_t *duckLisp,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_cons;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_return(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_size_t count) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_return;

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

	duckLisp->locals_length -= count;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pop(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_size_t count) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

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

	duckLisp->locals_length -= count;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_greater(duckLisp_t *duckLisp,
                                 dl_array_t *assembly,
                                 const dl_ptrdiff_t source_index1,
                                 const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_greater;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_equal(duckLisp_t *duckLisp,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t source_index1,
                               const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_equal;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_less(duckLisp_t *duckLisp,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_less;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_not(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_not;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_multiply(duckLisp_t *duckLisp,
                                  dl_array_t *assembly,
                                  const dl_ptrdiff_t source_index1,
                                  const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_mul;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_divide(duckLisp_t *duckLisp,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index1,
                                const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_div;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_add(duckLisp_t *duckLisp,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_add;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_sub(duckLisp_t *duckLisp,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_sub;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index2;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_nop(duckLisp_t *duckLisp, dl_array_t *assembly) {

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_nop;

	// Push arguments into instruction.

	// Push instruction into list.
	return dl_array_pushElement(assembly, &instruction);
}

dl_error_t duckLisp_emit_move(duckLisp_t *duckLisp,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t destination_index,
                              const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_move;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - source_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - destination_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushLabel(duckLisp_t *duckLisp,
                                   dl_array_t *assembly,
                                   dl_ptrdiff_t *stackIndex,
                                   const dl_ptrdiff_t label_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushLabel;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushBoolean(duckLisp_t *duckLisp,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushBoolean;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = integer;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushInteger;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = integer;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushString(duckLisp_t *duckLisp,
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

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushString;

	// Write string length.

	if (string_length > DL_UINT16_MAX) {
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    DL_STR("String longer than DL_UINT_MAX. Truncating string to fit."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		string_length = DL_UINT16_MAX;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_string;
	argument.value.string.value = string;
	argument.value.string.value_length = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushSymbol(duckLisp_t *duckLisp,
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
			goto l_cleanup;
		}
		string_length = DL_UINT16_MAX;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = id;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_string;
	argument.value.string.value = string;
	argument.value.string.value_length = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushClosure(duckLisp_t *duckLisp,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t function_label_index,
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
	instruction.instructionClass = duckLisp_instructionClass_pushClosure;

	// Push arguments into instruction.

	// Function label
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = function_label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Captures
	argument.type = duckLisp_instructionArgClass_type_integer;
	DL_DOTIMES(i, captures_length) {
		// This is the absolute index, not relative to the top of the stack.
		argument.value.integer = captures[i];
		e = dl_array_pushElement(&instruction.args, &argument);
		if (e) goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	/* duckLisp->locals_length++; */

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_ccall(duckLisp_t *duckLisp, dl_array_t *assembly, dl_ptrdiff_t callback_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_ccall;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = callback_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp,
                                   dl_array_t *assembly,
                                   dl_ptrdiff_t *stackIndex,
                                   const dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushIndex;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp->locals_length - index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_pushUpvalue(duckLisp_t *duckLisp,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushUpvalue;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	duckLisp->locals_length++;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_acall(duckLisp_t *duckLisp,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t function_index,
                               const dl_size_t count) {
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
	instruction.instructionClass = duckLisp_instructionClass_acall;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = duckLisp->locals_length - function_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

// We do label scoping in the emitters because scope will have no meaning during assembly.

dl_error_t duckLisp_emit_call(duckLisp_t *duckLisp,
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

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_call;

	// `label_index` should never equal -1 after this function exits.
	e = scope_getLabelFromName(duckLisp, &label_index, label, label_length);
	if (e) {
		goto l_cleanup;
	}

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Goto references undeclared label \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_emit_brz(duckLisp_t *duckLisp,
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

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_brz;

	if (pops < 0) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("brz: Cannot pop a negative number of objects."));
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// `label_index` should never equal -1 after this function exits.
	e = scope_getLabelFromName(duckLisp, &label_index, label, label_length);
	if (e) {
		goto l_cleanup;
	}

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("brz references undeclared label \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = pops;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length -= pops;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_emit_brnz(duckLisp_t *duckLisp,
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

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_brnz;

	if (pops < 0) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("brnz: Cannot pop a negative number of objects."));
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// `label_index` should never equal -1 after this function exits.
	e = scope_getLabelFromName(duckLisp, &label_index, label, label_length);
	if (e) {
		goto l_cleanup;
	}

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("brnz references undeclared label \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = pops;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length -= pops;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_emit_jump(duckLisp_t *duckLisp, dl_array_t *assembly, char *label, dl_size_t label_length) {
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

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_jump;

	// `label_index` should never equal -1 after this function exits.
	e = scope_getLabelFromName(duckLisp, &label_index, label, label_length);
	if (e) {
		goto l_cleanup;
	}

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Goto references undeclared label \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_emit_label(duckLisp_t *duckLisp,
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

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pseudo_label;

	// This is why we pushed the scope here.
	e = scope_getTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

	// Make sure label is declared.
	/**/ dl_trie_find(scope.labels_trie, &label_index, label, label_length);
	if (e) {
		goto l_cleanup;
	}
	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Label \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\" is not a top-level expression in a closed scope."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

/*
  ==========
  Generators
  ==========
*/

/* `gensym` creates a label that is unlikely to ever be used. */
dl_error_t duckLisp_gensym(duckLisp_t *duckLisp, duckLisp_ast_identifier_t *identifier) {
	dl_error_t e = dl_error_ok;

	identifier->value_length = 1 + 8/4*sizeof(dl_size_t);  // This is dependent on the size of the gensym number.
	e = dl_malloc(duckLisp->memoryAllocation, (void **) &identifier->value, identifier->value_length);
	if (e) {
		identifier->value_length = 0;
		return dl_error_outOfMemory;}
	identifier->value[0] = '\0';  // Surely not even an idiot would start a string with a null char.
	for (dl_ptrdiff_t i = 1; (dl_size_t) i <= 8/4*sizeof(dl_size_t); i++) {
		identifier->value[i] = dl_nybbleToHexChar((duckLisp->gensym_number >> 4*i) & 0xF);
	}
	duckLisp->gensym_number++;
	return e;
}

dl_error_t duckLisp_register_label(duckLisp_t *duckLisp, char *name, const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	dl_ptrdiff_t label_index = -1;
	duckLisp_scope_t scope;

	duckLisp_label_t label;

	e = scope_getTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

	// declare the label.
	label.name = name;
	label.name_length = name_length;

	/**/ dl_array_init(&label.sources,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_label_source_t),
	                   dl_array_strategy_double);
	label.target = -1;
	e = dl_array_pushElement(&duckLisp->labels, &label);
	if (e) {
		goto l_cleanup;
	}
	label_index = duckLisp->labels.elements_length - 1;
	e = dl_trie_insert(&scope.labels_trie, label.name, label.name_length, label_index);
	if (e) {
		goto l_cleanup;
	}

	e = scope_setTop(duckLisp, &scope);
	if (e) goto l_cleanup;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_generator_typeof(duckLisp_t *duckLisp,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &args_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto l_cleanup;

	e = duckLisp_emit_typeof(duckLisp, assembly, args_index);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_nullp(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &args_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto l_cleanup;

	e = duckLisp_emit_nullp(duckLisp, assembly, args_index);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_car(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &args_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto l_cleanup;

	e = duckLisp_emit_car(duckLisp, assembly, args_index);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_cdr(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &args_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto l_cleanup;

	e = duckLisp_emit_cdr(duckLisp, assembly, args_index);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_cons(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2];

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < expression->compoundExpressions_length - 1; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

	e = duckLisp_emit_cons(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_list(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;

	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        &args_index,
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_quote_helper(duckLisp_t *duckLisp,
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
		e = duckLisp_emit_pushBoolean(duckLisp, assembly, &temp_index, tree->value.boolean.value);
		if (e) goto l_cleanup;
		break;
	case duckLisp_ast_type_int:
		e = duckLisp_emit_pushInteger(duckLisp, assembly, &temp_index, tree->value.integer.value);
		if (e) goto l_cleanup;
		break;
		/* case duckLisp_ast_type_float: */
		/*		e = duckLisp_emit_pushFloat(duckLisp, assembly, &temp_index, tree->value.floatingPoint.value); */
		/*		if (e) goto l_cleanup; */
		/*		temp_type = duckLisp_ast_type_float; */
		/*		break; */
	case duckLisp_ast_type_string:
		e = duckLisp_emit_pushString(duckLisp,
		                             assembly,
		                             stack_index,
		                             tree->value.string.value,
		                             tree->value.string.value_length);
		if (e) goto l_cleanup;
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
			if (e) goto l_cleanup;
			tempIdentifier.value_length = tree->value.identifier.value_length;
			e = dl_malloc(duckLisp->memoryAllocation, (void **) &tempIdentifier.value, tempIdentifier.value_length);
			if (e) goto l_cleanup;
			/**/ dl_memcopy_noOverlap(tempIdentifier.value, tree->value.identifier.value, tempIdentifier.value_length);
			e = dl_array_pushElement(&duckLisp->symbols_array, (void *) &tempIdentifier);
			if (e) goto l_cleanup;
		}
		// Push symbol
		e = duckLisp_emit_pushSymbol(duckLisp,
		                             assembly,
		                             stack_index,
		                             temp_index,
		                             tree->value.identifier.value,
		                             tree->value.identifier.value_length);
		if (e) goto l_cleanup;
		break;
	case duckLisp_ast_type_expression:
		if (tree->value.expression.compoundExpressions_length > 0) {
			dl_ptrdiff_t last_temp_index;
			e = duckLisp_emit_nil(duckLisp, assembly);
			if (e) goto l_cleanup;
			last_temp_index = duckLisp->locals_length - 1;
			for (dl_ptrdiff_t j = tree->value.expression.compoundExpressions_length - 1; j >= 0; --j) {
				e = duckLisp_generator_quote_helper(duckLisp,
				                                    assembly,
				                                    &temp_index,
				                                    &tree->value.expression.compoundExpressions[j]);
				if (e) goto l_cleanup;
				e = duckLisp_emit_cons(duckLisp, assembly, duckLisp->locals_length - 1, last_temp_index);
				if (e) goto l_cleanup;
				last_temp_index = duckLisp->locals_length - 1;
			}
			*stack_index = duckLisp->locals_length - 1;
		}
		else {
			e = duckLisp_emit_nil(duckLisp, assembly);
			if (e) goto l_cleanup;
			*stack_index = duckLisp->locals_length - 1;
		}
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("quote"));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&tempString);
	if (eError) {
		e = eError;
	}

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_quote(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
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

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	switch (tree->type) {
	case duckLisp_ast_type_bool:
		e = duckLisp_emit_pushBoolean(duckLisp, assembly, &temp_index, tree->value.boolean.value);
		if (e) goto l_cleanup;
		break;
	case duckLisp_ast_type_int:
		e = duckLisp_emit_pushInteger(duckLisp, assembly, &temp_index, tree->value.integer.value);
		if (e) goto l_cleanup;
		break;
		/* case duckLisp_ast_type_float: */
		/*		e = duckLisp_emit_pushFloat(duckLisp, assembly, &temp_index, tree->value.floatingPoint.value); */
		/*		if (e) goto l_cleanup; */
		/*		temp_type = duckLisp_ast_type_float; */
		/*		break; */
	case duckLisp_ast_type_string:
		e = duckLisp_emit_pushString(duckLisp,
		                             assembly,
		                             &temp_index,
		                             tree->value.string.value,
		                             tree->value.string.value_length);
		if (e) goto l_cleanup;
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
			if (e) goto l_cleanup;
			tempIdentifier.value_length = tree->value.identifier.value_length;
			e = dl_malloc(duckLisp->memoryAllocation, (void **) &tempIdentifier.value, tempIdentifier.value_length);
			if (e) goto l_cleanup;
			/**/ dl_memcopy_noOverlap(tempIdentifier.value, tree->value.identifier.value, tempIdentifier.value_length);
			e = dl_array_pushElement(&duckLisp->symbols_array, (void *) &tempIdentifier);
			if (e) goto l_cleanup;
		}
		// Push symbol
		e = duckLisp_emit_pushSymbol(duckLisp,
		                             assembly,
		                             dl_null,
		                             temp_index,
		                             tree->value.identifier.value,
		                             tree->value.identifier.value_length);
		if (e) goto l_cleanup;
		break;
	case duckLisp_ast_type_expression:
		if (tree->value.expression.compoundExpressions_length > 0) {
			dl_ptrdiff_t last_temp_index;
			e = duckLisp_emit_nil(duckLisp, assembly);
			if (e) goto l_cleanup;
			last_temp_index = duckLisp->locals_length - 1;
			for (dl_ptrdiff_t j = tree->value.expression.compoundExpressions_length - 1; j >= 0; --j) {
				e = duckLisp_generator_quote_helper(duckLisp,
				                                    assembly,
				                                    &temp_index,
				                                    &tree->value.expression.compoundExpressions[j]);
				if (e) goto l_cleanup;
				e = duckLisp_emit_cons(duckLisp, assembly, duckLisp->locals_length - 1, last_temp_index);
				if (e) goto l_cleanup;
				last_temp_index = duckLisp->locals_length - 1;
			}
		}
		else {
			e = duckLisp_emit_nil(duckLisp, assembly);
			if (e) goto l_cleanup;
		}
		break;
	default:
		e = dl_array_pushElements(&eString, functionName, functionName_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&tempString);
	if (eError) {
		e = eError;
	}

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_noscope(duckLisp_t *duckLisp,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_size_t startStack_length;
	dl_ptrdiff_t identifier_index = -1;
	dl_bool_t noPop = dl_false;

	// Arguments

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		duckLisp_ast_compoundExpression_t currentExpression = expression->compoundExpressions[i];
		switch (currentExpression.type) {
		case duckLisp_ast_type_none:
			break;
		case duckLisp_ast_type_expression:
			startStack_length = duckLisp->locals_length;
			if ((currentExpression.value.expression.compoundExpressions_length > 0) &&
			    (currentExpression.value.expression.compoundExpressions[0].type == duckLisp_ast_type_identifier)) {
				dl_string_compare(&noPop,
				                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
				                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length,
				                  DL_STR("var"));
			}
			e = duckLisp_compile_expression(duckLisp,
			                                assembly,
			                                expression->compoundExpressions[0].value.identifier.value,
			                                expression->compoundExpressions[0].value.identifier.value_length,
			                                &currentExpression.value.expression);
			if (e) goto l_cleanup;
			if (!noPop
			    && (duckLisp->locals_length
			        > (startStack_length + ((dl_size_t) i == expression->compoundExpressions_length - 1)))) {
				if ((dl_size_t) i == expression->compoundExpressions_length - 1) {
					e = duckLisp_emit_move(duckLisp, assembly, startStack_length, duckLisp->locals_length - 1);
				}
				e = duckLisp_emit_pop(duckLisp,
				                      assembly,
				                      duckLisp->locals_length
				                      - startStack_length
				                      - ((dl_size_t) i == expression->compoundExpressions_length - 1));
				noPop = dl_true;
			}
			break;
		case duckLisp_ast_type_identifier:
			e = duckLisp_scope_getLocalIndexFromName(duckLisp, &identifier_index,
			                                         currentExpression.value.identifier.value,
			                                         currentExpression.value.identifier.value_length);
			if (e) goto l_cleanup;
			if (identifier_index == -1) {
				e = dl_array_pushElements(&eString, DL_STR("expression: Could not find local \""));
				if (e) {
					goto l_cleanup;
				}
				e = dl_array_pushElements(&eString, currentExpression.value.identifier.value,
				                          currentExpression.value.identifier.value_length);
				if (e) {
					goto l_cleanup;
				}
				e = dl_array_pushElements(&eString, DL_STR("\"."));
				if (e) {
					goto l_cleanup;
				}
				eError = duckLisp_error_pushRuntime(duckLisp,
				                                    eString.elements,
				                                    eString.elements_length * eString.element_size);
				if (eError) {
					e = eError;
				}
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			// We are NOT pushing an index since the index is part of the instruction.
			e = duckLisp_emit_pushIndex(duckLisp, assembly, dl_null, identifier_index);
			if (e) {
				goto l_cleanup;
				/* } */
			}
			break;
		case duckLisp_ast_type_string:
			e = duckLisp_emit_pushString(duckLisp,
			                             assembly,
			                             dl_null,
			                             currentExpression.value.string.value,
			                             currentExpression.value.string.value_length);
			if (e) goto l_cleanup;
			break;
		case duckLisp_ast_type_float:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Float constants are not yet supported."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
			break;
		case duckLisp_ast_type_int:
			e = duckLisp_emit_pushInteger(duckLisp, assembly, dl_null, currentExpression.value.integer.value);
			if (e) goto l_cleanup;
			break;
		case duckLisp_ast_type_bool:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Boolean constants are not yet supported."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
			break;
		}
	}

	// Footer

	duckLisp->locals_length = startStack_length;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_consToAST(duckLisp_t *duckLisp,
                              duckLisp_ast_compoundExpression_t *ast,
                              duckVM_gclist_cons_t *cons) {
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
		              sizeof("cons") - 1);
		if (e) goto cleanup;
		/**/ dl_memcopy_noOverlap(ast->value.expression.compoundExpressions[op].value.identifier.value, DL_STR("cons"));
		ast->value.expression.compoundExpressions[op].value.identifier.value_length = sizeof("cons") - 1;
		ast->value.expression.compoundExpressions[op].type = duckLisp_ast_type_identifier;

		switch (cons->type) {
		case duckVM_gclist_cons_type_addrAddr:
			puts("addr-addr");
			duckLisp_consToAST(duckLisp, &ast->value.expression.compoundExpressions[car], cons->car.addr);
			duckLisp_consToAST(duckLisp, &ast->value.expression.compoundExpressions[cdr], cons->cdr.addr);
			break;
		case duckVM_gclist_cons_type_addrObject:
			puts("addr-object");
			duckLisp_consToAST(duckLisp, &ast->value.expression.compoundExpressions[car], cons->car.addr);
			duckLisp_objectToAST(duckLisp, &ast->value.expression.compoundExpressions[cdr], cons->cdr.data);
			break;
		case duckVM_gclist_cons_type_objectAddr:
			puts("object-addr");
			duckLisp_objectToAST(duckLisp, &ast->value.expression.compoundExpressions[car], cons->car.data);
			duckLisp_consToAST(duckLisp, &ast->value.expression.compoundExpressions[cdr], cons->cdr.addr);
			break;
		case duckVM_gclist_cons_type_objectObject:
			puts("object-object");
			duckLisp_objectToAST(duckLisp, &ast->value.expression.compoundExpressions[car], cons->car.data);
			duckLisp_objectToAST(duckLisp, &ast->value.expression.compoundExpressions[cdr], cons->cdr.data);
			break;
		default:
			e = dl_error_invalidValue;
		}
	}
	else {
		puts("null");
		ast->value.expression.compoundExpressions = dl_null;
		ast->value.expression.compoundExpressions_length = 0;
		ast->type = duckLisp_ast_type_expression;
	}

 cleanup:

	return e;
}

dl_error_t duckLisp_objectToAST(duckLisp_t *duckLisp,
                                duckLisp_ast_compoundExpression_t *ast,
                                duckLisp_object_t *object) {
	dl_error_t e = dl_error_ok;

	switch (object->type) {
	case duckLisp_object_type_bool:
		puts("boolean");
		ast->value.boolean.value = object->value.boolean;
		ast->type = duckLisp_ast_type_bool;
		break;
	case duckLisp_object_type_integer:
		printf("integer %lli\n", object->value.integer);
		ast->value.integer.value = object->value.integer;
		ast->type = duckLisp_ast_type_int;
		break;
	case duckLisp_object_type_float:
		puts("float");
		ast->value.floatingPoint.value = object->value.floatingPoint;
		ast->type = duckLisp_ast_type_float;
		break;
	case duckLisp_object_type_string:
		puts("string");
		// @TODO: This (and case symbol below) is a problem since it will never be freed.
		ast->value.string.value_length = object->value.string.value_length;
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &ast->value.string.value,
		              object->value.string.value_length);
		if (e) break;
		/**/ dl_memcopy_noOverlap(ast->value.string.value,
		                          object->value.string.value,
		                          object->value.string.value_length);
		ast->type = duckLisp_ast_type_string;
		break;
	case duckLisp_object_type_symbol:
		puts("symbol");
		ast->value.identifier.value_length = object->value.symbol.value_length;
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &ast->value.identifier.value,
		              object->value.symbol.value_length);
		if (e) break;
		/**/ dl_memcopy_noOverlap(ast->value.identifier.value,
		                          object->value.symbol.value,
		                          object->value.symbol.value_length);
		ast->type = duckLisp_ast_type_identifier;
		break;
	case duckLisp_object_type_function:
		puts("function");
		e = dl_error_invalidValue;
		break;
	case duckLisp_object_type_list:
		puts("list");
		e = duckLisp_consToAST(duckLisp, ast, object->value.list);
		break;
	default:
		e = dl_error_invalidValue;
	}

	return e;
}

dl_error_t duckLisp_generator_constexpr(duckLisp_t *duckLisp,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_t subCompiler;
	duckVM_t subVM;
	dl_size_t tempDlSize;
	dl_array_t bytecodeArray;
	duckLisp_object_t object;
	duckLisp_ast_compoundExpression_t ast;

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) goto l_cleanup;

	/* Compile */

	subCompiler.memoryAllocation = duckLisp->memoryAllocation;

	e = duckLisp_init(&subCompiler);
	if (e) goto l_cleanup;

	/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
	printf("constexpr: Pre compilation memory usage: %llu/%llu (%llu%%)\n",
	       tempDlSize,
	       duckLisp->memoryAllocation->size,
	       100*tempDlSize/duckLisp->memoryAllocation->size);

	e = duckLisp_compileAST(&subCompiler, &bytecodeArray, expression->compoundExpressions[1]);
	if (e) goto l_cleanupDL;

	/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
	printf("constexpr: Post compilation/Pre execution memory usage: %llu/%llu (%llu%%)\n",
	       tempDlSize,
	       duckLisp->memoryAllocation->size,
	       100*tempDlSize/duckLisp->memoryAllocation->size);

	/* Execute */

	subVM.memoryAllocation = duckLisp->memoryAllocation;

	// Shouldn't need too much.
	e = duckVM_init(&subVM, 1000, 1000);
	if (e) goto l_cleanupDL;

	e = duckVM_execute(&subVM, bytecodeArray.elements);
	if (e) goto l_cleanupVM;
	object = DL_ARRAY_GETTOPADDRESS(subVM.stack, duckLisp_object_t);

	/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
	printf("constexpr: Post execution memory usage: %llu/%llu (%llu%%)\n",
	       tempDlSize,
	       duckLisp->memoryAllocation->size,
	       100*tempDlSize/duckLisp->memoryAllocation->size);

	// Compile result.

	e = duckLisp_objectToAST(duckLisp, &ast, &object);
	if (e) goto l_cleanupVM;

	e = ast_print_compoundExpression(*duckLisp, ast);
	if (e) goto l_cleanupVM;
	putchar('\n');

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &ast,
	                                        dl_null,
	                                        dl_null,
	                                        dl_false);
	if (e) goto l_cleanupVM;

	puts("done");

 l_cleanupVM:
	/**/ duckVM_quit(&subVM);

 l_cleanupDL:

	eError = dl_array_pushElements(&duckLisp->errors,
	                               subCompiler.errors.elements,
	                               subCompiler.errors.elements_length);
	if (eError) e = eError;

	/**/ duckLisp_quit(&subCompiler);

 l_cleanup:
	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_defun(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_ast_identifier_t gensym = {0};
	dl_size_t startStack_length = 0;

	if (expression->compoundExpressions_length < 3) {
		e = dl_array_pushElements(&eString, DL_STR("Too few arguments for function \""));
		if (e) {
			goto l_cleanup;
		}

		e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}

		e = dl_array_pushElements(&eString, DL_STR("\"."));
		if (e) {
			goto l_cleanup;
		}

		e = duckLisp_error_pushRuntime(duckLisp,
		                               (char *) eString.elements,
		                               eString.elements_length * eString.element_size);
		if (e) {
			goto l_cleanup;
		}

		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = duckLisp_error_pushRuntime(duckLisp, DL_STR("defun: Name field must be an identifier."));
		if (e) goto l_cleanup;
		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	if ((expression->compoundExpressions[2].type != duckLisp_ast_type_expression)
	    && ((expression->compoundExpressions[2].type == duckLisp_ast_type_int)
	        && (expression->compoundExpressions[2].value.integer.value != 0))) {

		e = duckLisp_error_pushRuntime(duckLisp, DL_STR("defun: Args field must be a list."));
		if (e) {
			goto l_cleanup;
		}

		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	// Register function.

	e = duckLisp_addInterpretedFunction(duckLisp, expression->compoundExpressions[1].value.identifier);
	if (e) goto l_cleanup;

	// Header.

	e = duckLisp_register_label(duckLisp,
	                            expression->compoundExpressions[1].value.identifier.value,
	                            expression->compoundExpressions[1].value.identifier.value_length);
	if (e) goto l_cleanup;

	e = duckLisp_gensym(duckLisp, &gensym);
	if (e) goto l_cleanup;

	e = duckLisp_register_label(duckLisp, gensym.value, gensym.value_length);
	if (e) goto l_cleanup;

	// (goto gensym)
	e = duckLisp_emit_jump(duckLisp, assembly, gensym.value, gensym.value_length);
	if (e) goto l_cleanup;

	// (label function_name)
	e = duckLisp_emit_label(duckLisp,
	                        assembly,
	                        expression->compoundExpressions[1].value.identifier.value,
	                        expression->compoundExpressions[1].value.identifier.value_length);
	if (e) goto l_cleanup;

	{
		dl_ptrdiff_t function_label_index = -1;

		e = duckLisp_pushScope(duckLisp, dl_null, dl_true);
		if (e) goto l_cleanup;

		// `label_index` should never equal -1 after this function exits.
		e = scope_getLabelFromName(duckLisp,
		                           &function_label_index,
		                           expression->compoundExpressions[1].value.identifier.value,
		                           expression->compoundExpressions[1].value.identifier.value_length);
		if (e) goto l_cleanup;
		if (function_label_index == -1) {
			// We literally just added the function name to the parent scope.
			e = dl_error_cantHappen;
			goto l_cleanup;
		}


		// Arguments

		startStack_length = duckLisp->locals_length;

		if (expression->compoundExpressions[2].type != duckLisp_ast_type_int) {
			duckLisp_ast_expression_t *args_list = &expression->compoundExpressions[2].value.expression;
			for (dl_ptrdiff_t j = 0; (dl_size_t) j < args_list->compoundExpressions_length; j++) {

				if (args_list->compoundExpressions[j].type != duckLisp_ast_type_identifier) {
					e = duckLisp_error_pushRuntime(duckLisp, DL_STR("defun: All args must be identifiers."));
					if (e) goto l_cleanup;
					e = dl_error_invalidValue;
					goto l_cleanup;
				}

				/* --duckLisp->locals_length; */
				e = duckLisp_scope_addObject(duckLisp,
				                             args_list->compoundExpressions[j].value.identifier.value,
				                             args_list->compoundExpressions[j].value.identifier.value_length);
				if (e) {
					goto l_cleanup;
				}
				duckLisp->locals_length++;
			}
		}

		// Body

		duckLisp_ast_expression_t progn;
		progn.compoundExpressions = &expression->compoundExpressions[3];
		progn.compoundExpressions_length = expression->compoundExpressions_length - 3;
		e = duckLisp_generator_expression(duckLisp, assembly, &progn);
		if (e) goto l_cleanup;

		// Footer

		e = duckLisp_emit_return(duckLisp,
		                         assembly,
		                         TIF(expression->compoundExpressions[2].type == duckLisp_ast_type_expression,
		                             duckLisp->locals_length - startStack_length - 1,
		                             0));
		if (e) goto l_cleanup;

		duckLisp->locals_length = startStack_length;

		{
			/* This needs to be in the same scope or outer than the function arguments so that they don't get
			   captured. It should not need access to the function's local variables, so this scope should be fine. */
			duckLisp_scope_t scope;
			e = scope_getTop(duckLisp, &scope);
			if (e) goto l_cleanup;
			e = duckLisp_emit_pushClosure(duckLisp,
			                              assembly,
			                              dl_null,
			                              function_label_index,
			                              scope.function_uvs,
			                              scope.function_uvs_length);
			if (e) goto l_cleanup;
		}

		e = duckLisp_popScope(duckLisp, dl_null);
		if (e) goto l_cleanup;
	}

	// (label gensym)
	e = duckLisp_emit_label(duckLisp, assembly, gensym.value, gensym.value_length);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_not(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &args_index,
	                                        dl_null,
	                                        dl_false);
	if (e) goto l_cleanup;

	e = duckLisp_emit_not(duckLisp, assembly, args_index);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_multiply(duckLisp_t *duckLisp,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; i < 2; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

	/*
	  `add` accepts pointer arguments. It returns a value on the stack.
	*/
	e = duckLisp_emit_multiply(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_divide(duckLisp_t *duckLisp,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; i < 2; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

	/*
	  `add` accepts pointer arguments. It returns a value on the stack.
	*/
	e = duckLisp_emit_divide(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_add(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; i < 2; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

	/*
	  `add` accepts pointer arguments. It returns a value on the stack.
	*/
	e = duckLisp_emit_add(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_sub(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; i < 2; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}


	e = duckLisp_emit_sub(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_equal(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; i < 2; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

	e = duckLisp_emit_equal(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_greater(duckLisp_t *duckLisp,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; i < 2; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

	e = duckLisp_emit_greater(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_less(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	for (dl_ptrdiff_t i = 0; i < 2; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i + 1],
		                                        &args_index[i],
		                                        dl_null,
		                                        dl_false);
		if (e) goto l_cleanup;
	}

	e = duckLisp_emit_less(duckLisp, assembly, args_index[0], args_index[1]);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_while(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
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
		goto l_cleanup;
	}

	if (expression->compoundExpressions_length < 3) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Too few arguments for function \""));
		if (eError) {
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                               expression->compoundExpressions[0].value.identifier.value_length);
		if (eError) {
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    (char *) eString.elements,
		                                    eString.elements_length * eString.element_size);
		if (eError) e = eError;
		goto l_cleanup;
	}

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
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	if (forceGoto && branch) {
		e = duckLisp_gensym(duckLisp, &gensym_start);
		if (e) goto l_cleanup;

		e = duckLisp_register_label(duckLisp, gensym_start.value, gensym_start.value_length);
		if (e) goto l_free_gensym_start;

		e = duckLisp_emit_label(duckLisp, assembly, gensym_start.value, gensym_start.value_length);
		if (e) goto l_free_gensym_start;

		{
			duckLisp_ast_expression_t progn;

			e = duckLisp_pushScope(duckLisp, dl_null, dl_false);
			if (e) goto l_free_gensym_start;

			// Arguments

			startStack_length = duckLisp->locals_length;

			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, assembly, &progn);
			if (e) goto l_free_gensym_start;

			if (duckLisp->locals_length > startStack_length) {
				e = duckLisp_emit_pop(duckLisp, assembly, duckLisp->locals_length - startStack_length);
			}

			e = duckLisp_popScope(duckLisp, dl_null);
			if (e) goto l_free_gensym_start;
		}

		e = duckLisp_emit_jump(duckLisp,
		                       assembly,
		                       gensym_start.value,
		                       gensym_start.value_length);
		if (e) goto l_free_gensym_start;

		goto l_free_gensym_start;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_start);
		if (e) goto l_cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_loop);
		if (e) goto l_free_gensym_start;

		e = duckLisp_register_label(duckLisp, gensym_start.value, gensym_start.value_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_register_label(duckLisp, gensym_loop.value, gensym_loop.value_length);
		if (e) goto l_free_gensym_end;

		e = duckLisp_emit_jump(duckLisp, assembly, gensym_start.value, gensym_start.value_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_label(duckLisp, assembly, gensym_loop.value, gensym_loop.value_length);
		if (e) goto l_free_gensym_end;

		{
			duckLisp_ast_expression_t progn;

			e = duckLisp_pushScope(duckLisp, dl_null, dl_false);
			if (e) goto l_free_gensym_end;

			// Arguments

			startStack_length = duckLisp->locals_length;

			progn.compoundExpressions = &expression->compoundExpressions[2];
			progn.compoundExpressions_length = expression->compoundExpressions_length - 2;
			e = duckLisp_generator_expression(duckLisp, assembly, &progn);
			if (e) goto l_free_gensym_end;

			if (duckLisp->locals_length > startStack_length) {
				e = duckLisp_emit_pop(duckLisp, assembly, duckLisp->locals_length - startStack_length);
			}

			e = duckLisp_popScope(duckLisp, dl_null);
			if (e) goto l_free_gensym_end;
		}

		e = duckLisp_emit_label(duckLisp, assembly, gensym_start.value, gensym_start.value_length);
		if (e) goto l_free_gensym_end;
		startStack_length = duckLisp->locals_length;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[1],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_brnz(duckLisp,
		                       assembly,
		                       gensym_loop.value,
		                       gensym_loop.value_length,
		                       duckLisp->locals_length - startStack_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_pushInteger(duckLisp, assembly, dl_null, 0);
		if (e) goto l_free_gensym_end;

		goto l_free_gensym_end;
	}
	/* Flow does not reach here. */

	/*
	  (goto start)
	  (label loop)

	  (label start)
	  (brnz condition loop)
	*/

 l_free_gensym_end:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_loop.value);
	gensym_loop.value_length = 0;
 l_free_gensym_start:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_start.value);
	gensym_start.value_length = 0;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_unless(duckLisp_t *duckLisp,
                                     dl_array_t *assembly,
                                     duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_ptrdiff_t startStack_length;
	/* dl_bool_t noPop = dl_false; */
	int pops = 0;

	duckLisp_ast_identifier_t gensym_then;
	duckLisp_ast_identifier_t gensym_end;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

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
		e = duckLisp_scope_getLocalIndexFromName(duckLisp,
		                                         &args_index[0],
		                                         expression->compoundExpressions[1].value.identifier.value,
		                                         expression->compoundExpressions[1].value.identifier.value_length);
		if (e) goto l_cleanup;
		if (args_index[0] == -1) {
			e = dl_error_invalidValue;
			eError = dl_array_pushElements(&eString, DL_STR("Could not find local \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[1].value.identifier.value,
			                               expression->compoundExpressions[1].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\" in generator \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
			                               expression->compoundExpressions[0].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\"."));
			if (eError) {
				goto l_cleanup;
			}
			eError = duckLisp_error_pushRuntime(duckLisp,
			                                    eString.elements,
			                                    eString.elements_length * eString.element_size);
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		e = duckLisp_emit_pushIndex(duckLisp, assembly, dl_null, args_index[0]);
		if (e) goto l_cleanup;
		break;
	case duckLisp_ast_type_expression:
		startStack_length = duckLisp->locals_length;
		e = duckLisp_compile_expression(duckLisp,
		                                assembly,
		                                expression->compoundExpressions[0].value.identifier.value,
		                                expression->compoundExpressions[0].value.identifier.value_length,
		                                &expression->compoundExpressions[1].value.expression);
		if (e) goto l_cleanup;
		pops = duckLisp->locals_length - startStack_length;

		args_index[0] = duckLisp->locals_length - 1;
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("unless: Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	if (forceGoto && branch) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[2],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		goto l_cleanup;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_then);
		if (e) goto l_cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_end);
		if (e) goto l_free_gensym_then;

		e = duckLisp_register_label(duckLisp, gensym_then.value, gensym_then.value_length);
		if (e) goto l_free_gensym_end;

		e = duckLisp_register_label(duckLisp, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;

		e = duckLisp_emit_brnz(duckLisp, assembly, gensym_then.value, gensym_then.value_length, pops);
		if (e) goto l_free_gensym_end;
		startStack_length = duckLisp->locals_length;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[2],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto l_free_gensym_end;
		pops = duckLisp->locals_length - startStack_length - 1;
		if (pops < 0) {
			e = duckLisp_emit_pushInteger(duckLisp, assembly, dl_null, 0);
		}
		else if (pops > 0) {
			e = duckLisp_emit_pop(duckLisp, assembly, pops);
		}
		e = duckLisp_emit_jump(duckLisp, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_label(duckLisp, assembly, gensym_then.value, gensym_then.value_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_pushInteger(duckLisp, assembly, dl_null, 0);
		if (e) goto l_free_gensym_end;
		// This is to account for the fact that we have two expressions, but only one that is run.
		--duckLisp->locals_length;
		e = duckLisp_emit_label(duckLisp, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;

		goto l_free_gensym_end;
	}
	/* Flow does not reach here. */

 l_free_gensym_end:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_end.value);
	gensym_end.value_length = 0;
 l_free_gensym_then:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_then.value);
	gensym_then.value_length = 0;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_when(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_ptrdiff_t startStack_length;
	/* dl_bool_t noPop = dl_false; */
	int pops = 0;

	duckLisp_ast_identifier_t gensym_then;
	duckLisp_ast_identifier_t gensym_end;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

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
		e = duckLisp_scope_getLocalIndexFromName(duckLisp,
		                                         &args_index[0],
		                                         expression->compoundExpressions[1].value.identifier.value,
		                                         expression->compoundExpressions[1].value.identifier.value_length);
		if (e) goto l_cleanup;
		if (args_index[0] == -1) {
			e = dl_error_invalidValue;
			eError = dl_array_pushElements(&eString, DL_STR("Could not find local \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[1].value.identifier.value,
			                               expression->compoundExpressions[1].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\" in generator \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
			                               expression->compoundExpressions[0].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\"."));
			if (eError) {
				goto l_cleanup;
			}
			eError = duckLisp_error_pushRuntime(duckLisp,
			                                    eString.elements,
			                                    eString.elements_length * eString.element_size);
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		e = duckLisp_emit_pushIndex(duckLisp, assembly, dl_null, args_index[0]);
		if (e) goto l_cleanup;
		break;
	case duckLisp_ast_type_expression:
		startStack_length = duckLisp->locals_length;
		e = duckLisp_compile_expression(duckLisp,
		                                assembly,
		                                expression->compoundExpressions[0].value.identifier.value,
		                                expression->compoundExpressions[0].value.identifier.value_length,
		                                &expression->compoundExpressions[1].value.expression);
		if (e) goto l_cleanup;
		pops = duckLisp->locals_length - startStack_length;

		args_index[0] = duckLisp->locals_length - 1;
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("if: Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	if (forceGoto && branch) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[2],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		goto l_cleanup;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_then);
		if (e) goto l_cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_end);
		if (e) goto l_free_gensym_then;

		e = duckLisp_register_label(duckLisp, gensym_then.value, gensym_then.value_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_register_label(duckLisp, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;

		e = duckLisp_emit_brnz(duckLisp, assembly, gensym_then.value, gensym_then.value_length, pops);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_pushInteger(duckLisp, assembly, dl_null, 0);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_jump(duckLisp, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_label(duckLisp, assembly, gensym_then.value, gensym_then.value_length);
		if (e) goto l_free_gensym_end;
		startStack_length = duckLisp->locals_length;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[2],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto l_free_gensym_end;
		pops = duckLisp->locals_length - startStack_length - 1;
		if (pops < 0) {
			e = duckLisp_emit_pushInteger(duckLisp, assembly, dl_null, 0);
		}
		else if (pops > 0) {
			e = duckLisp_emit_pop(duckLisp, assembly, pops);
		}
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_label(duckLisp, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;

		goto l_free_gensym_end;
	}
	/* Flow does not reach here. */

 l_free_gensym_end:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_end.value);
	gensym_end.value_length = 0;
 l_free_gensym_then:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_then.value);
	gensym_then.value_length = 0;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_if(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_ptrdiff_t startStack_length;
	/* dl_bool_t noPop = dl_false; */
	int pops = 0;

	duckLisp_ast_identifier_t gensym_then;
	duckLisp_ast_identifier_t gensym_end;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 4);
	if (e) {
		goto l_cleanup;
	}

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
		e = duckLisp_scope_getLocalIndexFromName(duckLisp,
		                                         &args_index[0],
		                                         expression->compoundExpressions[1].value.identifier.value,
		                                         expression->compoundExpressions[1].value.identifier.value_length);
		if (e) goto l_cleanup;
		if (args_index[0] == -1) {
			e = dl_error_invalidValue;
			eError = dl_array_pushElements(&eString, DL_STR("Could not find local \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[1].value.identifier.value,
			                               expression->compoundExpressions[1].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\" in generator \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
			                               expression->compoundExpressions[0].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\"."));
			if (eError) {
				goto l_cleanup;
			}
			eError = duckLisp_error_pushRuntime(duckLisp,
			                                    eString.elements,
			                                    eString.elements_length * eString.element_size);
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		e = duckLisp_emit_pushIndex(duckLisp, assembly, dl_null, args_index[0]);
		if (e) goto l_cleanup;
		break;
	case duckLisp_ast_type_expression:
		startStack_length = duckLisp->locals_length;
		e = duckLisp_compile_expression(duckLisp,
		                                assembly,
		                                expression->compoundExpressions[0].value.identifier.value,
		                                expression->compoundExpressions[0].value.identifier.value_length,
		                                &expression->compoundExpressions[1].value.expression);
		if (e) goto l_cleanup;
		pops = duckLisp->locals_length - startStack_length;

		args_index[0] = duckLisp->locals_length - 1;
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("if: Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	if (forceGoto) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[branch
		                                                                         ? 2
		                                                                         : 3],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		goto l_cleanup;
	}
	else {
		e = duckLisp_gensym(duckLisp, &gensym_then);
		if (e) goto l_cleanup;
		e = duckLisp_gensym(duckLisp, &gensym_end);
		if (e) goto l_free_gensym_then;

		e = duckLisp_register_label(duckLisp, gensym_then.value, gensym_then.value_length);
		if (e) goto l_free_gensym_end;

		e = duckLisp_register_label(duckLisp, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;

		e = duckLisp_emit_brnz(duckLisp, assembly, gensym_then.value, gensym_then.value_length, pops);
		if (e) goto l_free_gensym_end;

		startStack_length = duckLisp->locals_length;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[3],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		pops = duckLisp->locals_length - startStack_length - 1;
		if (pops < 0) {
			e = dl_array_pushElements(&eString, DL_STR("if: \"else\" part of expression contains an invalid form"));
			if (e) {
				goto l_free_gensym_end;
			}
		}
		else if (pops > 0) {
			e = duckLisp_emit_move(duckLisp, assembly, startStack_length, duckLisp->locals_length - 1);
			if (e) goto l_free_gensym_end;
			e = duckLisp_emit_pop(duckLisp, assembly, pops);
		}
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_jump(duckLisp, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;
		e = duckLisp_emit_label(duckLisp, assembly, gensym_then.value, gensym_then.value_length);

		duckLisp->locals_length = startStack_length;

		if (e) goto l_free_gensym_end;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[2],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		pops = duckLisp->locals_length - startStack_length - 1;
		if (pops < 0) {
			e = dl_array_pushElements(&eString, DL_STR("if: \"then\" part of expression contains an invalid form"));
			if (e) {
				goto l_free_gensym_end;
			}
		}
		else if (pops > 0) {
			e = duckLisp_emit_move(duckLisp, assembly, startStack_length, duckLisp->locals_length - 1);
			if (e) goto l_free_gensym_end;
			e = duckLisp_emit_pop(duckLisp, assembly, pops);
		}
		if (e) goto l_free_gensym_end;

		e = duckLisp_emit_label(duckLisp, assembly, gensym_end.value, gensym_end.value_length);
		if (e) goto l_free_gensym_end;

		goto l_free_gensym_end;
	}
	/* Flow does not reach here. */

	/* (brnz condition $then); */
	/* else; */
	/* (goto $end); */
	/* (label $then); */
	/* then; */
	/* (label $end); */

 l_free_gensym_end:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_end.value);
	gensym_end.value_length = 0;
 l_free_gensym_then:
	e = dl_free(duckLisp->memoryAllocation, (void **) &gensym_then.value);
	gensym_then.value_length = 0;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_brnz(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t args_index[2] = {-1, -1};

	dl_bool_t forceGoto = dl_false;
	dl_bool_t branch = dl_false;

	dl_ptrdiff_t startStack_length;
	/* dl_bool_t noPop = dl_false; */
	int pops = 0;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

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
		e = duckLisp_scope_getLocalIndexFromName(duckLisp,
		                                         &args_index[0],
		                                         expression->compoundExpressions[1].value.identifier.value,
		                                         expression->compoundExpressions[1].value.identifier.value_length);
		if (e) goto l_cleanup;
		if (args_index[0] == -1) {
			e = dl_error_invalidValue;
			eError = dl_array_pushElements(&eString, DL_STR("Could not find local \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[1].value.identifier.value,
			                               expression->compoundExpressions[1].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\" in generator \""));
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
			                               expression->compoundExpressions[0].value.identifier.value_length);
			if (eError) {
				goto l_cleanup;
			}
			eError = dl_array_pushElements(&eString, DL_STR("\"."));
			if (eError) {
				goto l_cleanup;
			}
			eError = duckLisp_error_pushRuntime(duckLisp,
			                                    eString.elements,
			                                    eString.elements_length * eString.element_size);
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		// I think we need to pop 1? I don't see anything being pushed though.
		break;
	case duckLisp_ast_type_expression:

		startStack_length = duckLisp->locals_length;
		// I don't *think* we need to worry about `var` in conditions. If you do that outside of a progn, you are practically begging for a stack corruption.
		/* if ((currentExpression.value.expression.compoundExpressions_length > 0) && */
		/*		(currentExpression.value.expression.compoundExpressions[0].type == duckLisp_ast_type_identifier)) { */
		/*		dl_string_compare(&noPop, currentExpression.value.expression.compoundExpressions[0].value.identifier.value, */
		/*						  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length, */
		/*						  DL_STR("var")); */
		/* } */
		e = duckLisp_compile_expression(duckLisp,
		                                assembly,
		                                expression->compoundExpressions[0].value.identifier.value,
		                                expression->compoundExpressions[0].value.identifier.value_length,
		                                &expression->compoundExpressions[1].value.expression);
		if (e) goto l_cleanup;
		pops = duckLisp->locals_length - startStack_length;
		/* if (!noPop && (duckLisp->locals_length > startStack_length)) { */
		/*		e = duckLisp_emit_pop(duckLisp, assembly, duckLisp->locals_length - startStack_length); */
		/*		noPop = dl_true; */
		/* } */

		args_index[0] = duckLisp->locals_length - 1;
		break;
	default:
		e = dl_array_pushElements(&eString, DL_STR("brnz: Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Label
	if (expression->compoundExpressions[2].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 2 of function \""));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString,
		                          expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	if (forceGoto) {
		if (branch) {
			e = duckLisp_emit_jump(duckLisp, assembly, expression->compoundExpressions[2].value.string.value,
			                       expression->compoundExpressions[2].value.string.value_length);
		}
	}
	else {
		e = duckLisp_emit_brnz(duckLisp, assembly, expression->compoundExpressions[2].value.string.value,
		                       expression->compoundExpressions[2].value.string.value_length,
		                       pops);
	}
	if (e) {
		goto l_cleanup;
	}

	// Don't push label into trie. This will be done later during assembly.

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_setq(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t index = -1;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("setq: Argument 1 of function \""));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[2],
	                                        &index,
	                                        dl_null,
	                                        dl_true);
	if (e) goto l_cleanup;

	// Unlike most other instances, this is for assignment.
	e = duckLisp_scope_getLocalIndexFromName(duckLisp,
	                                         &identifier_index,
	                                         expression->compoundExpressions[1].value.identifier.value,
	                                         expression->compoundExpressions[1].value.identifier.value_length);
	if (e) goto l_cleanup;
	if (identifier_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("setq: Could not find local \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, expression->compoundExpressions[1].value.identifier.value,
		                               expression->compoundExpressions[1].value.identifier.value_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\" in generator \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                               expression->compoundExpressions[0].value.identifier.value_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	e = duckLisp_emit_move(duckLisp, assembly, identifier_index, duckLisp->locals_length - 1);
	if (e) goto l_cleanup;

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_createVar(duckLisp_t *duckLisp,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_size_t startStack_length = duckLisp->locals_length;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	if (e) {
		goto l_cleanup;
	}

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Insert arg1 into this scope's name trie.
	e = duckLisp_scope_addObject(duckLisp, expression->compoundExpressions[1].value.identifier.value,
	                             expression->compoundExpressions[1].value.identifier.value_length);
	if (e) {
		goto l_cleanup;
	}

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[2],
	                                        dl_null,
	                                        dl_null,
	                                        dl_true);
	if (e) goto l_cleanup;

	if (startStack_length != duckLisp->locals_length - 1) {
		e = duckLisp_emit_move(duckLisp, assembly, startStack_length, duckLisp->locals_length - 1);
		if (e) goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_comment(duckLisp_t *duckLisp,
                                      dl_array_t *assembly,
                                      duckLisp_ast_expression_t *expression) {
	(void) duckLisp;
	(void) assembly;

	expression->compoundExpressions_length = 0;

	return dl_error_ok;
}

dl_error_t duckLisp_generator_nop(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	(void) expression;

	return duckLisp_emit_nop(duckLisp, assembly);
}

dl_error_t duckLisp_generator_label(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString,
		                          expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	e = duckLisp_emit_label(duckLisp,
	                        assembly,
	                        expression->compoundExpressions[1].value.string.value,
	                        expression->compoundExpressions[1].value.string.value_length);
	if (e) {
		goto l_cleanup;
	}

	// Don't push label into trie. This will be done later during assembly.

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_goto(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 2);
	if (e) {
		goto l_cleanup;
	}

	if (expression->compoundExpressions[1].type != duckLisp_ast_type_identifier) {
		e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString,
		                          expression->compoundExpressions[0].value.identifier.value,
		                          expression->compoundExpressions[0].value.identifier.value_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	e = duckLisp_emit_jump(duckLisp,
	                       assembly,
	                       expression->compoundExpressions[1].value.string.value,
	                       expression->compoundExpressions[1].value.string.value_length);
	if (e) {
		goto l_cleanup;
	}

	// Don't push label into trie. This will be done later during assembly.

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_pushScope(duckLisp_t *duckLisp,
                                        dl_array_t *assembly,
                                        duckLisp_ast_expression_t *expression) {
	(void) assembly;
	dl_error_t e = dl_error_ok;

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 1);
	if (e) {
		goto l_cleanup;
	}

	// Push a new scope.
	e = duckLisp_pushScope(duckLisp, dl_null, dl_false);

 l_cleanup:

	return e;
}

dl_error_t duckLisp_generator_popScope(duckLisp_t *duckLisp,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	(void) assembly;
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	/* Check arguments for call and type errors. */

	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 1);
	if (e) {
		goto l_cleanup;
	}

	// Pop a new scope.
	e = duckLisp_popScope(duckLisp, dl_null);

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_acall(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t outerStartStack_length;
	dl_ptrdiff_t innerStartStack_length;


	if (expression->compoundExpressions_length == 0) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	if (expression->compoundExpressions[0].type != duckLisp_ast_type_identifier) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}

	if (expression->compoundExpressions_length < 2) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Too few arguments for function \""));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString,
		                               expression->compoundExpressions[0].value.identifier.value,
		                               expression->compoundExpressions[0].value.identifier.value_length);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    (char *) eString.elements,
		                                    eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
			goto l_cleanup;
		}
		goto l_cleanup;
	}

	/* Generate */

	e = duckLisp_compile_compoundExpression(duckLisp,
	                                        assembly,
	                                        expression->compoundExpressions[0].value.identifier.value,
	                                        expression->compoundExpressions[0].value.identifier.value_length,
	                                        &expression->compoundExpressions[1],
	                                        &identifier_index,
	                                        dl_null,
	                                        dl_true);
	if (e) goto l_cleanup;

	outerStartStack_length = duckLisp->locals_length;

	for (dl_ptrdiff_t i = 2; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = duckLisp->locals_length;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto l_cleanup;

		e = duckLisp_emit_move(duckLisp, assembly, innerStartStack_length, duckLisp->locals_length - 1);
		if (e) goto l_cleanup;
		e = duckLisp_emit_pop(duckLisp, assembly, duckLisp->locals_length - innerStartStack_length - 1);
		if (e) goto l_cleanup;
	}

	// The zeroth argument is the function name, which also happens to be a label.
	e = duckLisp_emit_acall(duckLisp, assembly, identifier_index, 0);
	if (e) {
		goto l_cleanup;
	}

	duckLisp->locals_length = outerStartStack_length + 1;

	// Don't push label into trie. This will be done later during assembly.

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_subroutine(duckLisp_t *duckLisp,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t outerStartStack_length = duckLisp->locals_length;
	dl_ptrdiff_t innerStartStack_length;

	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		innerStartStack_length = duckLisp->locals_length;
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        &identifier_index,
		                                        dl_null,
		                                        dl_true);
		if (e) goto l_cleanup;

		e = duckLisp_emit_move(duckLisp, assembly, innerStartStack_length, duckLisp->locals_length - 1);
		if (e) goto l_cleanup;
		e = duckLisp_emit_pop(duckLisp, assembly, duckLisp->locals_length - innerStartStack_length - 1);
		if (e) goto l_cleanup;
	}
	duckLisp->locals_length = outerStartStack_length + 1;

	// The zeroth argument is the function name, which also happens to be a label.
	e = duckLisp_emit_call(duckLisp,
	                       assembly,
	                       expression->compoundExpressions[0].value.string.value,
	                       expression->compoundExpressions[0].value.string.value_length,
	                       expression->compoundExpressions_length - 1);
	if (e) {
		goto l_cleanup;
	}

	// Don't push label into trie. This will be done later during assembly.

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_callback(duckLisp_t *duckLisp,
                                       dl_array_t *assembly,
                                       duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t callback_index = -1;

	e = duckLisp_scope_getStaticIndexFromName(duckLisp, &callback_index,
	                                          expression->compoundExpressions[0].value.string.value,
	                                          expression->compoundExpressions[0].value.string.value_length);
	if (e || callback_index == -1) {
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("callback: Could not find callback name."));
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	// Push all arguments onto the stack.
	for (dl_ptrdiff_t i = 1; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        expression->compoundExpressions[0].value.identifier.value,
		                                        expression->compoundExpressions[0].value.identifier.value_length,
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto l_cleanup;
	}

	// Create the string variable.
	e = duckLisp_emit_ccall(duckLisp, assembly, callback_index);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp,
                                         dl_array_t *assembly,
                                         duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_bool_t result = dl_false;
	duckLisp_scope_t scope;
	dl_ptrdiff_t label_index = -1;

	dl_size_t startStack_length;
	dl_bool_t foundVar = dl_false;
	dl_bool_t foundInclude = dl_false;
	dl_size_t pops = 0;

	// Push a new scope.
	e = duckLisp_pushScope(duckLisp, dl_null, dl_false);
	if (e) {
		goto l_cleanup;
	}

	/* Labels */

	// I believe this should come before any compilation.

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		duckLisp_ast_compoundExpression_t currentCE = expression->compoundExpressions[i];
		if (currentCE.type == duckLisp_ast_type_expression) {
			duckLisp_ast_expression_t currentE = currentCE.value.expression;
			if (currentE.compoundExpressions_length == 2 &&
			    currentE.compoundExpressions[0].type == duckLisp_ast_type_identifier &&
			    currentE.compoundExpressions[1].type == duckLisp_ast_type_identifier) {

				duckLisp_ast_identifier_t function = currentE.compoundExpressions[0].value.identifier;
				/**/ dl_string_compare(&result, function.value, function.value_length, DL_STR("label"));
				if (result) {
					duckLisp_ast_identifier_t label_name = currentE.compoundExpressions[1].value.identifier;
					duckLisp_label_t label;

					// This is why we pushed the scope here.
					e = scope_getTop(duckLisp, &scope);
					if (e) {
						goto l_cleanup;
					}

					// Make sure label is undeclared.
					/**/ dl_trie_find(scope.labels_trie, &label_index, label_name.value, label_name.value_length);
					if (label_index != -1) {
						e = dl_error_invalidValue;
						eError = dl_array_pushElements(&eString, DL_STR("Multiple definitions of label \""));
						if (eError) {
							e = eError;
							goto l_cleanup;
						}
						eError = dl_array_pushElements(&eString, label_name.value, label_name.value_length);
						if (eError) {
							e = eError;
							goto l_cleanup;
						}
						eError = dl_array_pushElements(&eString, DL_STR("\"."));
						if (eError) {
							e = eError;
							goto l_cleanup;
						}
						eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements),
						                                    eString.elements_length);
						if (eError) {
							e = eError;
						}
						goto l_cleanup;
					}

					// declare the label.
					label.name = label_name.value;
					label.name_length = label_name.value_length;

					/**/ dl_array_init(&label.sources,
					                   duckLisp->memoryAllocation,
					                   sizeof(duckLisp_label_source_t),
					                   dl_array_strategy_double);
					label.target = -1;
					e = dl_array_pushElement(&duckLisp->labels, &label);
					if (e) {
						goto l_cleanup;
					}
					label_index = duckLisp->labels.elements_length - 1;
					e = dl_trie_insert(&scope.labels_trie, label.name, label.name_length, label_index);
					if (e) {
						goto l_cleanup;
					}


					e = scope_setTop(duckLisp, &scope);
					if (e) {
						goto l_cleanup;
					}
				}
			}
		}
	}

	/* Compile */

	for (dl_ptrdiff_t i = 0; (dl_size_t) i < expression->compoundExpressions_length; i++) {
		duckLisp_ast_compoundExpression_t currentExpression = expression->compoundExpressions[i];
		startStack_length = duckLisp->locals_length;
		if ((currentExpression.type == duckLisp_ast_type_expression)
		    && (currentExpression.value.expression.compoundExpressions_length > 0)
		    && (currentExpression.value.expression.compoundExpressions[0].type == duckLisp_ast_type_identifier)) {
			dl_string_compare(&foundVar,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length,
			                  DL_STR("var"));
			// `include` is an exception because the included file exists in the parent scope.
			dl_string_compare(&foundInclude,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value,
			                  currentExpression.value.expression.compoundExpressions[0].value.identifier.value_length,
			                  DL_STR("include"));
		}
		e = duckLisp_compile_compoundExpression(duckLisp,
		                                        assembly,
		                                        DL_STR("expression"),
		                                        &expression->compoundExpressions[i],
		                                        dl_null,
		                                        dl_null,
		                                        dl_true);
		if (e) goto l_cleanup;
		if (!foundInclude
		    && (duckLisp->locals_length
		        > (startStack_length + ((dl_size_t) i == expression->compoundExpressions_length - 1)))) {
			/* if ((dl_size_t) i != expression->compoundExpressions_length - 1) { */
			/*		e = duckLisp_emit_move(duckLisp, assembly, startStack_length, duckLisp->locals_length - 1); */
			/* } */
			if (startStack_length != duckLisp->locals_length - 1) {
				e = duckLisp_emit_move(duckLisp, assembly, startStack_length, duckLisp->locals_length - 1);
				if (e) goto l_cleanup;
			}
			pops = (duckLisp->locals_length
			        - startStack_length
			        - ((dl_size_t) i == expression->compoundExpressions_length - 1)
			        - foundVar);
			if (pops > 0) {
				e = duckLisp_emit_pop(duckLisp, assembly, pops);
			}
		}
		foundInclude = dl_false;
	}

	// And pop it... This is so much easier than it used to be. No more queuing `pop-scope`s.
	e = duckLisp_popScope(duckLisp, dl_null);

 l_cleanup:

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

/*
  =======
  Compile
  =======
*/

// This is only to be used after the bytecode has been fully assembled.
typedef struct {
	// If this is an array index to a linked list element, incrementing the link address will not necessarily increment
	// this variable.
	dl_ptrdiff_t source;	// Points to the array (not list) index.
	dl_ptrdiff_t target;	// Points to the array (not list) index.
	dl_uint8_t size;	// Can hold values 1-4.
	dl_bool_t forward;	// True if a forward reference.
	dl_bool_t absolute; // Indicates an absolute address, which is always 32 bits.
} jumpLink_t;

typedef struct {
	jumpLink_t *links;
	dl_size_t links_length;
} linkArray_t;

typedef enum {
	jumpLinkPointers_type_address,
	jumpLinkPointers_type_target,
} jumpLinkPointer_type_t;

typedef struct {
	dl_ptrdiff_t index;
	jumpLinkPointer_type_t type;
} jumpLinkPointer_t;

int jumpLink_less(const void *l, const void *r, const void *context) {
	// Array of links.
	const linkArray_t *linkArray = context;
	// Pointers to links.
	const jumpLinkPointer_t *left_pointer = l;
	const jumpLinkPointer_t *right_pointer = r;
	// Addresses we are comparing.
	/* See those `2 * `s and ` + 1`s? We call that a hack. If we have
	   (label l1) (goto l2) (nop) (goto l1) (label l2)
	   then the source address assigned to (goto l1) is the same as the target address
	   assigned to (label l2). This *should* be fine, but Hoare Quicksort messes with
	   the order when indexing the links. To force an explicit order, we append an
	   extra bit that is set to make the comparison think that labels are larger than
	   the equivalent goto.
	*/
	const dl_ptrdiff_t left = ((left_pointer->type == jumpLinkPointers_type_target)
	                           ? (2 * linkArray->links[left_pointer->index].target + 1)
	                           : (2 * linkArray->links[left_pointer->index].source));
	const dl_ptrdiff_t right = ((right_pointer->type == jumpLinkPointers_type_target)
	                            ? (2 * linkArray->links[right_pointer->index].target + 1)
	                            : (2 * linkArray->links[right_pointer->index].source));
	return left - right;
}

dl_error_t duckLisp_compile_compoundExpression(duckLisp_t *duckLisp,
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
	/* dl_ptrdiff_t upvalue_index; */
	duckLisp_ast_type_t temp_type;

	switch (compoundExpression->type) {
	case duckLisp_ast_type_bool:
		e = duckLisp_emit_pushBoolean(duckLisp, assembly, &temp_index, compoundExpression->value.boolean.value);
		if (e) goto l_cleanup;
		temp_type = duckLisp_ast_type_bool;
		break;
	case duckLisp_ast_type_int:
		e = duckLisp_emit_pushInteger(duckLisp, assembly, &temp_index, compoundExpression->value.integer.value);
		if (e) goto l_cleanup;
		temp_type = duckLisp_ast_type_int;
		break;
	/* case duckLisp_ast_type_float: */
	/* 	e = duckLisp_emit_pushFloat(duckLisp, */
	/* 	                            assembly, */
	/* 	                            &temp_index, */
	/* 	                            compoundExpression->value.floatingPoint.value); */
	/* 	if (e) goto l_cleanup; */
	/* 	temp_type = duckLisp_ast_type_float; */
	/* 	break; */
	case duckLisp_ast_type_string:
		e = duckLisp_emit_pushString(duckLisp, assembly, &temp_index, compoundExpression->value.string.value,
		                             compoundExpression->value.string.value_length);
		if (e) goto l_cleanup;
		temp_type = duckLisp_ast_type_string;
		break;
	case duckLisp_ast_type_identifier:
		e = duckLisp_scope_getLocalIndexFromName(duckLisp,
		                                         &temp_index,
		                                         compoundExpression->value.identifier.value,
		                                         compoundExpression->value.identifier.value_length);
		if (e) goto l_cleanup;
		if (temp_index == -1) {
			dl_ptrdiff_t scope_index;
			e = duckLisp_scope_getFreeLocalIndexFromName(duckLisp,
			                                             &temp_index,
			                                             &scope_index,
			                                             compoundExpression->value.identifier.value,
			                                             compoundExpression->value.identifier.value_length);
			if (e) goto l_cleanup;
			if (temp_index == -1) {
				// Now we check for labels.
				e = scope_getLabelFromName(duckLisp,
				                           &temp_index,
				                           compoundExpression->value.identifier.value,
				                           compoundExpression->value.identifier.value_length);
				if (e) goto l_cleanup;
				if (temp_index == -1) {
					e = dl_array_pushElements(&eString, DL_STR("compoundExpression: Could not find variable \""));
					if (e) {
						goto l_cleanup;
					}
					e = dl_array_pushElements(&eString, compoundExpression->value.identifier.value,
					                          compoundExpression->value.identifier.value_length);
					if (e) {
						goto l_cleanup;
					}
					e = dl_array_pushElements(&eString, DL_STR("\"."));
					if (e) {
						goto l_cleanup;
					}
					eError = duckLisp_error_pushRuntime(duckLisp,
					                                    eString.elements,
					                                    eString.elements_length * eString.element_size);
					if (eError) {
						e = eError;
					}
					e = dl_error_invalidValue;
					goto l_cleanup;
				}
				else {
					e = duckLisp_emit_pushLabel(duckLisp, assembly, dl_null, temp_index);
				}
			}
			else {
				/**/
				/* Now the trick here is that we need to mirror the free variable as a local variable.
				   Actually, scratch that. We need to simply push the UV. Creating it as a local variable is an
				   optimization that can be done in `duckLisp_compile_expression`. It can't be done here. */
				printf("Found free variable \"");
				for (dl_ptrdiff_t i = 0; (dl_size_t) i < compoundExpression->value.identifier.value_length; i++) {
					putchar(compoundExpression->value.identifier.value[i]);
				}
				printf("\" in scope %lli with index %lli.\n", scope_index, temp_index);
				e = duckLisp_emit_pushUpvalue(duckLisp, assembly, dl_null, temp_index);
				if (e) goto l_cleanup;
			}
		}
		else if (pushReference) {
			// We are NOT pushing an index since the index is part of the instruction.
			e = duckLisp_emit_pushIndex(duckLisp, assembly, dl_null, temp_index);
			if (e) {
				goto l_cleanup;
			}
		}

		temp_type = duckLisp_ast_type_none;  // Let's use `none` as a wildcard. Variables do not have a set type.
		break;
	case duckLisp_ast_type_expression:
		e = duckLisp_compile_expression(duckLisp,
		                                assembly,
		                                functionName,
		                                functionName_length,
		                                &compoundExpression->value.expression);
		if (e) goto l_cleanup;
		temp_index = duckLisp->locals_length - 1;
		temp_type = duckLisp_ast_type_none;
		break;
	default:
		temp_type = duckLisp_ast_type_none;
		e = dl_array_pushElements(&eString, functionName, functionName_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	if (index != dl_null) *index = temp_index;
	if (type != dl_null) *type = temp_type;

 l_cleanup:
	return e;
}

dl_error_t duckLisp_compile_expression(duckLisp_t *duckLisp,
                                       dl_array_t *assembly,
                                       char *functionName,
                                       const dl_size_t functionName_length,
                                       duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_functionType_t functionType;
	dl_ptrdiff_t functionIndex = -1;

	dl_error_t (*generatorCallback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*);

	if (expression->compoundExpressions_length == 0) {
		e = duckLisp_emit_nil(duckLisp, assembly);
		goto l_cleanup;
	}

	// Compile!
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
		e = duckLisp_generator_expression(duckLisp, assembly, expression);
		if (e) {
			goto l_cleanup;
		}
		break;
	case duckLisp_ast_type_identifier:
		// Run function generator.
		{
			duckLisp_ast_identifier_t functionName = {0};
			functionName = expression->compoundExpressions[0].value.identifier;
			e = scope_getFunctionFromName(duckLisp, &functionType, &functionIndex, functionName.value,
			                              functionName.value_length);
			if (e) {
				goto l_cleanup;
			}
			if (functionType == duckLisp_functionType_none) {
				e = dl_error_invalidValue;
				eError = dl_array_pushElements(&eString, DL_STR("Symbol \""));
				if (eError) {
					e = eError;
					goto l_cleanup;
				}
				eError = dl_array_pushElements(&eString, functionName.value, functionName.value_length);
				if (eError) {
					e = eError;
					goto l_cleanup;
				}
				eError = dl_array_pushElements(&eString, DL_STR("\" is not a function, callback, or generator."));
				if (eError) {
					e = eError;
					goto l_cleanup;
				}
				eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements), eString.elements_length);
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
		}
		switch (functionType) {
		case duckLisp_functionType_ducklisp:
			/* What's nice is we only need the label to generate the required code. No tries required. ^‿^ */
			e = duckLisp_generator_subroutine(duckLisp, assembly, expression);
			if (e) {
				goto l_cleanup;
			}
			break;
		case duckLisp_functionType_c:
			e = duckLisp_generator_callback(duckLisp, assembly, expression);
			if (e) {
				goto l_cleanup;
			}
			break;
		case duckLisp_functionType_generator:
			// e = m_generatorAt(functionIndex)(duckLisp, &currentExpression)
			e = dl_array_get(&duckLisp->generators_stack, &generatorCallback, functionIndex);
			if (e) {
				goto l_cleanup;
			}
			e = generatorCallback(duckLisp, assembly, expression);
			if (e) {
				goto l_cleanup;
			}
			break;
		default:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid function type. Can't happen."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		break;
	default:
		e = dl_array_pushElements(&eString, functionName, functionName_length);
		if (e) {
			goto l_cleanup;
		}
		e = dl_array_pushElements(&eString, DL_STR(": Unsupported data type."));
		if (e) {
			goto l_cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

 l_cleanup:
	return e;
}

dl_error_t duckLisp_compileAST(duckLisp_t *duckLisp,
                               dl_array_t *bytecode,
                               duckLisp_ast_compoundExpression_t astCompoundexpression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	// Not initialized until later. This is reused for multiple arrays.
	dl_array_t assembly; // duckLisp_instructionObject_t
	// Initialize to zero.
	/**/ dl_array_init(&assembly,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionObject_t),
	                   dl_array_strategy_fit);
	duckLisp_instructionObject_t tempInstructionObject;
	// Create high-level bytecode list.
	/* dl_array_t instructionList; // dl_array_t */
	/* /\**\/ dl_array_init(&instructionList, &duckLisp->memoryAllocation, sizeof(dl_array_t), dl_array_strategy_double); */
	// expression stack for navigating the tree.
	dl_array_t expressionStack;
	/**/ dl_array_init(&expressionStack,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_ast_compoundExpression_t),
	                   dl_array_strategy_double);

	typedef struct {
		dl_uint8_t byte;
		dl_ptrdiff_t next;
		dl_ptrdiff_t prev;
	} byteLink_t;

	dl_array_t bytecodeList; // byteLink_t
	/**/ dl_array_init(&bytecodeList, duckLisp->memoryAllocation, sizeof(byteLink_t), dl_array_strategy_double);

	byteLink_t tempByteLink;

	// dl_array_t bytecode; // unsigned char
	/**/ dl_array_init(bytecode, duckLisp->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);

	dl_ptrdiff_t tempPtrdiff = -1;


	/* * * * * *
	 * Compile *
	 * * * * * */


	putchar('\n');

	if (astCompoundexpression.type != duckLisp_ast_type_expression) {
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Cannot compile non-expression types to bytecode."));
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}

	/* First stage: Create assembly tree from AST. */

	e = duckLisp_compile_expression(duckLisp, &assembly, DL_STR("compileAST"), &astCompoundexpression.value.expression);
	if (e) goto l_cleanup;

	// Print list.
	printf("\n");

	{
		dl_size_t tempDlSize;
		/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
		printf("Compiler memory usage (post compilation): %llu/%llu (%llu%%)\n\n",
		       tempDlSize,
		       duckLisp->memoryAllocation->size,
		       100*tempDlSize/duckLisp->memoryAllocation->size);
	}

	/* dl_array_t ia; */
	duckLisp_instructionObject_t io;
	/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < instructionList.elements_length; i++) { */
	/*		ia = DL_ARRAY_GETADDRESS(instructionList, dl_array_t, i); */
	/* printf("{\n"); */
	for (dl_ptrdiff_t j = 0; (dl_size_t) j < assembly.elements_length; j++) {
		io = DL_ARRAY_GETADDRESS(assembly, duckLisp_instructionObject_t, j);
		/* printf("{\n"); */
		printf("Instruction class: %s",
		       (char *[33]){
			       ""  // Trick the formatter.
				       "nop",
				       "push-string",
				       "push-boolean",
				       "push-integer",
				       "push-index",
				       "push-symbol",
				       "push-upvalue",
				       "push-closure",
				       "call",
				       "ccall",
				       "acall",
				       "jump",
				       "brz",
				       "brnz",
				       "move",
				       "not",
				       "mul",
				       "div",
				       "add",
				       "sub",
				       "equal",
				       "less",
				       "greater",
				       "cons",
				       "car",
				       "cdr",
				       "null?",
				       "type-of",
				       "pop",
				       "return",
				       "nil",
				       "push-label",
				       "label",
				       }[io.instructionClass]);
		/* printf("[\n"); */
		duckLisp_instructionArgClass_t ia;
		if (io.args.elements_length == 0) {
			putchar('\n');
		}
		for (dl_ptrdiff_t k = 0; (dl_size_t) k < io.args.elements_length; k++) {
			putchar('\n');
			ia = ((duckLisp_instructionArgClass_t *) io.args.elements)[k];
			/* printf("		   {\n"); */
			printf("		Type: %s",
			       (char *[4]){
				       "none",
				       "integer",
				       "index",
				       "string",
			       }[ia.type]);
			putchar('\n');
			printf("		Value: ");
			switch (ia.type) {
			case duckLisp_instructionArgClass_type_none:
				printf("None");
				break;
			case duckLisp_instructionArgClass_type_integer:
				printf("%i", ia.value.integer);
				break;
			case duckLisp_instructionArgClass_type_index:
				printf("%llu", ia.value.index);
				break;
			case duckLisp_instructionArgClass_type_string:
				printf("\"");
				for (dl_ptrdiff_t m = 0; (dl_size_t) m < ia.value.string.value_length; m++) {
					switch (ia.value.string.value[m]) {
					case '\n':
						putchar('\\');
						putchar('n');
						break;
					default:
						putchar(ia.value.string.value[m]);
					}
				}
				printf("\"");
				break;
			default:
				printf("		Undefined type.\n");
			}
			putchar('\n');
		}
		printf("\n");
		/* printf("}\n"); */
	}
	/* printf("}\n"); */
	/* } */


	/** * * * * *
	 * Assemble *
	 * * * * * **/

	byteLink_t currentInstruction;
	dl_array_t currentArgs; // unsigned char
	linkArray_t linkArray = {0};
	currentInstruction.prev = -1;
	/**/ dl_array_init(&currentArgs, duckLisp->memoryAllocation, sizeof(unsigned char), dl_array_strategy_double);
	/* for (dl_ptrdiff_t i = instructionList.elements_length - 1; i >= 0; --i) { */
	/*		dl_array_t instructions = DL_ARRAY_GETADDRESS(instructionList, dl_array_t, i); */
	/* for (dl_ptrdiff_t j = 0; (dl_size_t) j < instructions.elements_length; j++) { */
	/*		duckLisp_instructionObject_t instruction = DL_ARRAY_GETADDRESS(instructions, duckLisp_instructionObject_t, j); */
	for (dl_ptrdiff_t j = 0; (dl_size_t) j < assembly.elements_length; j++) {
		duckLisp_instructionObject_t instruction = DL_ARRAY_GETADDRESS(assembly, duckLisp_instructionObject_t, j);
		// This is OK because there is no chance of reallocating the args array.
		duckLisp_instructionArgClass_t *args = &DL_ARRAY_GETADDRESS(instruction.args,
		                                                            duckLisp_instructionArgClass_t, 0);
		dl_size_t byte_length;

		e = dl_array_clear(&currentArgs);
		if (e) {
			goto l_cleanup;
		}

		switch (instruction.instructionClass) {
		case duckLisp_instructionClass_nop: {
			// Finish later. We probably don't need it.
			currentInstruction.byte = duckLisp_instruction_nop;
			break;
		}
		case duckLisp_instructionClass_pushIndex: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_index:
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_pushIndex8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_pushIndex16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushIndex32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushBoolean: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				currentInstruction.byte = duckLisp_instruction_pushBooleanFalse + (args[0].value.integer != 0);
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) e = eError;
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushInteger: {
			dl_bool_t sign = args[0].value.integer < 0;
			unsigned long long absolute = sign ? -args[0].value.integer : args[0].value.integer;

			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				if (absolute < 0x80LU) {
					currentInstruction.byte = duckLisp_instruction_pushInteger8;
					byte_length = 1;
				}
				else if (absolute < 0x8000LU) {
					currentInstruction.byte = duckLisp_instruction_pushInteger16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushInteger32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushString: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_pushString8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_pushString16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushString32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			switch (args[1].type) {
			case duckLisp_instructionArgClass_type_string:
				e = dl_array_pushElements(&currentArgs, args[1].value.string.value, args[1].value.string.value_length);
				if (e) {
					goto l_cleanup;
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushSymbol: {
			dl_bool_t sign;
			unsigned long long absolute[2];
			sign = args[0].value.integer < 0;
			absolute[0] = sign ? -args[0].value.integer : args[0].value.integer;
			sign = args[1].value.integer < 0;
			absolute[1] = sign ? -args[1].value.integer : args[1].value.integer;
			absolute[0] = dl_max(absolute[0], absolute[1]);

			if ((args[0].type == duckLisp_instructionArgClass_type_integer) &&
			    (args[1].type == duckLisp_instructionArgClass_type_integer) &&
			    (args[2].type == duckLisp_instructionArgClass_type_string)) {
				if (absolute[0] < 0x100LU) {
					currentInstruction.byte = duckLisp_instruction_pushSymbol8;
					byte_length = 1;
				}
				else if (absolute[0] < 0x10000LU) {
					currentInstruction.byte = duckLisp_instruction_pushSymbol16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushSymbol32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n + byte_length) = ((args[1].value.integer
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				e = dl_array_pushElements(&currentArgs, args[2].value.string.value, args[2].value.string.value_length);
				if (e) {
					goto l_cleanup;
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class[es]. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushUpvalue: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				/* I'm not checking if it's negative since I'm lazy and that should never happen. */
				if (args[0].value.index < 0x100LL) {
					currentInstruction.byte = duckLisp_instruction_pushUpvalue8;
					byte_length = 1;
				}
				else if (args[0].value.index < 0x10000LL) {
					currentInstruction.byte = duckLisp_instruction_pushUpvalue16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushUpvalue32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto l_cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class[es]. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushClosure: {
			if (args[0].type == duckLisp_instructionArgClass_type_integer) {
				dl_ptrdiff_t index = 0;
				currentInstruction.byte = duckLisp_instruction_pushClosure32;
				byte_length = 4;
				// Function label
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto l_cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, l) = ((args[0].value.integer
					                                                    >> 8*(byte_length - l - 1))
					                                                   & 0xFFU);
				}
				index += byte_length;

				// Number of upvalues
				byte_length = 4;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto l_cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = (((instruction.args.elements_length - 1)
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				// Upvalues
				// I'm not going to check args here either.
				byte_length = 4;
				e = dl_array_pushElements(&currentArgs,
				                          dl_null,
				                          (instruction.args.elements_length - 1) * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t l = 1; (dl_size_t) l < instruction.args.elements_length; l++) {
					DL_DOTIMES(m, byte_length) {
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + m) = ((args[l].value.integer
						                                                            >> 8*(byte_length - m - 1))
						                                                           & 0xFFU);
					}
				}
				index += byte_length * (instruction.args.elements_length - 1);
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class[es]. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_move: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_move8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_move16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_move32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}

		case duckLisp_instructionClass_pop: {
			if (args[0].type == duckLisp_instructionArgClass_type_integer) {
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_pop8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_pop16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pop32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_not: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_not8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_not16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_not32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_mul: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_mul8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_mul16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_mul32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_div: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_div8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_div16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_div32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_add: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_add8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_add16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_add32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_sub: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_sub8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_sub16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_sub32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_equal: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_equal8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_equal16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_equal32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_greater: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_greater8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_greater16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_greater32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_less: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_less8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_less16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_less32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_cons: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_cons8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_cons16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_cons32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_car: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_car8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_car16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_car32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_cdr: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_cdr8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_cdr16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_cdr32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_nullp: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_nullp8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_nullp16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_nullp32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_typeof: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_typeof8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_typeof16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_typeof32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_nil: {
			currentInstruction.byte = duckLisp_instruction_nil;
			break;
		}
		case duckLisp_instructionClass_ccall: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_ccall8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_ccall16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_ccall32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_acall: {
			if (args[0].type == duckLisp_instructionArgClass_type_integer) {
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_acall8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_acall16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_acall32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length + 1);
				if (e) {
					goto l_cleanup;
				}
				printf("%i\n", args[0].value.integer);
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < 1; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n + 1) = args[1].value.integer & 0xFFU;
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
			// @TODO: Redo scoping. Tries in parent scopes need to be searched as well.
		case duckLisp_instructionClass_pseudo_label:
		case duckLisp_instructionClass_pushLabel:
		case duckLisp_instructionClass_call:
		case duckLisp_instructionClass_jump:
		case duckLisp_instructionClass_brnz: {
			dl_ptrdiff_t label_index = -1;
			duckLisp_label_t label;
			duckLisp_label_source_t labelSource;
			label_index = args[0].value.integer;

			// This should never fail due to the above initialization.
			label = DL_ARRAY_GETADDRESS(duckLisp->labels, duckLisp_label_t, label_index);
			// There should only be one label instruction. The rest should be branches.
			tempPtrdiff = bytecodeList.elements_length;
			if (instruction.instructionClass == duckLisp_instructionClass_pseudo_label) {
				if (label.target == -1) {
					label.target = tempPtrdiff;
				}
				else {
					e = dl_error_invalidValue;
					eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Redefinition of label."));
					if (eError) {
						e = eError;
					}
					goto l_cleanup;
				}
			}
			else if (instruction.instructionClass == duckLisp_instructionClass_pushLabel) {
				tempPtrdiff++;	// `++` for opcode. This is so the optimizer can be used with generic address links.
				labelSource.source = tempPtrdiff;
				labelSource.absolute = dl_true;
				e = dl_array_pushElement(&label.sources, &labelSource);
				if (e) {
					goto l_cleanup;
				}
				linkArray.links_length++;
			}
			else {
				tempPtrdiff++;	// `++` for opcode. This is so the optimizer can be used with generic address links.
				labelSource.source = tempPtrdiff;
				labelSource.absolute = dl_false;
				e = dl_array_pushElement(&label.sources, &labelSource);
				if (e) {
					goto l_cleanup;
				}
				linkArray.links_length++;
			}
			DL_ARRAY_GETADDRESS(duckLisp->labels, duckLisp_label_t, label_index) = label;

			if (instruction.instructionClass == duckLisp_instructionClass_pseudo_label) {
				continue;
			}
			if ((instruction.instructionClass == duckLisp_instructionClass_brnz)
			    || (instruction.instructionClass == duckLisp_instructionClass_call)
			    || (instruction.instructionClass == duckLisp_instructionClass_pushLabel)) {
				switch (instruction.instructionClass) {
				case duckLisp_instructionClass_pushLabel:
					currentInstruction.byte = duckLisp_instruction_pushInteger32;
					break;
				case duckLisp_instructionClass_brnz:
					currentInstruction.byte = duckLisp_instruction_brnz8;
					break;
				case duckLisp_instructionClass_call:
					currentInstruction.byte = duckLisp_instruction_call8;
					break;
				default:
					e = dl_error_invalidValue;
					goto l_cleanup;
				}

				if (instruction.instructionClass != duckLisp_instructionClass_pushLabel) {
					// br?? also have a pop argument. Insert that.
					e = dl_array_pushElements(&currentArgs, dl_null, 1);
					if (e) {
						goto l_cleanup;
					}
					for (dl_ptrdiff_t n = 0; (dl_size_t) n < 1; n++) {
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = args[1].value.integer & 0xFFU;
					}
				}
			}
			else {
				// First guess: Jump is < 128 B away.
				currentInstruction.byte = duckLisp_instruction_jump8;
			}
			break;
		}
		case duckLisp_instructionClass_return: {
			if (args[0].type == duckLisp_instructionArgClass_type_integer) {
				if (!args[0].value.integer) {
					currentInstruction.byte = duckLisp_instruction_return0;
					byte_length = 0;
				}
				else if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_return8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_return16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_return32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto l_cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid instruction class. Aborting."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		}

		// Write instruction.
		if (bytecodeList.elements_length > 0) {
			DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = bytecodeList.elements_length;
		}
		currentInstruction.prev = bytecodeList.elements_length - 1;
		e = dl_array_pushElement(&bytecodeList, &currentInstruction);
		// e = dl_array_pushElement(bytecode, dl_null);
		if (e) {
			if (bytecodeList.elements_length > 0) DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = -1;
			goto l_cleanup;
		}
		// DL_ARRAY_GETTOPADDRESS(*bytecode, dl_uint8_t) = currentInstruction.byte & 0xFFU;
		for (dl_ptrdiff_t k = 0; (dl_size_t) k < currentArgs.elements_length; k++) {
			tempByteLink.byte = DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, k);
			DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = bytecodeList.elements_length;
			tempByteLink.prev = bytecodeList.elements_length - 1;
			e = dl_array_pushElement(&bytecodeList, &tempByteLink);
			if (e) {
				if (bytecodeList.elements_length > 0) DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = -1;
				goto l_cleanup;
			}
		}
	}
	/* } */
	if (bytecodeList.elements_length > 0) {
		DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = -1;
	}

	{
		dl_size_t tempDlSize;
		/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
		printf("Compiler memory usage (post assembly): %llu/%llu (%llu%%)\n",
		       tempDlSize,
		       duckLisp->memoryAllocation->size,
		       100*tempDlSize/duckLisp->memoryAllocation->size);
	}

	// Resolve jumps here.

	if (linkArray.links_length) {
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &linkArray.links,
		              linkArray.links_length * sizeof(jumpLink_t));
		if (e) goto l_cleanup;

		{
			dl_ptrdiff_t index = 0;
			for (dl_ptrdiff_t i = 0; (dl_size_t) i < duckLisp->labels.elements_length; i++) {
				duckLisp_label_t label = DL_ARRAY_GETADDRESS(duckLisp->labels, duckLisp_label_t, i);
				for (dl_ptrdiff_t j = 0; (dl_size_t) j < label.sources.elements_length; j++) {
					linkArray.links[index].target = label.target;
					linkArray.links[index].source = DL_ARRAY_GETADDRESS(label.sources,
					                                                    duckLisp_label_source_t,
					                                                    j).source;
					linkArray.links[index].absolute = DL_ARRAY_GETADDRESS(label.sources,
					                                                      duckLisp_label_source_t,
					                                                      j).absolute;
					linkArray.links[index].size = 0;
					linkArray.links[index].forward = label.target > linkArray.links[index].source;
					index++;
				}
				e = dl_array_quit(&label.sources);
				if (e) goto l_cleanup;
			}
		}

		putchar('\n');

#if 1
		/* Address has been set.
		   Target has been set.*/

		/* Create a copy of the original linkArray. This gives us a one-to-one mapping of the new goto addresses to the
		   current goto addresses. */

		linkArray_t newLinkArray;
		newLinkArray.links_length = linkArray.links_length;
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &newLinkArray.links,
		              newLinkArray.links_length * sizeof(jumpLink_t));
		if (e) goto l_cleanup;

		/**/ dl_memcopy_noOverlap(newLinkArray.links, linkArray.links, linkArray.links_length * sizeof(jumpLink_t));

		/* Create array double the size as jumpLink. */

		jumpLinkPointer_t *jumpLinkPointers = dl_null;
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &jumpLinkPointers,
		              2 * linkArray.links_length * sizeof(jumpLinkPointer_t));
		if (e) goto l_cleanup;


		/* Fill array with each jumpLink index and index type. */
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			jumpLinkPointers[i].index = i;
			jumpLinkPointers[i].type = jumpLinkPointers_type_address;
		}

		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			jumpLinkPointers[i + linkArray.links_length].index = i;
			jumpLinkPointers[i + linkArray.links_length].type = jumpLinkPointers_type_target;
		}

		/* I suspect a simple linked list would have been faster than all this junk. */

		/**/ quicksort_hoare(jumpLinkPointers,
		                     2 * linkArray.links_length,
		                     sizeof(jumpLinkPointer_t),
		                     0,
		                     2 * linkArray.links_length - 1,
		                     jumpLink_less,
		                     &linkArray);

		for (dl_ptrdiff_t i = 0; (dl_size_t) i < 2 * linkArray.links_length; i++) {
			printf("%lld %c		 ", jumpLinkPointers[i].index, jumpLinkPointers[i].type ? 't' : 's');
		}
		putchar('\n');
		putchar('\n');

		/* Optimize addressing size. */

		dl_ptrdiff_t offset;
		do {
			printf("\n");
			offset = 0;
			for (dl_ptrdiff_t j = 0; (dl_size_t) j < 2 * newLinkArray.links_length; j++) {
				unsigned char newSize;
				dl_ptrdiff_t difference;
				dl_ptrdiff_t index = jumpLinkPointers[j].index;
				jumpLink_t link = newLinkArray.links[index];

				// Make sure to check for duplicate links.
				// ^^^ Make sure to ignore that. They are not duplicates.
				// They need to point to the original links so that the originals can be updated.
				// This means I should have created a member that points to the original struct.

				/*
				  Required structs:
				  goto-label struct. Has a single label and multiple gotos. Possibly superfluous. Done.
				  Original jump link struct. Saved so that the bytecode addresses can be updated. Done.
				  Malleable jump link struct. Scratchpad and final result of calculation. Done.
				  Link pointer struct. Sorted so that malleable links can be updated in order. Done.
				*/

				/* jumpLink[i].address += offset; */

				if (jumpLinkPointers[j].type == jumpLinkPointers_type_target) {
					link.target += offset;
					printf("t %lli	index l%lli		 offset %lli\n", link.target, index, offset);
				}
				else {
					link.source += offset;

					/* Range calculation */
					difference = link.target - (link.source + link.size);

					/* Size calculation */
					if ((DL_INT8_MAX >= difference) && (difference >= DL_INT8_MIN)) {
						newSize = 1;  /* +1 for opcode. */
					}
					else if ((DL_INT16_MAX >= difference) && (difference >= DL_INT16_MIN)) {
						newSize = 2;
					}
					else {
						newSize = 4;
					}
					if (link.absolute) newSize = 4;

					printf("t %lli	index j%lli		 offset %lli  difference %lli  size %u	newSize %u\n",
					       link.source,
					       index,
					       offset,
					       difference,
					       link.size,
					       newSize);
					if (newSize > link.size) {
						offset += newSize - link.size;
						link.size = newSize;
					}
				}
				newLinkArray.links[index] = link;
			}
		} while (offset != 0);
		putchar('\n');

		printf("old: ");
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			printf("%s%lli⇒%lli ; ",
			       linkArray.links[i].forward ? "f" : "",
			       linkArray.links[i].source,
			       linkArray.links[i].target);
		}
		printf("\n");

		printf("new: ");
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < newLinkArray.links_length; i++) {
			printf("%s%lli⇒%lli ; ",
			       newLinkArray.links[i].forward ? "f" : "",
			       newLinkArray.links[i].source,
			       newLinkArray.links[i].target);
		}
		printf("\n\n");


		/* Insert addresses into bytecode. */

		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			/* The bytecode list is a linked list, but there is no problem
			   addressing it as an array since the links were inserted
			   in order. Additional links will be placed on the end of
			   the array and will not affect the indices of the earlier links. */

			// ` - 1` because we want to insert the links *in place of* the target link.
			dl_ptrdiff_t base_address = linkArray.links[i].source - 1;
			dl_bool_t array_end = DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).next == -1;

			if (!newLinkArray.links[i].absolute) {
				if (newLinkArray.links[i].size == 1) {
				}
				else if (newLinkArray.links[i].size == 2) {
					// This is sketch.
					DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).byte += 1;
				}
				else {
					DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).byte += 2;
				}
			}

			dl_ptrdiff_t previous = base_address;
			for (dl_uint8_t j = 1; j <= newLinkArray.links[i].size; j++) {
				byteLink_t byteLink;
				if (newLinkArray.links[i].absolute) {
					byteLink.byte = (newLinkArray.links[i].target >> 8*(newLinkArray.links[i].size - j)) & 0xFFU;
				}
				else {
					byteLink.byte = (((newLinkArray.links[i].target
					                   - (newLinkArray.links[i].source + newLinkArray.links[i].size))
					                  >> 8*(newLinkArray.links[i].size - j))
					                 & 0xFFU);
				}
				byteLink.prev = previous;
				byteLink.next = linkArray.links[i].source;

				DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, previous).next = bytecodeList.elements_length;
				DL_ARRAY_GETADDRESS(bytecodeList,
				                    byteLink_t,
				                    linkArray.links[i].source).prev = bytecodeList.elements_length;

				e = dl_array_pushElement(&bytecodeList, &byteLink);
				if (e) goto l_cleanup;

				previous = bytecodeList.elements_length - 1;
			}

			if (array_end) DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, bytecodeList.elements_length - 1).next = -1;
		}

#endif

		{
			dl_size_t tempDlSize;
			/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
			printf("Compiler memory usage (mid fixups): %llu/%llu (%llu%%)\n\n",
			       tempDlSize,
			       duckLisp->memoryAllocation->size,
			       100*tempDlSize/duckLisp->memoryAllocation->size);
		}

		// Clean up, clean up, everybody do your share…
		e = dl_free(duckLisp->memoryAllocation, (void *) &newLinkArray.links);
		if (e) goto l_cleanup;
		e = dl_free(duckLisp->memoryAllocation, (void *) &linkArray.links);
		if (e) goto l_cleanup;
		e = dl_free(duckLisp->memoryAllocation, (void *) &jumpLinkPointers);
		if (e) goto l_cleanup;
	} /* End address space optimization. */

	// Adjust the opcodes for the address size and set address.
	// i.e. rewrite the whole instruction.

	{
		dl_size_t tempDlSize;
		/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
		printf("Compiler memory usage (post fixups): %llu/%llu (%llu%%)\n\n",
		       tempDlSize,
		       duckLisp->memoryAllocation->size,
		       100*tempDlSize/duckLisp->memoryAllocation->size);
	}

	// Convert bytecodeList to array.
	if (bytecodeList.elements_length > 0) {
		tempByteLink.next = 0;
		while (tempByteLink.next != -1) {
			/* if (tempByteLink.next == 193) break; */
			tempByteLink = DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, tempByteLink.next);
			e = dl_array_pushElement(bytecode, &tempByteLink.byte);
			if (e) {
				goto l_cleanup;
			}
		}
	}

	// Push a return instruction.
	dl_uint8_t tempChar = duckLisp_instruction_return0;
	e = dl_array_pushElement(bytecode, &tempChar);
	if (e) {
		goto l_cleanup;
	}

	{
		dl_size_t tempDlSize;
		/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
		printf("Compiler memory usage (post bytecode write): %llu/%llu (%llu%%)\n",
		       tempDlSize,
		       duckLisp->memoryAllocation->size,
		       100*tempDlSize/duckLisp->memoryAllocation->size);
	}

	/* * * * * *
	 * Cleanup *
	 * * * * * */

 l_cleanup:

	putchar('\n');

	eError = dl_array_quit(&bytecodeList);
	if (eError) {
		e = eError;
	}

	DL_ARRAY_FOREACH(tempInstructionObject, assembly, {
			e = dl_array_e;
			break;
		}, {
			if (tempInstructionObject.args.elements_length) {
				eError = dl_array_quit(&tempInstructionObject.args);
				if (eError) {
					e = eError;
					break;
				}
			}
		})

		eError = dl_array_quit(&assembly);
	if (eError) {
		e = eError;
	}

	eError = dl_array_quit(&expressionStack);
	if (eError) {
		e = eError;
	}

	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}

	return e;
}

/*
  ================
  Public functions
  ================
*/

dl_error_t duckLisp_init(duckLisp_t *duckLisp) {
	dl_error_t error = dl_error_ok;

	// All language-defined generators go here.
	struct {
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*);
	} generators[] = {{DL_STR("comment"),      duckLisp_generator_comment},
	                  {DL_STR(";"),            duckLisp_generator_comment},
	                  {DL_STR("nop"),          duckLisp_generator_nop},
	                  {DL_STR("goto"),         duckLisp_generator_goto},
	                  {DL_STR("call"),         duckLisp_generator_acall},
	                  {DL_STR("label"),        duckLisp_generator_label},
	                  {DL_STR("var"),          duckLisp_generator_createVar},
	                  {DL_STR("setq"),         duckLisp_generator_setq},
	                  {DL_STR("\xE2\x86\x90"), duckLisp_generator_setq},
	                  {DL_STR("not"),          duckLisp_generator_not},
	                  {DL_STR("*"),            duckLisp_generator_multiply},
	                  {DL_STR("/"),            duckLisp_generator_divide},
	                  {DL_STR("+"),            duckLisp_generator_add},
	                  {DL_STR("-"),            duckLisp_generator_sub},
	                  {DL_STR("while"),        duckLisp_generator_while},
	                  {DL_STR("if"),           duckLisp_generator_if},
	                  {DL_STR("when"),         duckLisp_generator_when},
	                  {DL_STR("unless"),       duckLisp_generator_unless},
	                  {DL_STR("brnz"),         duckLisp_generator_brnz},
	                  {DL_STR("="),            duckLisp_generator_equal},
	                  {DL_STR("<"),            duckLisp_generator_less},
	                  {DL_STR(">"),            duckLisp_generator_greater},
	                  {DL_STR("defun"),        duckLisp_generator_defun},
	                  {DL_STR("constexpr"),    duckLisp_generator_constexpr},
	                  {DL_STR("noscope"),      duckLisp_generator_noscope},
	                  {DL_STR("quote"),        duckLisp_generator_quote},
	                  {DL_STR("list"),         duckLisp_generator_list},
	                  {DL_STR("cons"),         duckLisp_generator_cons},
	                  {DL_STR("car"),          duckLisp_generator_car},
	                  {DL_STR("cdr"),          duckLisp_generator_cdr},
	                  {DL_STR("null?"),        duckLisp_generator_nullp},
	                  {DL_STR("type-of"),      duckLisp_generator_typeof},
	                  {dl_null, 0,             dl_null}};

	/* No error */ dl_array_init(&duckLisp->source, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_fit);
	// /* No error */ cst_expression_init(&duckLisp->cst);
	// /* No error */ ast_expression_init(&duckLisp->ast);
	/* No error */ dl_array_init(&duckLisp->errors,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_error_t),
	                             dl_array_strategy_fit);
	/* /\* No error *\/ dl_array_init(&duckLisp->stack, */
	/*                              &duckLisp->memoryAllocation, */
	/*                              sizeof(duckLisp_object_t), */
	/*                              dl_array_strategy_double); */
	/* No error */ dl_array_init(&duckLisp->scope_stack,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_scope_t),
	                             dl_array_strategy_fit);
	/* No error */ dl_array_init(&duckLisp->generators_stack,
	                             duckLisp->memoryAllocation,
	                             sizeof(dl_error_t (*)(duckLisp_t*, duckLisp_ast_expression_t*)),
	                             dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->labels,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_label_t),
	                             dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->symbols_array,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_ast_identifier_t),
	                             dl_array_strategy_double);
	/* No error */ dl_trie_init(&duckLisp->symbols_trie, duckLisp->memoryAllocation, -1);

	duckLisp->locals_length = 0;
	duckLisp->statics_length = 0;
	duckLisp->gensym_number = 0;

	{
		duckLisp_scope_t scope;
		/**/ scope_init(duckLisp, &scope, dl_false);
		error = dl_array_pushElement(&duckLisp->scope_stack, &scope);
		if (error) goto cleanup;
	}

	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		error = duckLisp_addGenerator(duckLisp, generators[i].callback, generators[i].name, generators[i].name_length);
		if (error) {
			printf("Could not register generator. (%s)\n", dl_errorString[error]);
			goto cleanup;
		}
	}

	cleanup:

	return error;
}

void duckLisp_quit(duckLisp_t *duckLisp) {
	(void)duckLisp;  // @TODO: See all this memory being freed? Neither do I.
}

dl_error_t duckLisp_cst_print(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t cst) {
	dl_error_t e = dl_error_ok;

	e = cst_print_compoundExpression(*duckLisp, cst);
	putchar('\n');
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
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

/*
  Creates a function from a string in the current scope.
*/
dl_error_t duckLisp_loadString(duckLisp_t *duckLisp,
                               unsigned char **bytecode,
                               dl_size_t *bytecode_length,
                               char *source,
                               const dl_size_t source_length) {
	dl_error_t e = dl_error_ok;
	// struct {
	//		dl_bool_t duckLispMemory;
	// } d = {0};

	dl_ptrdiff_t index = -1;
	duckLisp_ast_compoundExpression_t ast;
	duckLisp_cst_compoundExpression_t cst;
	dl_array_t bytecodeArray;
	dl_bool_t result = dl_false;

	/**/ cst_compoundExpression_init(&cst);
	/**/ ast_compoundExpression_init(&ast);

	// Trim whitespace from the beginning of the file.
	do {
		/**/ dl_string_isSpace(&result, *source);
		if (result) {
			source++;
		}
	} while (result);

	index = duckLisp->source.elements_length;

	e = dl_array_pushElements(&duckLisp->source, source, source_length);
	if (e) {
		goto l_cleanup;
	}

	/* Parse. */

	e = duckLisp_cst_append(duckLisp, &cst, index, dl_true);
	if (e) {
		goto l_cleanup;
	}

	e = duckLisp_ast_append(duckLisp, &ast, &cst, index, dl_true);
	if (e) {
		goto l_cleanup;
	}

	// printf("CST: ");
	// e = cst_print_compoundExpression(*duckLisp, cst); putchar('\n');
	// if (e) {
	//		goto l_cleanup;
	// }
	e = cst_compoundExpression_quit(duckLisp, &cst);
	if (e) {
		goto l_cleanup;
	}
	printf("AST: ");
	e = ast_print_compoundExpression(*duckLisp, ast); putchar('\n');
	if (e) {
		goto l_cleanup;
	}

	{
		dl_size_t tempDlSize;
		/**/ dl_memory_usage(&tempDlSize, *duckLisp->memoryAllocation);
		printf("\nCompiler memory usage: %llu/%llu (%llu%%)",
		       tempDlSize,
		       duckLisp->memoryAllocation->size,
		       100*tempDlSize/duckLisp->memoryAllocation->size);
	}

	/* Compile AST to bytecode. */

	e = duckLisp_compileAST(duckLisp, &bytecodeArray, ast);
	if (e) {
		goto l_cleanup;
	}

	e = ast_compoundExpression_quit(duckLisp, &ast);
	if (e) {
		goto l_cleanup;
	}

	*bytecode = ((unsigned char*) (bytecodeArray).elements);
	*bytecode_length = bytecodeArray.elements_length;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_scope_addObject(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	// Stick name and index in the current scope's trie.
	e = scope_getTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

	e = dl_trie_insert(&scope.locals_trie, name, name_length, duckLisp->locals_length);
	if (e) {
		goto l_cleanup;
	}
	/* duckLisp->locals_length++; */

	e = scope_setTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_addInterpretedFunction(duckLisp_t *duckLisp, const duckLisp_ast_identifier_t name) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	// Stick name and index in the current scope's trie.
	e = scope_getTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

	// Record function type in function trie.
	e = dl_trie_insert(&scope.functions_trie, name.value, name.value_length, duckLisp_functionType_ducklisp);
	if (e) {
		goto l_cleanup;
	}
	// So simple. :)

	e = scope_setTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_addGenerator(duckLisp_t *duckLisp,
                                 dl_error_t (*callback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*),
                                 const char *name,
                                 const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	// Stick name and index in the current scope's trie.
	e = scope_getTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

	// Record function type in function trie.
	e = dl_trie_insert(&scope.functions_trie, name, name_length, duckLisp_functionType_generator);
	if (e) {
		goto l_cleanup;
	}
	// Record the generator stack index.
	e = dl_trie_insert(&scope.generators_trie, name, name_length, duckLisp->generators_stack.elements_length);
	if (e) {
		goto l_cleanup;
	}
	scope.generators_length++;
	e = dl_array_pushElement(&duckLisp->generators_stack, &callback);
	if (e) {
		goto l_cleanup;
	}

	e = scope_setTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}

dl_error_t duckLisp_linkCFunction(duckLisp_t *duckLisp,
                                  dl_ptrdiff_t *index,
                                  const char *name,
                                  const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_scope_t scope;

	// Stick name and index in the current scope's trie.
	e = scope_getTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

	// Record function type in function trie.
	e = dl_trie_insert(&scope.functions_trie, name, name_length, duckLisp_functionType_c);
	if (e) {
		goto l_cleanup;
	}
	// Record the VM stack index.
	e = dl_trie_insert(&scope.statics_trie, name, name_length, duckLisp->statics_length);
	if (e) {
		goto l_cleanup;
	}
	*index = duckLisp->statics_length;
	duckLisp->statics_length++;

	e = scope_setTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

 l_cleanup:

	return e;
}


char *duckLisp_disassemble(dl_memoryAllocation_t *memoryAllocation,
                           const dl_uint8_t *bytecode,
                           const dl_size_t length) {
	dl_error_t e = dl_error_ok;

	dl_array_t disassembly;
	/**/ dl_array_init(&disassembly, memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_uint8_t opcode;
	dl_ptrdiff_t arg = 0;
	dl_uint8_t tempChar;
	dl_size_t tempSize;
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < length; i++) {
		if (arg == 0) opcode = bytecode[i];

		switch (opcode) {
		case duckLisp_instruction_nop:
			e = dl_array_pushElements(&disassembly, DL_STR("nop\n"));
			if (e) return dl_null;
			arg = 0;
			continue;

		case duckLisp_instruction_pushString8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-string.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '"';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			default:
				if (tempSize > 0) {

					if (bytecode[i] == '\n') {
						e = dl_array_pushElements(&disassembly, DL_STR("\\n"));
						if (e) return dl_null;
					}
					else {
						e = dl_array_pushElement(&disassembly, (dl_uint8_t *) &bytecode[i]);
						if (e) return dl_null;
					}
					--tempSize;
					if (!tempSize) {
						tempChar = '"';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = '\n';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushSymbol8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-symbol.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '"';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			default:
				if (tempSize > 0) {

					if (bytecode[i] == '\n') {
						e = dl_array_pushElements(&disassembly, DL_STR("\\n"));
						if (e) return dl_null;
					}
					else {
						e = dl_array_pushElement(&disassembly, (dl_uint8_t *) &bytecode[i]);
						if (e) return dl_null;
					}
					--tempSize;
					if (!tempSize) {
						tempChar = '"';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = '\n';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushBooleanFalse:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-boolean-false"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushBooleanTrue:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-boolean-true"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushInteger8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-integer.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushInteger16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-integer.16 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushInteger32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-integer.32 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushIndex8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-index.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushUpvalue8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-upvalue.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushUpvalue16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-upvalue.16 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushUpvalue32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-upvalue.32 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushClosure32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-closure.32 "));
				if (e) return dl_null;
				break;
			case 1:
				// Function address
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 3;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 2;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 1;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) tempChar = '\n';
				else tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) {
					arg = 0;
					continue;
				}
				break;
			default:
				// Upvalues
				if (tempSize-- > 0) {
					DL_DOTIMES(m, 4) {
						tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						if (m != (4 - 1)) i++;
					}
					if (tempSize == 0) tempChar = '\n';
					else tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					if (tempSize == 0) {
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_call8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("call.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_call16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("call.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_call32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("call.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_acall8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("acall.8         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_acall16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("acall.16        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_acall32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("acall.32        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_ccall8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("c-call.8        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_brnz8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("brnz.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_brnz16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("brnz.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_brnz32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("brnz.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_jump8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("jump.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_jump16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("jump.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_jump32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("jump.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_move8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("move.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_move16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("move.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_move32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("move.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pop8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("pop.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pop16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("pop.16          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pop32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("pop.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_not8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("not.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_not16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("not.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_not32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("not.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_add8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("add.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_add16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("add.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_add32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("add.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_mul8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("mul.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_mul16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("mul.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_mul32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("mul.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_div8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("div.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_div16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("div.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_div32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("div.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_sub8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("sub.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_sub16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("sub.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_sub32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("sub.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_equal8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("equal.8				"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_equal16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("equal.16	   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_equal32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("equal.32		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_greater8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("greater.8		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_greater16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("greater.16	   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_greater32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("greater.32		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_less8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("less.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_less16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("less.16			   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_less32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("less.32				"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_cons8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cons.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cons16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cons.16			   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cons32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cons.32				"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_car8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("car.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_car16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("car.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_car32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("car.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_cdr8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cdr.8			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cdr16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cdr.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cdr32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cdr.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_nullp8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("null?.8				"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_nullp16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("null?.16	   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_nullp32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("null?.32		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_typeof8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("type-of.8		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_typeof16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("type-of.16	   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_typeof32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("type-of.32		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_nil:
			e = dl_array_pushElements(&disassembly, DL_STR("nil\n"));
			if (e) return dl_null;
			arg = 0;
			continue;

		case duckLisp_instruction_return0:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.0"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_return8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.8        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_return16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.16		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_return32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.32		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		default:
			e = dl_array_pushElements(&disassembly, DL_STR("Illegal opcode '"));
			if (e) return dl_null;
			tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
			tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
			tempChar = '\'';
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
			tempChar = '\n';
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
		}
		arg++;
	}

	e = dl_realloc(memoryAllocation, &disassembly.elements, disassembly.elements_length * disassembly.element_size);
	if (e) {
		return dl_null;
	}

	e = dl_array_pushElements(&disassembly, DL_STR("\0"));
	if (e) return dl_null;
	return ((char *) disassembly.elements);
}
