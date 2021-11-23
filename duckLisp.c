
#include "duckLisp.h"
#include "DuckLib/string.h"
#include <stdio.h>

/*
===============
Error reporting
===============
*/

static dl_error_t duckLisp_error_pushSyntax(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length, const dl_ptrdiff_t index,
                                      dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	
	duckLisp_error_t error;
	
	if (!throwErrors) {
		goto l_cleanup;
	}
	
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) &error.message, message_length * sizeof(char));
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
	
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) &error.message, message_length * sizeof(char));
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

dl_error_t duckLisp_checkArgsAndReportError(duckLisp_t *duckLisp, duckLisp_ast_expression_t astExpression, const dl_size_t numArgs) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_array_t string;
	char *fewMany = dl_null;
	dl_size_t fewMany_length = 0;
	
	/**/ dl_array_init(&string, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	if (astExpression.compoundExpressions_length == 0) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if (astExpression.compoundExpressions[0].type != ast_compoundExpression_type_identifier) {
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
		
		e = dl_array_pushElements(&string, DL_STR("arguments for function \""));
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
		
		e = duckLisp_error_pushRuntime(duckLisp, (char *) string.elements, string.elements_length * string.element_size);
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

static void cst_compoundExpression_init(duckLisp_cst_compoundExpression_t *compoundExpression);
static dl_error_t cst_parse_compoundExpression(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors);
static dl_error_t cst_print_compoundExpression(duckLisp_t duckLisp, duckLisp_cst_compoundExpression_t compoundExpression);
static dl_error_t ast_generate_compoundExpression(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression,
                                                  duckLisp_cst_compoundExpression_t compoundExpressionCST, dl_bool_t throwErrors);
static dl_error_t ast_print_compoundExpression(duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression);

static void cst_isIdentifierSymbol(dl_bool_t *result, const char character) {
	switch (character) {
	case '~':
	case '`':
	case '!':
	case '@':
	case '$':
	case '%':
	case '^':
	case '&':
	case '*':
	case '_':
	case '-':
	case '+':
	case '=':
	case '[':
	case '{':
	case ']':
	case '}':
	case '|':
	case '\\':
	case ':':
	case ';':
	case '<':
	case ',':
	case '>':
	case '.':
	case '?':
	case '/':
		*result = dl_true;
		break;
	default:
		*result = dl_false;
	}
}


static void cst_expression_init(duckLisp_cst_expression_t *expression) {
	expression->compoundExpressions = dl_null;
	expression->compoundExpressions_length = 0;
}

static dl_error_t cst_expression_quit(duckLisp_t *duckLisp, duckLisp_cst_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	
	expression->compoundExpressions_length = 0;
	
	e = dl_free(&duckLisp->memoryAllocation, (void **) &expression);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t cst_parse_expression(duckLisp_t *duckLisp, duckLisp_cst_expression_t *expression, char *source, const dl_ptrdiff_t start_index,
                                       const dl_size_t length, dl_bool_t throwErrors) {
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
	dl_bool_t justPushed = dl_false;
	dl_bool_t wasWhitespace = dl_false;
	
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
	
	/* No error */ dl_array_init(&bracketStack, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	d.bracketStack = dl_true;
	
	child_start_index = index;
	
	while (index < stop_index) {
		
		/**/ dl_string_isSpace(&tempBool, source[index]);
		if ((bracketStack.elements_length == 0) && ((!tempBool && wasWhitespace) || justPopped)) {
			// Set start index.
			child_start_index = index;
		}
		wasWhitespace = tempBool;
		
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
					eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("No open parenthesis for closing parenthesis."), index, throwErrors);
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
				eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("No open parenthesis for closing parenthesis."), index, throwErrors);
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
		
		/**/ dl_string_isSpace(&tempBool, source[index]);
		if ((bracketStack.elements_length == 0) && ((tempBool && !wasWhitespace) || justPopped || (index >= stop_index))) {	
			// if (index >= stop_index) {
			// 	eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("File ended in expression or string brackets."), index);
			// 	e = eError ? eError : dl_error_invalidValue;
			// 	goto l_cleanup;
			// }
			
			if (index >= stop_index) {
				tempBool = dl_true;
			}
			else {
				/**/ dl_string_isSpace(&tempBool, source[index]);
			}
			if (tempBool) {
				child_length = index - child_start_index;
				
				// We have you now!
				
				// Allocate an expression.
				e = dl_realloc(&duckLisp->memoryAllocation, (void **) &expression->compoundExpressions,
				               (expression->compoundExpressions_length + 1) * sizeof(duckLisp_cst_compoundExpression_t));
				if (e) {
					goto l_cleanup;
				}
				/**/ cst_compoundExpression_init(&expression->compoundExpressions[expression->compoundExpressions_length]);
				
				// putchar('[');
				// for (dl_size_t i = child_start_index; i < child_start_index + child_length; i++) {
				// 	putchar(source[i]);
				// }
				// putchar(']');
				// putchar('\n');
				
				e = cst_parse_compoundExpression(duckLisp, &expression->compoundExpressions[expression->compoundExpressions_length], source, child_start_index,
				                                 child_length, throwErrors);
				if (e) {
					goto l_cleanup;
				}
				
				expression->compoundExpressions_length++;
			}
		}
	}
	
	if (bracketStack.elements_length != 0) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("No closing parenthesis for opening parenthesis."), index, throwErrors);
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

static void ast_expression_init(duckLisp_ast_expression_t *expression) {
	expression->compoundExpressions = dl_null;
	expression->compoundExpressions_length = 0;
}

static dl_error_t ast_expression_quit(duckLisp_t *duckLisp, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	
	expression->compoundExpressions_length = 0;
	
	e = dl_free(&duckLisp->memoryAllocation, (void **) &expression);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t ast_generate_expression(duckLisp_t *duckLisp, duckLisp_ast_expression_t *expression,
                                       const duckLisp_cst_expression_t expressionCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) &expression->compoundExpressions,
	              expressionCST.compoundExpressions_length * sizeof(duckLisp_ast_compoundExpression_t));
	if (e) {
		goto l_cleanup;
	}
	
	for (dl_size_t i = 0; i < expressionCST.compoundExpressions_length; i++) {
		e = ast_generate_compoundExpression(duckLisp, &expression->compoundExpressions[i], expressionCST.compoundExpressions[i], throwErrors);
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


static void cst_identifier_init(duckLisp_cst_identifier_t *identifier) {
	identifier->token_index = 0;
	identifier->token_length = 0;
}

static void cst_identifier_quit(duckLisp_t *duckLisp, duckLisp_cst_identifier_t *identifier) {
	identifier->token_index = 0;
	identifier->token_length = 0;
}

static dl_error_t cst_parse_identifier(duckLisp_t *duckLisp, duckLisp_cst_identifier_t *identifier, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	if (index >= stop_index) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Unexpected end of file in identifier."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	/* No error */ dl_string_isAlpha(&tempBool, source[index]);
	if (!tempBool) {
		/* No error */ cst_isIdentifierSymbol(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a alpha or allowed symbol in identifier."), index, throwErrors);
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
					eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a alpha, digit, or allowed symbol in identifier."), index, throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}
			}
		}
		index++;
	}
	
	identifier->token_index = start_index;
	identifier->token_length = length;
	
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

static void ast_identifier_init(duckLisp_ast_identifier_t *identifier) {
	identifier->value = dl_null;
	identifier->value_length = 0;
}

static dl_error_t ast_identifier_quit(duckLisp_t *duckLisp, duckLisp_ast_identifier_t *identifier) {
	dl_error_t e = dl_error_ok;
	
	identifier->value_length = 0;
	
	e = dl_free(&duckLisp->memoryAllocation, (void **) &identifier->value);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t ast_generate_identifier(duckLisp_t *duckLisp, duckLisp_ast_identifier_t *identifier,
                                               const duckLisp_cst_identifier_t identifierCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	identifier->value_length = 0;
	
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) &identifier->value, identifierCST.token_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}
	
	/**/ dl_memcopy_noOverlap(identifier->value, &((char *) duckLisp->source.elements)[identifierCST.token_index], identifierCST.token_length);
	
	identifier->value_length = identifierCST.token_length;
	
	l_cleanup:
	
	return e;
}

static void ast_print_identifier(duckLisp_t duckLisp, duckLisp_ast_identifier_t identifier) {
	if (identifier.value_length == 0) {
		printf("{NULL}");
		return;
	}
	
	for (dl_size_t i = 0; i < identifier.value_length; i++) {
		putchar(identifier.value[i]);
	}
}


static void cst_bool_init(duckLisp_cst_bool_t *boolean) {
	boolean->token_length = 0;
	boolean->token_index = 0;
}

static void cst_bool_quit(duckLisp_t *duckLisp, duckLisp_cst_bool_t *boolean) {
	boolean->token_index = 0;
	boolean->token_length = 0;
}

static dl_error_t cst_parse_bool(duckLisp_t *duckLisp, duckLisp_cst_bool_t *boolean, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	/* No error */ dl_string_compare(&tempBool, &source[start_index], length, DL_STR("true"));
	if (!tempBool) {
		/* No error */ dl_string_compare(&tempBool, &source[start_index], length, DL_STR("false"));
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a \"true\" or \"false\" in boolean."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}
	
	boolean->token_index = start_index;
	boolean->token_length = length;
	
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

static void ast_bool_init(duckLisp_ast_bool_t *boolean) {
	boolean->value = dl_false;
}

static void ast_bool_quit(duckLisp_t *duckLisp, duckLisp_ast_bool_t *boolean) {
	boolean->value = dl_false;
}

static dl_error_t ast_generate_bool(duckLisp_t *duckLisp, duckLisp_ast_bool_t *boolean,
                                               const duckLisp_cst_bool_t booleanCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	e = dl_string_toBool(&boolean->value, &((char *) duckLisp->source.elements)[booleanCST.token_index], booleanCST.token_length);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Could not convert token to bool."), booleanCST.token_index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static void ast_print_bool(duckLisp_t duckLisp, duckLisp_ast_bool_t boolean) {
	printf("%s", boolean.value ? "true" : "false");
}


static void cst_int_init(duckLisp_cst_integer_t *integer) {
	integer->token_length = 0;
	integer->token_index = 0;
}

static void cst_int_quit(duckLisp_t *duckLisp, duckLisp_cst_integer_t *integer) {
	integer->token_index = 0;
	integer->token_length = 0;
}

static dl_error_t cst_parse_int(duckLisp_t *duckLisp, duckLisp_cst_integer_t *integer, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
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
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Unexpected end of file in integer."), index, throwErrors);
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
	
	integer->token_index = start_index;
	integer->token_length = length;
	
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

static void ast_int_init(duckLisp_ast_integer_t *integer) {
	integer->value = 0;
}

static void ast_int_quit(duckLisp_t *duckLisp, duckLisp_ast_integer_t *integer) {
	integer->value = 0;
}

static dl_error_t ast_generate_int(duckLisp_t *duckLisp, duckLisp_ast_integer_t *integer,
                                               const duckLisp_cst_integer_t integerCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	e = dl_string_toPtrdiff(&integer->value, &((char *) duckLisp->source.elements)[integerCST.token_index], integerCST.token_length);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Could not convert token to int."), integerCST.token_index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static void ast_print_int(duckLisp_t duckLisp, duckLisp_ast_integer_t integer) {
	printf("%lli", integer.value);
}


static void cst_float_init(duckLisp_cst_float_t *floatingPoint) {
	floatingPoint->token_length = 0;
	floatingPoint->token_index = 0;
}

static void cst_float_quit(duckLisp_t *duckLisp, duckLisp_cst_float_t *floatingPoint) {
	floatingPoint->token_index = 0;
	floatingPoint->token_length = 0;
}

static dl_error_t cst_parse_float(duckLisp_t *duckLisp, duckLisp_cst_float_t *floatingPoint, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	if (index >= stop_index) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Unexpected end of fragment in float."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if (source[index] == '-') {
		index++;
		
		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit after minus sign."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}
	
	// Try .1
	if (source[index] == '.') {
		index++;
		
		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit after decimal point."), index, throwErrors);
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
				// This is expected. 1. 234.e61  435. for example.
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
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected an integer in exponent of float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		
		if (source[index] == '-') {
			index++;
			
			if (index >= stop_index) {
				eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit after minus sign."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
		}
		
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit in exponent of float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
		
		while (index < stop_index) {
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected a digit in exponent of float."), index, throwErrors);
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
	
	floatingPoint->token_index = start_index;
	floatingPoint->token_length = length;
	
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

static void ast_float_init(duckLisp_ast_float_t *floatingPoint) {
	floatingPoint->value = 0.0;
}

static void ast_float_quit(duckLisp_t *duckLisp, duckLisp_ast_float_t *floatingPoint) {
	floatingPoint->value = 0.0;
}

static dl_error_t ast_generate_float(duckLisp_t *duckLisp, duckLisp_ast_float_t *floatingPoint,
                                               const duckLisp_cst_float_t floatingPointCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	e = dl_string_toDouble(&floatingPoint->value, &((char *) duckLisp->source.elements)[floatingPointCST.token_index], floatingPointCST.token_length);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Could not convert token to float."), floatingPointCST.token_index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static void ast_print_float(duckLisp_t duckLisp, duckLisp_ast_float_t floatingPoint) {
	printf("%e", floatingPoint.value);
}


static void cst_number_init(duckLisp_cst_number_t *number) {
	number->type = cst_number_type_none;
}

static dl_error_t cst_number_quit(duckLisp_t *duckLisp, duckLisp_cst_number_t *number) {
	dl_error_t e = dl_error_ok;
	
	switch (number->type) {
	case cst_number_type_bool:
		/**/ cst_bool_quit(duckLisp, &number->value.boolean);
		break;
	case cst_number_type_int:
		/**/ cst_int_quit(duckLisp, &number->value.integer);
		break;
	case cst_number_type_float:
		/**/ cst_float_quit(duckLisp, &number->value.floatingPoint);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	number->type = cst_number_type_none;
	
	return e;
}

static dl_error_t cst_parse_number(duckLisp_t *duckLisp, duckLisp_cst_number_t *number, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	
	// Try parsing as a bool.
	e = cst_parse_bool(duckLisp, &number->value.boolean, source, start_index, length, dl_false);
	if (e) {
		if (e != dl_error_invalidValue) {
			goto l_cleanup;
		}
		
		// Try parsing as an int.
		e = cst_parse_int(duckLisp, &number->value.integer, source, start_index, length, dl_false);
		if (e) {
			if (e != dl_error_invalidValue) {
				goto l_cleanup;
			}
			
			// Try parsing as a float.
			e = cst_parse_float(duckLisp, &number->value.floatingPoint, source, start_index, length, throwErrors);
			if (e) {
				if (e != dl_error_invalidValue) {
					// eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Could not parse number."), index, throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}
				goto l_cleanup;
			}
			else {
				number->type = cst_number_type_float;
			}
		}
		else {
			number->type = cst_number_type_int;
		}
	}
	else {
		number->type = cst_number_type_bool;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t cst_print_number(duckLisp_t duckLisp, duckLisp_cst_number_t number) {
	dl_error_t e = dl_error_ok;
	
	switch (number.type) {
	case cst_number_type_bool:
		/**/ cst_print_bool(duckLisp, number.value.boolean);
		break;
	case cst_number_type_int:
		/**/ cst_print_int(duckLisp, number.value.integer);
		break;
	case cst_number_type_float:
		/**/ cst_print_float(duckLisp, number.value.floatingPoint);
		break;
	default:
		printf("Number: Type %llu\n", number.type);
		e = dl_error_shouldntHappen;
	}
	
	l_cleanup:
	
	return e;
}

static void ast_number_init(duckLisp_ast_number_t *number) {
	number->type = ast_number_type_none;
}

static dl_error_t ast_number_quit(duckLisp_t *duckLisp, duckLisp_ast_number_t *number) {
	dl_error_t e = dl_error_ok;
	
	switch (number->type) {
	case ast_number_type_bool:
		/**/ ast_bool_quit(duckLisp, &number->value.boolean);
		break;
	case ast_number_type_int:
		/**/ ast_int_quit(duckLisp, &number->value.integer);
		break;
	case ast_number_type_float:
		/**/ ast_float_quit(duckLisp, &number->value.floatingPoint);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	number->type = ast_number_type_none;
	
	return e;
}

static dl_error_t ast_generate_number(duckLisp_t *duckLisp, duckLisp_ast_number_t *number,
                                      const duckLisp_cst_number_t numberCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	number->type = numberCST.type;
	switch (number->type) {
	case ast_number_type_bool:
		e = ast_generate_bool(duckLisp, &number->value.boolean, numberCST.value.boolean, throwErrors);
		break;
	case ast_number_type_int:
		e = ast_generate_int(duckLisp, &number->value.integer, numberCST.value.integer, throwErrors);
		break;
	case ast_number_type_float:
		e = ast_generate_float(duckLisp, &number->value.floatingPoint, numberCST.value.floatingPoint, throwErrors);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t ast_print_number(duckLisp_t duckLisp, duckLisp_ast_number_t number) {
	dl_error_t e = dl_error_ok;
	
	switch (number.type) {
	case ast_number_type_bool:
		/**/ ast_print_bool(duckLisp, number.value.boolean);
		break;
	case ast_number_type_int:
		/**/ ast_print_int(duckLisp, number.value.integer);
		break;
	case ast_number_type_float:
		/**/ ast_print_float(duckLisp, number.value.floatingPoint);
		break;
	default:
		printf("Number: Type %llu\n", number.type);
		e = dl_error_shouldntHappen;
	}
	
	l_cleanup:
	
	return e;
}


static void cst_string_init(duckLisp_cst_string_t *string) {
	string->token_length = 0;
	string->token_index = 0;
}

static void cst_string_quit(duckLisp_t *duckLisp, duckLisp_cst_string_t *string) {
	string->token_index = 0;
	string->token_length = 0;
}

static dl_error_t cst_parse_string(duckLisp_t *duckLisp, duckLisp_cst_string_t *string, char *source,
                                   const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	if (index >= stop_index) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Zero length fragment."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if (source[index] == '#') {
		index++;
		
		if (index >= stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected characters after stringify operator."), index, throwErrors);
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
				index++;
				
				if (index >= stop_index) {
					eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected character in string escape sequence."), index, throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}
				
				// Eat character.
			}
			else if (source[index] == '"') {
				index++;
				break;
			}
			
			index++;
		}
		
		if (index != stop_index) {
			eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected end of fragment after quote."), index, throwErrors);
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
	string->token_index = start_index + 1;
	string->token_length = length - 2;
	
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

static void ast_string_init(duckLisp_ast_string_t *string) {
	string->value = dl_null;
	string->value_length = 0;
}

static dl_error_t ast_string_quit(duckLisp_t *duckLisp, duckLisp_ast_string_t *string) {
	dl_error_t e = dl_error_ok;
	
	string->value_length = 0;
	
	e = dl_free(&duckLisp->memoryAllocation, (void **) &string->value);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t ast_generate_string(duckLisp_t *duckLisp, duckLisp_ast_string_t *string,
                                      const duckLisp_cst_string_t stringCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	string->value_length = 0;
	
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) &string->value, stringCST.token_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}
	
	/**/ dl_memcopy_noOverlap(string->value, &((char *) duckLisp->source.elements)[stringCST.token_index], stringCST.token_length);
	
	string->value_length = stringCST.token_length;
	
	l_cleanup:
	
	return e;
}

static void ast_print_string(duckLisp_t duckLisp, duckLisp_ast_string_t string) {
	if (string.value_length == 0) {
		puts("{NULL}");
		return;
	}
	
	putchar('"');
	for (dl_size_t i = 0; i < string.value_length; i++) {
		switch (string.value[i]) {
		case '"':
		case '\\':
			putchar('\\');
		}
		putchar(string.value[i]);
	}
	putchar('"');
}


static void cst_constant_init(duckLisp_cst_constant_t *constant) {
	constant->type = cst_constant_type_none;
}

static dl_error_t cst_constant_quit(duckLisp_t *duckLisp, duckLisp_cst_constant_t *constant) {
	dl_error_t e = dl_error_ok;
	
	switch (constant->type) {
	case cst_constant_type_number:
		e = cst_number_quit(duckLisp, &constant->value.number);
		break;
	case cst_constant_type_string:
		/**/ cst_string_quit(duckLisp, &constant->value.string);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	constant->type = cst_constant_type_none;
	
	return e;
}

static dl_error_t cst_parse_constant(duckLisp_t *duckLisp, duckLisp_cst_constant_t *constant, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	
	// Try parsing as a number.
	e = cst_parse_number(duckLisp, &constant->value.number, source, start_index, length, throwErrors);
	if (e) {
		if (e != dl_error_invalidValue) {
			goto l_cleanup;
		}
		
		// Try parsing as a string.
		e = cst_parse_string(duckLisp, &constant->value.string, source, start_index, length, throwErrors);
		if (e) {
			if (e != dl_error_invalidValue) {
				goto l_cleanup;
			}
			// eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Not a constant."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		else {
			constant->type = cst_constant_type_string;
		}
	}
	else {
		constant->type = cst_constant_type_number;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t cst_print_constant(duckLisp_t duckLisp, duckLisp_cst_constant_t constant) {
	dl_error_t e = dl_error_ok;
	
	switch (constant.type) {
	case cst_constant_type_number:
		e = cst_print_number(duckLisp, constant.value.number);
		break;
	case cst_constant_type_string:
		/**/ cst_print_string(duckLisp, constant.value.string);
		break;
	default:
		printf("Constant: No type\n");
		e = dl_error_shouldntHappen;
	}
	
	return e;
}

static void ast_constant_init(duckLisp_ast_constant_t *constant) {
	constant->type = ast_constant_type_none;
}

static dl_error_t ast_constant_quit(duckLisp_t *duckLisp, duckLisp_ast_constant_t *constant) {
	dl_error_t e = dl_error_ok;
	
	switch (constant->type) {
	case ast_constant_type_number:
		e = ast_number_quit(duckLisp, &constant->value.number);
		break;
	case ast_constant_type_string:
		e = ast_string_quit(duckLisp, &constant->value.string);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	constant->type = ast_constant_type_none;
	
	return e;
}

static dl_error_t ast_generate_constant(duckLisp_t *duckLisp, duckLisp_ast_constant_t *constant,
                                        const duckLisp_cst_constant_t constantCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	constant->type = constantCST.type;
	switch (constant->type) {
	case ast_constant_type_number:
		e = ast_generate_number(duckLisp, &constant->value.number, constantCST.value.number, throwErrors);
		break;
	case ast_constant_type_string:
		e = ast_generate_string(duckLisp, &constant->value.string, constantCST.value.string, throwErrors);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t ast_print_constant(duckLisp_t duckLisp, duckLisp_ast_constant_t constant) {
	dl_error_t e = dl_error_ok;
	
	switch (constant.type) {
	case ast_constant_type_number:
		e = ast_print_number(duckLisp, constant.value.number);
		break;
	case ast_constant_type_string:
		/**/ ast_print_string(duckLisp, constant.value.string);
		break;
	default:
		printf("Constant: No type\n");
		e = dl_error_shouldntHappen;
	}
	
	return e;
}


static void cst_compoundExpression_init(duckLisp_cst_compoundExpression_t *compoundExpression) {
	compoundExpression->type = cst_compoundExpression_type_none;
}

static dl_error_t cst_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression) {
	dl_error_t e = dl_error_ok;
	
	switch (compoundExpression->type) {
	case cst_compoundExpression_type_constant:
		e = cst_constant_quit(duckLisp, &compoundExpression->value.constant);
		break;
	case cst_compoundExpression_type_identifier:
		/**/ cst_identifier_quit(duckLisp, &compoundExpression->value.identifier);
		break;
	case cst_compoundExpression_type_expression:
		e = cst_expression_quit(duckLisp, &compoundExpression->value.expression);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	compoundExpression->type = cst_compoundExpression_type_none;
	
	return e;
}

static dl_error_t cst_parse_compoundExpression(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	
	cst_compoundExpression_init(compoundExpression);
	
	// Try parsing as a constant. Do this first because of boolean values that look like identifiers.
	e = cst_parse_constant(duckLisp, &compoundExpression->value.constant, source, start_index, length, dl_false);
	if (e) {
		if (e != dl_error_invalidValue) {
			goto l_cleanup;
		}
		
		// Try parsing as an identifier.
		e = cst_parse_identifier(duckLisp, &compoundExpression->value.identifier, source, start_index, length, dl_false);
		
		if (e) {
			if (e != dl_error_invalidValue) {
				goto l_cleanup;
			}
			
			// Try parsing as an expression.
			e = cst_parse_expression(duckLisp, &compoundExpression->value.expression, source, start_index, length, throwErrors);
			if (e) {
				if (e != dl_error_invalidValue) {
					goto l_cleanup;
				}
				// eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Not a compound expression."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
			else {
				compoundExpression->type = cst_compoundExpression_type_expression;
			}
		}
		else {
			compoundExpression->type = cst_compoundExpression_type_identifier;
		}
	}
	else {
		compoundExpression->type = cst_compoundExpression_type_constant;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t cst_print_compoundExpression(duckLisp_t duckLisp, duckLisp_cst_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;
	
	switch (compoundExpression.type) {
	case cst_compoundExpression_type_constant:
		e = cst_print_constant(duckLisp, compoundExpression.value.constant);
		break;
	case cst_compoundExpression_type_identifier:
		/**/ cst_print_identifier(duckLisp, compoundExpression.value.identifier);
		break;
	case cst_compoundExpression_type_expression:
		e = cst_print_expression(duckLisp, compoundExpression.value.expression);
		break;
	default:
		printf("Compound expression: Type %llu\n", compoundExpression.type);
		e = dl_error_shouldntHappen;
	}
	
	return e;
}

static void ast_compoundExpression_init(duckLisp_ast_compoundExpression_t *compoundExpression) {
	compoundExpression->type = ast_compoundExpression_type_none;
}

static dl_error_t ast_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression) {
	dl_error_t e = dl_error_ok;
	
	switch (compoundExpression->type) {
	case ast_compoundExpression_type_constant:
		e = ast_constant_quit(duckLisp, &compoundExpression->value.constant);
		break;
	case ast_compoundExpression_type_identifier:
		e = ast_identifier_quit(duckLisp, &compoundExpression->value.identifier);
		break;
	case ast_compoundExpression_type_expression:
		e = ast_expression_quit(duckLisp, &compoundExpression->value.expression);
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	compoundExpression->type = ast_compoundExpression_type_none;
	
	return e;
}

static dl_error_t ast_generate_compoundExpression(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression,
                                                  duckLisp_cst_compoundExpression_t compoundExpressionCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	compoundExpression->type = compoundExpressionCST.type;
	switch (compoundExpression->type) {
	case ast_compoundExpression_type_constant:
		e = ast_generate_constant(duckLisp, &compoundExpression->value.constant, compoundExpressionCST.value.constant, throwErrors);
		break;
	case ast_compoundExpression_type_identifier:
		e = ast_generate_identifier(duckLisp, &compoundExpression->value.identifier, compoundExpressionCST.value.identifier, throwErrors);
		break;
	case ast_compoundExpression_type_expression:
		if (compoundExpressionCST.value.expression.compoundExpressions_length == 0) {
			compoundExpression->type = ast_compoundExpression_type_constant;
			compoundExpression->value.constant.type = ast_constant_type_number;
			compoundExpression->value.constant.value.number.type = ast_number_type_int;
			compoundExpression->value.constant.value.number.value.integer.value = 0;
		}
		else e = ast_generate_expression(duckLisp, &compoundExpression->value.expression,
		                                 compoundExpressionCST.value.expression, throwErrors);
		
		break;
	default:
		e = dl_error_shouldntHappen;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t ast_print_compoundExpression(duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;
	
	switch (compoundExpression.type) {
	case ast_compoundExpression_type_constant:
		e = ast_print_constant(duckLisp, compoundExpression.value.constant);
		break;
	case ast_compoundExpression_type_identifier:
		/**/ ast_print_identifier(duckLisp, compoundExpression.value.identifier);
		break;
	case ast_compoundExpression_type_expression:
		e = ast_print_expression(duckLisp, compoundExpression.value.expression);
		break;
	default:
		printf("Compound expression: Type %llu\n", compoundExpression.type);
		e = dl_error_shouldntHappen;
	}
	
	return e;
}


static dl_error_t cst_append(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *cst, const int index, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	char *source = duckLisp->source.elements;
	const dl_size_t source_length = duckLisp->source.elements_length;
	
	// e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->cst.compoundExpressions,
	//                (duckLisp->cst.compoundExpressions_length + 1) * sizeof(duckLisp_cst_compoundExpression_t));
	// if (e) {
	// 	goto l_cleanup;
	// }
	// duckLisp->cst.compoundExpressions_length++;
	
	// e = cst_parse_compoundExpression(duckLisp, &duckLisp->cst.compoundExpressions[duckLisp->cst.compoundExpressions_length - 1], source, index,
	//                                  source_length - index, throwErrors);
	e = cst_parse_compoundExpression(duckLisp, cst, source, index, source_length - index, throwErrors);
	if (e) {
		eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Error parsing expression."), 0, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

static dl_error_t ast_append(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *ast, duckLisp_cst_compoundExpression_t *cst, const int index,
                             dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	char *source = duckLisp->source.elements;
	const dl_size_t source_length = duckLisp->source.elements_length;
	
	// e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->ast.compoundExpressions,
	//                (duckLisp->ast.compoundExpressions_length + 1) * sizeof(duckLisp_ast_compoundExpression_t));
	// if (e) {
	// 	goto l_cleanup;
	// }
	// duckLisp->ast.compoundExpressions_length++;
	
	// e = ast_generate_compoundExpression(duckLisp, &duckLisp->ast.compoundExpressions[duckLisp->ast.compoundExpressions_length - 1],
	//                                     duckLisp->cst.compoundExpressions[duckLisp->cst.compoundExpressions_length - 1], throwErrors);
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

static void scope_init(duckLisp_t *duckLisp, duckLisp_scope_t *scope) {
	/**/ dl_trie_init(&scope->variables_trie, &duckLisp->memoryAllocation, -1);
	scope->variables_length = 0;
	/**/ dl_trie_init(&scope->generators_trie, &duckLisp->memoryAllocation, -1);
	scope->generators_length = 0;
	/**/ dl_trie_init(&scope->functions_trie, &duckLisp->memoryAllocation, -1);
	scope->functions_length = 0;
}

static dl_error_t scope_getTop(duckLisp_t *duckLisp, duckLisp_scope_t *scope) {
	dl_error_t e = dl_error_ok;
	
	e = dl_array_getTop(&duckLisp->scope_stack, scope);
	if (e == dl_error_bufferUnderflow) {
		// Push a scope if we don't have one yet.
		/**/ scope_init(duckLisp, scope);
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

static dl_error_t scope_getObjectFromName(duckLisp_t *duckLisp, duckLisp_object_t *object, const char *name, const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;
	
	duckLisp_scope_t scope;
	dl_ptrdiff_t index = -1;
	dl_ptrdiff_t scope_index = duckLisp->scope_stack.elements_length;
	
	while (dl_true) {
		e = dl_array_get(&duckLisp->scope_stack, (void *) &scope, --scope_index);
		if (e) {
			if (e == dl_error_invalidValue) {
				object->type = duckLisp_object_type_none;
				e = dl_error_ok;
			}
			break;
		}
		
		/**/ dl_trie_find(scope.variables_trie, &index, name, name_length);
		if (index != -1) {
			e = dl_array_get(&duckLisp->stack, (void *) object, index);
			break;
		}
	}
	
	return e;
}

static dl_error_t scope_getObjectIndexFromName(duckLisp_t *duckLisp, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length) {
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
		
		/**/ dl_trie_find(scope.variables_trie, index, name, name_length);
		if (*index != -1) {
			break;
		}
	}
	
	return e;
}

static dl_error_t scope_getFunctionFromName(duckLisp_t *duckLisp, duckLisp_functionType_t *functionType, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length) {
	dl_error_t e = dl_error_ok;
	
	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = duckLisp->scope_stack.elements_length;
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
		
		/**/ dl_trie_find(scope.functions_trie, (dl_ptrdiff_t *) functionType, name, name_length);
		if (*functionType != duckLisp_functionType_generator) {
			/**/ dl_trie_find(scope.variables_trie, index, name, name_length);
		}
		else {
			/**/ dl_trie_find(scope.generators_trie, index, name, name_length);
		}
		// Return the function in the nearest scope.
		if (*functionType != duckLisp_functionType_none) {
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

dl_error_t duckLisp_emit_pushString(duckLisp_t *duckLisp, dl_array_t *assembly, dl_ptrdiff_t *stackIndex, char *string, dl_size_t string_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushString;
	
	// Write string length.
	
	if (string_length > DL_UINT16_MAX) {
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("String longer than DL_UINT_MAX. Truncating string to fit."));
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
	
	l_cleanup:
	
	return e;
}

dl_error_t duckLisp_emit_ccall(duckLisp_t *duckLisp, dl_array_t *assembly, dl_ptrdiff_t callback_index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
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

dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp, dl_array_t *assembly, int integer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
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
	
	l_cleanup:
	
	return e;
}

dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp, dl_array_t *assembly, dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushIndex;
	
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
	
	l_cleanup:
	
	return e;
}

/*
==========
Generators
==========
*/

dl_error_t duckLisp_generator_subroutine(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	// dl_ptrdiff_t identifier_index = -1;
	// dl_ptrdiff_t string_index = -1;
	// dl_array_t *assemblyFragment = dl_null;
	
	// /* Check arguments for call and type errors. */
	
	// e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	// if (e) {
	// 	goto l_cleanup;
	// }
	
	// if (expression->compoundExpressions[1].type != ast_compoundExpression_type_identifier) {
	// 	e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
	// 	                          expression->compoundExpressions[0].value.identifier.value_length);
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
	// 	if (eError) {
	// 		e = eError;
	// 	}
	// 	goto l_cleanup;
	// }
	
	// if ((expression->compoundExpressions[2].type != ast_compoundExpression_type_constant) &&
	//     (expression->compoundExpressions[2].value.constant.type != ast_constant_type_string)) {
	// 	e = dl_array_pushElements(&eString, DL_STR("Argument 2 of function \""));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
	// 	                          expression->compoundExpressions[0].value.identifier.value_length);
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, DL_STR("\" should be a string."));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
	// 	if (eError) {
	// 		e = eError;
	// 	}
	// 	goto l_cleanup;
	// }
	
	// // Create the string variable.
	// e = duckLisp_emit_pushString(duckLisp, assembly, &identifier_index, expression->compoundExpressions[2].value.constant.value.string.value,
	//                                  expression->compoundExpressions[2].value.constant.value.string.value_length);
	// if (e) {
	// 	goto l_cleanup;
	// }
	
	// // Insert arg1 into this scope's name trie.
	// e = duckLisp_scope_addObject(duckLisp, expression->compoundExpressions[1].value.constant.value.string.value,
	//                              expression->compoundExpressions[1].value.constant.value.string.value_length);
	// if (e) {
	// 	goto l_cleanup;
	// }
	
	l_cleanup:
	
	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}
	
	return e;
}

dl_error_t duckLisp_generator_callback(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	dl_ptrdiff_t callback_index = -1;
	dl_ptrdiff_t argument_index = -1;
	// dl_ptrdiff_t string_index = -1;
	// dl_array_t *assemblyFragment = dl_null;
	
	// /* Check arguments for call and type errors. */
	
	// e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 3);
	// if (e) {
	// 	goto l_cleanup;
	// }
	
	// if (expression->compoundExpressions[1].type != ast_compoundExpression_type_identifier) {
	// 	e = dl_array_pushElements(&eString, DL_STR("Argument 1 of function \""));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
	// 	                          expression->compoundExpressions[0].value.identifier.value_length);
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, DL_STR("\" should be an identifier."));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
	// 	if (eError) {
	// 		e = eError;
	// 	}
	// 	goto l_cleanup;
	// }
	
	// if ((expression->compoundExpressions[2].type != ast_compoundExpression_type_constant) &&
	//     (expression->compoundExpressions[2].value.constant.type != ast_constant_type_string)) {
	// 	e = dl_array_pushElements(&eString, DL_STR("Argument 2 of function \""));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, expression->compoundExpressions[0].value.identifier.value,
	// 	                          expression->compoundExpressions[0].value.identifier.value_length);
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	e = dl_array_pushElements(&eString, DL_STR("\" should be a string."));
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// 	eError = duckLisp_error_pushRuntime(duckLisp, eString.elements, eString.elements_length * eString.element_size);
	// 	if (eError) {
	// 		e = eError;
	// 	}
	// 	goto l_cleanup;
	// }
	
	e = scope_getObjectIndexFromName(duckLisp, &callback_index,
	                                 expression->compoundExpressions[0].value.constant.value.string.value,
	                                 expression->compoundExpressions[0].value.constant.value.string.value_length);
	if (e || callback_index == -1) {
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Could not find callback name."));
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}
	
	// Push all arguments onto the stack.
	for (int i = 1; i < expression->compoundExpressions_length; i++) {
		switch (expression->compoundExpressions[i].type) {
		case ast_compoundExpression_type_identifier:
			e = scope_getObjectIndexFromName(duckLisp, &argument_index,
			                                 expression->compoundExpressions[i].value.constant.value.string.value,
			                                 expression->compoundExpressions[i].value.constant.value.string.value_length);
			if (e || argument_index == -1) {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Could not find callback name."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			
			e = duckLisp_emit_pushIndex(duckLisp, assembly, argument_index);
			if (e) {
				goto l_cleanup;
			}
			break;
		default:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Unsupported expression type."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
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

dl_error_t duckLisp_generator_expression(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	
	// Insert code here.
	
	// l_cleanup:
	
	return e;
}

/*
=======
Compile
=======
*/

static dl_error_t compile(duckLisp_t *duckLisp, dl_array_t *bytecode, duckLisp_ast_compoundExpression_t astCompoundexpression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	duckLisp_ast_identifier_t functionName = {0};
	duckLisp_functionType_t functionType;
	dl_ptrdiff_t functionIndex = -1;
	// Not initialized until later. This is reused for multiple arrays.
	dl_array_t assembly; // duckLisp_instructionObject_t
	// Initialize to zero.
	/**/ dl_array_init(&assembly, dl_null, 0, dl_array_strategy_fit);
	// Create high-level bytecode list.
	dl_array_t instructionList; // dl_array_t
	/**/ dl_array_init(&instructionList, &duckLisp->memoryAllocation, sizeof(dl_array_t), dl_array_strategy_double);
	// expression stack for navigating the tree.
	dl_array_t expressionStack;
	/**/ dl_array_init(&expressionStack, &duckLisp->memoryAllocation, sizeof(duckLisp_ast_compoundExpression_t), dl_array_strategy_double);
	dl_ptrdiff_t expression_index = 0;
	duckLisp_ast_compoundExpression_t currentExpression;
	dl_error_t (*generatorCallback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*);
	
	typedef struct node_s {
		dl_array_t instructionObjects;
		dl_ptrdiff_t *nodes;
		dl_size_t nodes_length;
	} node_t;
	
	dl_array_t nodeArray; // node_t
	/**/ dl_array_init(&nodeArray, &duckLisp->memoryAllocation, sizeof(node_t), dl_array_strategy_double);
	node_t tempNode;
	dl_ptrdiff_t assemblyTreeRoot = -1;
	dl_array_t nodeStack; // dl_ptrdiff_t
	/**/ dl_array_init(&nodeStack, &duckLisp->memoryAllocation, sizeof(dl_ptrdiff_t), dl_array_strategy_double);
	dl_ptrdiff_t currentNode = -1;
	
	// dl_array_t bytecode; // unsigned char
	/**/ dl_array_init(bytecode, &duckLisp->memoryAllocation, sizeof(unsigned char), dl_array_strategy_double);
	
	
	/* * * * * *
	 * Compile *
	 * * * * * */
	
	
	putchar('\n');
	
	if (astCompoundexpression.type != ast_compoundExpression_type_expression) {
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Cannot compile non-expression types to bytecode."));
		if (eError) {
			e = eError;
		}
		goto l_cleanup;
	}
	
	/* First stage: Create assembly tree from AST. */
	
	// Bootstrap.
	currentExpression.type = ast_compoundExpression_type_expression;
	currentExpression.value.expression.compoundExpressions = &astCompoundexpression;
	currentExpression.value.expression.compoundExpressions_length = 1;
	
	assemblyTreeRoot = currentNode = nodeArray.elements_length; // 0
	
	// Initialize to zero.
	/**/ dl_array_init(&tempNode.instructionObjects, dl_null, 0, dl_array_strategy_fit);
	tempNode.nodes = dl_null;
	tempNode.nodes_length = 0;
	e = dl_array_pushElement(&nodeArray, &tempNode);
	if (e) {
		goto l_cleanup;
	}
	
	while (dl_true) {
		// Now that the subexpressions cannot change (generator has returned), push them onto the stack.
		for (dl_ptrdiff_t j = currentExpression.value.expression.compoundExpressions_length - 1; j >= 0; --j) {
			if (currentExpression.value.expression.compoundExpressions[j].type == ast_compoundExpression_type_expression) {
				
				// Create child and push into the node array.
				// Initialize to zero.
				/**/ dl_array_init(&tempNode.instructionObjects, dl_null, 0, dl_array_strategy_fit);
				tempNode.nodes_length = 0;
				tempNode.nodes = dl_null;
				e = dl_array_pushElement(&nodeArray, &tempNode);
				if (e) {
					goto l_cleanup;
				}
				
				// Push the node into the tree.
				// Notice how `#define`s are unscoped. This could be nice in Hanabi. I could also create a `let-m` instead.
#ifdef CURRENTNODE
#error CURRENTNODE should not have been defined.
#endif
#define CURRENTNODE DL_ARRAY_GETADDRESS(nodeArray, node_t, currentNode)

				e = dl_realloc(&duckLisp->memoryAllocation, (void **) &CURRENTNODE.nodes,
				               (CURRENTNODE.nodes_length + 1) * sizeof(node_t));
				if (e) {
					goto l_cleanup;
				}
				CURRENTNODE.nodes[CURRENTNODE.nodes_length] = nodeArray.elements_length - 1;
				CURRENTNODE.nodes_length++;
				
				/* Push the address of the new node on the stack. We are now traversing the section of the tree we just
				   created. */
				e = dl_array_pushElement(&nodeStack, &nodeArray.elements_length); // Need to `--` later.
				if (e) {
					goto l_cleanup;
				}

				// Push arguments.
				e = dl_array_pushElement(&expressionStack, &currentExpression.value.expression.compoundExpressions[j]);
				if (e) {
					goto l_cleanup;
				}
			}
		}
		
		if (expressionStack.elements_length <= 0) {
			break;
		}
		
		e = dl_array_popElement(&nodeStack, &currentNode);
		if (e) {
			goto l_cleanup;
		}
		--currentNode;
		
		e = dl_array_popElement(&expressionStack, &currentExpression);
		if (e) {
			goto l_cleanup;
		}
		
		e = ast_print_compoundExpression(*duckLisp, currentExpression);
		if (e) {
			goto l_cleanup;
		}
		putchar('\n');
		
		/* If it is an expression, call the generator for it to compile the expression. */
		if (currentExpression.value.expression.compoundExpressions_length == 0) {
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Encountered empty expression."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		
		/**/ dl_array_init(&assembly, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionObject_t), dl_array_strategy_double);
		
		// Compile!
		switch (currentExpression.value.expression.compoundExpressions[0].type) {
		case ast_compoundExpression_type_constant:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Constants as function names are not supported."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		case ast_compoundExpression_type_identifier:
			// Run function generator.
			functionName = currentExpression.value.expression.compoundExpressions[0].value.identifier;
			e = scope_getFunctionFromName(duckLisp, &functionType, &functionIndex, functionName.value, functionName.value_length);
			if (e) {
				goto l_cleanup;
			}
			if (functionType == duckLisp_functionType_none) {
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
			switch (functionType) {
			case duckLisp_functionType_ducklisp:
				e = duckLisp_generator_subroutine(duckLisp, &assembly, &currentExpression.value.expression);
				if (e) {
					goto l_cleanup;
				}
				break;
			case duckLisp_functionType_c:
				e = duckLisp_generator_callback(duckLisp, &assembly, &currentExpression.value.expression);
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
				e = generatorCallback(duckLisp, &assembly, &currentExpression.value.expression);
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
		case ast_compoundExpression_type_expression:
			// Run expression generator.
			e = duckLisp_generator_expression(duckLisp, &assembly, &currentExpression.value.expression);
			if (e) {
				goto l_cleanup;
			}
			break;
		default:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid compound expression type. Can't happen."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		}
		/* Important note: The generator has the freedom to rearrange its portion of the AST. This allows generators
		   much more freedom in optimizing code. */

		CURRENTNODE.instructionObjects = assembly;
	}
	
	// Flatten tree.
	e = dl_array_clear(&nodeStack);
	if (e) {
		goto l_cleanup;
	}
	currentNode = assemblyTreeRoot;
	
	while (dl_true) {
		// Append instructions to instruction list.
		e = dl_array_pushElement(&instructionList, &CURRENTNODE.instructionObjects);
		if (e) {
			goto l_cleanup;
		}
		
		// Push nodes.
		for (dl_ptrdiff_t j = CURRENTNODE.nodes_length - 1; j >= 0; --j) {
		// for (dl_ptrdiff_t j = 0; j < CURRENTNODE.nodes_length; j++) {
			e = dl_array_pushElement(&nodeStack, &CURRENTNODE.nodes[j]);
			if (e) {
				goto l_cleanup;
			}
		}
		// Done?
		if (nodeStack.elements_length <= 0) {
			break;
		}
		// Next node.
		e = dl_array_popElement(&nodeStack, &currentNode);
		if (e) {
			goto l_cleanup;
		}
	}
#undef CURRENTNODE
	
	// Print list.
	printf("\n");
	
	
	dl_array_t ia;
	duckLisp_instructionObject_t io;
	for (dl_ptrdiff_t i = 0; i < instructionList.elements_length; i++) {
		ia = DL_ARRAY_GETADDRESS(instructionList, dl_array_t, i);
		printf("{\n");
		for (dl_ptrdiff_t j = 0; j < ia.elements_length; j++) {
			io = DL_ARRAY_GETADDRESS(ia, duckLisp_instructionObject_t, j);
			printf("    {\n");
			printf("        Instruction class: %s\n",
				(char *[5]){
					"nop",
					"pushString",
					"pushInteger",
					"pushIndex",
					"ccall",
				}[io.instructionClass]);
			printf("        [\n");
			duckLisp_instructionArgClass_t ia;
			for (dl_ptrdiff_t k = 0; k < io.args.elements_length; k++) {
				ia = ((duckLisp_instructionArgClass_t *) io.args.elements)[k];
				printf("            {\n");
				printf("                Type: %s\n",
					(char *[4]){
						"none",
						"integer",
						"index",
						"string",
					}[ia.type]);
				printf("                Value: ");
				switch (ia.type) {
				case duckLisp_instructionArgClass_type_none:
					printf("None\n");
					break;
				case duckLisp_instructionArgClass_type_integer:
					printf("%i\n", ia.value.integer);
					break;
				case duckLisp_instructionArgClass_type_index:
					printf("%llu\n", ia.value.index);
					break;
				case duckLisp_instructionArgClass_type_string:
					printf("\"");
					for (dl_ptrdiff_t m = 0; m < ia.value.string.value_length; m++) {
						putchar(ia.value.string.value[m]);
					}
					printf("\"\n");
					break;
				default:
					printf("                Undefined type.\n");
				}
				printf("            }\n");
			}
			printf("        ]\n");
			printf("    }\n");
		}
		printf("}\n");
	}
	
	
	/** * * * * *
	 * Assemble *
	 * * * * * **/
	
	
	duckLisp_instruction_t currentInstruction;
	dl_array_t currentArgs; // unsigned char
	/**/ dl_array_init(&currentArgs, &duckLisp->memoryAllocation, sizeof(unsigned char), dl_array_strategy_double);
	for (dl_ptrdiff_t i = instructionList.elements_length - 1; i >= 0; --i) {
		dl_array_t instructions = DL_ARRAY_GETADDRESS(instructionList, dl_array_t, i);
		for (dl_ptrdiff_t j = 0; j < instructions.elements_length; j++) {
			duckLisp_instructionObject_t instruction = DL_ARRAY_GETADDRESS(instructions, duckLisp_instructionObject_t, j);
			// This is OK because there is no chance of reallocating the args array.
			duckLisp_instructionArgClass_t *args = &DL_ARRAY_GETADDRESS(instruction.args, duckLisp_instructionArgClass_t, 0);
			dl_size_t args_length = instruction.args.elements_length;
			dl_size_t byte_length;
			
			e = dl_array_clear(&currentArgs);
			if (e) {
				goto l_cleanup;
			}
			
			switch (instruction.instructionClass) {
			case duckLisp_instructionClass_nop:
				// Finish later. We probably don't need it.
				break;
			case duckLisp_instructionClass_pushIndex:
				switch (args[0].type) {
				case duckLisp_instructionArgClass_type_index:
					if ((unsigned long) args[0].value.index < 0x100UL) {
						currentInstruction = duckLisp_instruction_pushIndex8;
						byte_length = 1;
					}
					else if ((unsigned int) args[0].value.index < 0x10000UL) {
						currentInstruction = duckLisp_instruction_pushIndex16;
						byte_length = 2;
					}
					else {
						currentInstruction = duckLisp_instruction_pushIndex32;
						byte_length = 4;
					}
					e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
					if (e) {
						goto l_cleanup;
					}
					for (dl_ptrdiff_t n = 0; n < byte_length; n++) {
						DL_ARRAY_GETADDRESS(currentArgs, unsigned char, n) = (args[0].value.index >> 8*n) & 0xFFU;
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
			case duckLisp_instructionClass_pushInteger:
				switch (args[0].type) {
				case duckLisp_instructionArgClass_type_integer:
					if ((unsigned long) args[0].value.integer < 0x100UL) {
						currentInstruction = duckLisp_instruction_pushInteger8;
						byte_length = 1;
					}
					else if ((unsigned int) args[0].value.integer < 0x10000UL) {
						currentInstruction = duckLisp_instruction_pushInteger16;
						byte_length = 2;
					}
					else {
						currentInstruction = duckLisp_instruction_pushInteger32;
						byte_length = 4;
					}
					e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
					if (e) {
						goto l_cleanup;
					}
					for (dl_ptrdiff_t n = 0; n < byte_length; n++) {
						DL_ARRAY_GETADDRESS(currentArgs, unsigned char, n) = (args[0].value.integer >> 8*n) & 0xFFU;
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
			case duckLisp_instructionClass_pushString:
				switch (args[0].type) {
				case duckLisp_instructionArgClass_type_integer:
					if ((unsigned long) args[0].value.integer < 0x100UL) {
						currentInstruction = duckLisp_instruction_pushInteger8;
						byte_length = 1;
					}
					else if ((unsigned int) args[0].value.integer < 0x10000UL) {
						currentInstruction = duckLisp_instruction_pushInteger16;
						byte_length = 2;
					}
					else {
						currentInstruction = duckLisp_instruction_pushInteger32;
						byte_length = 4;
					}
					e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
					if (e) {
						goto l_cleanup;
					}
					for (dl_ptrdiff_t n = 0; n < byte_length; n++) {
						DL_ARRAY_GETADDRESS(currentArgs, unsigned char, n) = (args[0].value.integer >> 8*n) & 0xFFU;
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
			case duckLisp_instructionClass_ccall:
				switch (args[0].type) {
				case duckLisp_instructionArgClass_type_integer:
					if ((unsigned long) args[0].value.integer < 0x100UL) {
						currentInstruction = duckLisp_instruction_ccall8;
						byte_length = 1;
					}
					else if ((unsigned int) args[0].value.integer < 0x10000UL) {
						currentInstruction = duckLisp_instruction_ccall16;
						byte_length = 2;
					}
					else {
						currentInstruction = duckLisp_instruction_ccall32;
						byte_length = 4;
					}
					e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
					if (e) {
						goto l_cleanup;
					}
					for (dl_ptrdiff_t n = 0; n < byte_length; n++) {
						DL_ARRAY_GETADDRESS(currentArgs, unsigned char, n) = (args[0].value.integer >> 8*n) & 0xFFU;
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
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid instruction class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			
			// Write instruction.
			e = dl_array_pushElement(bytecode, dl_null);
			if (e) {
				goto l_cleanup;
			}
			DL_ARRAY_GETTOPADDRESS(*bytecode, unsigned char) = currentInstruction & 0xFFU;
			e = dl_array_pushElements(bytecode, currentArgs.elements, currentArgs.elements_length);
			if (e) {
				goto l_cleanup;
			}
		}
	}
	
	
	/* * * * * *
	 * Cleanup *
	 * * * * * */
	
	l_cleanup:
	
	putchar('\n');
	
	DL_ARRAY_FOREACH(tempNode, nodeArray, {
		break;
	}, {
		if (tempNode.nodes != dl_null) {
			eError = dl_free(&duckLisp->memoryAllocation, (void **) &tempNode.nodes);
			if (eError) {
				e = eError;
				break;
			}
		}
	})
	
	eError = dl_array_quit(&nodeStack);
	if (eError) {
		e = eError;
	}
	
	// I don't think this should ever be freed here.
	// eError = dl_array_quit(&assembly);
	// if (eError) {
	// 	e = eError;
	// }
	
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

dl_error_t duckLisp_init(duckLisp_t *duckLisp, void *memory, dl_size_t size) {
	dl_error_t error = dl_error_ok;
	
	error = dl_memory_init(&duckLisp->memoryAllocation, memory, size, dl_memoryFit_best);
	if (error) {
		goto l_cleanup;
	}
	
	/* No error */ dl_array_init(&duckLisp->source, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_fit);
	// /* No error */ cst_expression_init(&duckLisp->cst);
	// /* No error */ ast_expression_init(&duckLisp->ast);
	/* No error */ dl_array_init(&duckLisp->errors, &duckLisp->memoryAllocation, sizeof(duckLisp_error_t), dl_array_strategy_fit);
	/* No error */ dl_array_init(&duckLisp->stack, &duckLisp->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->scope_stack, &duckLisp->memoryAllocation, sizeof(duckLisp_scope_t), dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->bytecode, &duckLisp->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->generators_stack, &duckLisp->memoryAllocation,sizeof(dl_error_t (*)(duckLisp_t*, duckLisp_ast_expression_t*)),
	                             dl_array_strategy_double);
	
	duckLisp->frame_pointer = 0;
	
	error = dl_error_ok;
	l_cleanup:
	
	return error;
}

void duckLisp_quit(duckLisp_t *duckLisp) {
	
	// Don't bother freeing since we are going to run `dl_memory_quit`.
	/* No error */ dl_memory_quit(&duckLisp->memoryAllocation);
	// Prevent dangling pointers.
	/* No error */ dl_memclear(duckLisp, sizeof(duckLisp_t));
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
dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length, const char *source, const dl_size_t source_length) {
	dl_error_t e = dl_error_ok;
	// struct {
	// 	dl_bool_t duckLispMemory;
	// } d = {0};
	
	dl_ptrdiff_t index = -1;
	char tempChar;
	duckLisp_object_t object = {0};
	duckLisp_scope_t scope;
	duckLisp_ast_compoundExpression_t ast;
	duckLisp_cst_compoundExpression_t cst;
	dl_array_t bytecode;
	
	/**/ cst_compoundExpression_init(&cst);
	/**/ ast_compoundExpression_init(&ast);
	
	/* Save copy of source. */
	
	// if (duckLisp->cst.compoundExpressions_length != 0) {
	// 	// Add space between two segments of code.
	// 	tempChar = ' ';
	// 	e = dl_array_pushElement(&duckLisp->source, (char *) &tempChar);
	// 	if (e) {
	// 		goto l_cleanup;
	// 	}
	// }
	
	index = duckLisp->source.elements_length;
	
	e = dl_array_pushElements(&duckLisp->source, (char *) source, source_length);
	if (e) {
		goto l_cleanup;
	}
	
	/* Parse. */
	
	// *name = duckLisp->cst.compoundExpressions_length;
	
	e = cst_append(duckLisp, &cst, index, dl_true);
	if (e) {
		goto l_cleanup;
	}
	
	e = ast_append(duckLisp, &ast, &cst, index, dl_true);
	if (e) {
		goto l_cleanup;
	}
	
	// printf("CST: ");
	// e = cst_print_compoundExpression(*duckLisp, cst); putchar('\n');
	// if (e) {
	// 	goto l_cleanup;
	// }
	printf("AST: ");
	e = ast_print_compoundExpression(*duckLisp, ast); putchar('\n');
	if (e) {
		goto l_cleanup;
	}
	
	/* Compile AST to bytecode. */
	
	e = compile(duckLisp, &bytecode, ast);
	if (e) {
		goto l_cleanup;
	}

	/* Push fresh bytecode onto the end of the current bytecode. */
	e = dl_array_pushElements(&duckLisp->bytecode, bytecode.elements, bytecode.elements_length);
	if (e) {
		goto l_cleanup;
	}
	
	/* Push on stack. */
	
	// object.type = duckLisp_object_type_function;
	// e = duckLisp_pushObject(duckLisp, name, name_length, object);
	
	l_cleanup:
	
	return e;
}

void duckLisp_getArgLength(duckLisp_t *duckLisp, dl_size_t *length) {
	*length = ((duckLisp_object_t *) duckLisp->stack.elements)[duckLisp->frame_pointer].value.integer;
}

dl_error_t duckLisp_getArg(duckLisp_t *duckLisp, duckLisp_object_t *object, dl_ptrdiff_t index) {
	if (index < ((duckLisp_object_t *) duckLisp->stack.elements)[duckLisp->frame_pointer].value.integer) {
		*object = ((duckLisp_object_t *) duckLisp->stack.elements)[duckLisp->frame_pointer + index];
		return dl_error_ok;
	}
	else {
		return dl_error_bufferOverflow;
	}
}

dl_error_t duckLisp_pushReturn(duckLisp_t *duckLisp, duckLisp_object_t object) {
	dl_error_t e = dl_error_ok;
	
	e = dl_array_pushElement(&duckLisp->stack, (void *) &object);
	duckLisp->frame_pointer++;
	
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
	
	e = dl_trie_insert(&scope.variables_trie, name, name_length, scope.variables_length);
	if (e) {
		goto l_cleanup;
	}
	scope.variables_length++;
	
	e = scope_setTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

dl_error_t duckLisp_addGenerator(duckLisp_t *duckLisp,
                                 dl_error_t (*callback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*),
                                 const char *name, const dl_size_t name_length) {
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

dl_error_t duckLisp_linkCFunction(duckLisp_t *duckLisp, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length) {
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
	e = dl_trie_insert(&scope.variables_trie, name, name_length, scope.variables_length);
	if (e) {
		goto l_cleanup;
	}
	scope.variables_length++;

	e = scope_setTop(duckLisp, &scope);
	if (e) {
		goto l_cleanup;
	}

	l_cleanup:

	return e;
}

// /*
// Creates an object in the current scope.
// */
// dl_error_t duckLisp_pushObject(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length, const duckLisp_object_t object) {
// 	dl_error_t e = dl_error_ok;
	
// 	// Push object on the stack.
// 	e = dl_array_pushElement(&duckLisp->stack, (void *) &object);
// 	if (e) {
// 		goto l_cleanup;
// 	}
	
// 	// Stick name and index in the current scope's trie.
// 	e = duckLisp_scope_addObjectName(duckLisp, name, name_length);
// 	if (e) {
// 		goto l_cleanup;
// 	}
	
// 	l_cleanup:
	
// 	return e;
// }

// /*
// Creates a generator in the current scope.
// */
// dl_error_t duckLisp_pushGenerator(duckLisp_t *duckLisp, const char *name, const dl_size_t name_length,
//                                   const dl_error_t(*generator)(duckLisp_t*, const duckLisp_ast_expression_t)) {
// 	dl_error_t e = dl_error_ok;
	
// 	// Push object on the stack.
// 	e = dl_array_pushElement(&duckLisp->generators_stack, (void *) generator);
// 	if (e) {
// 		goto l_cleanup;
// 	}
	
// 	// Stick name and index in the current scope's generator trie.
// 	e = duckLisp_scope_addGeneratorName(duckLisp, name, name_length);
// 	if (e) {
// 		goto l_cleanup;
// 	}
	
// 	l_cleanup:
	
// 	return e;
// }
