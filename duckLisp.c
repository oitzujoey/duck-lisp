
#include "duckLisp.h"
#include "DuckLib/string.h"
#include <stdio.h>

/*
===============
Error reporting
===============
*/

static dl_error_t duckLisp_error_push(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length, const dl_ptrdiff_t index,
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
	
	e = array_pushElement(&duckLisp->errors, &error);
	if (e) {
		goto l_cleanup;
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
	case '#':
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
	
	array_t bracketStack;
	char topChar;
	
	dl_bool_t justPopped = dl_false;
	dl_bool_t justPushed = dl_false;
	dl_bool_t wasWhitespace = dl_false;
	
	/**/ cst_expression_init(expression);
	
	// Quick syntax checks.
	if (stop_index - index < 2) {
		eError = duckLisp_error_push(duckLisp, DL_STR("Not an expression: too short."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if ((source[start_index] != '(') || (source[stop_index - 1] != ')')) {
		eError = duckLisp_error_push(duckLisp, DL_STR("Not an expression: no parentheses."), index, throwErrors);
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
	
	/* No error */ array_init(&bracketStack, &duckLisp->memoryAllocation, sizeof(char), array_strategy_double);
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
			e = array_pushElement(&bracketStack, (void *) &source[index]);
			if (e) {
				goto l_cleanup;
			}
		}
		else if (source[index] == ')') {
			if (bracketStack.elements_length != 0) {
				// Check for opening parenthesis.
				e = array_getTop(&bracketStack, (void *) &topChar);
				if (e) {
					goto l_cleanup;
				}
				if (topChar != '(') {
					eError = duckLisp_error_push(duckLisp, DL_STR("No open parenthesis for closing parenthesis."), index, throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}
				
				// Pop opening parenthesis.
				e = array_popElement(&bracketStack, dl_null);
				if (e) {
					goto l_cleanup;
				}
				
				justPopped = dl_true;
			}
			else {
				eError = duckLisp_error_push(duckLisp, DL_STR("No open parenthesis for closing parenthesis."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
		}
		else if (source[index] == '"') {
			if (bracketStack.elements_length != 0) {
				e = array_getTop(&bracketStack, (void *) &topChar);
				if (e) {
					goto l_cleanup;
				}
			}
			if ((bracketStack.elements_length == 0) || (topChar != source[index])) {
				e = array_pushElement(&bracketStack, (void *) &source[index]);
				if (e) {
					goto l_cleanup;
				}
			}
			else {
				e = array_popElement(&bracketStack, dl_null);
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
			// 	eError = duckLisp_error_push(duckLisp, DL_STR("File ended in expression or string brackets."), index);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("No closing parenthesis for opening parenthesis."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	l_cleanup:
	
	if (d.bracketStack) {
		eCleanup = array_quit(&bracketStack);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("Unexpected end of file in identifier."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	/* No error */ dl_string_isAlpha(&tempBool, source[index]);
	if (!tempBool) {
		/* No error */ cst_isIdentifierSymbol(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected a alpha or allowed symbol in identifier."), index, throwErrors);
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
					eError = duckLisp_error_push(duckLisp, DL_STR("Expected a alpha, digit, or allowed symbol in identifier."), index, throwErrors);
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
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected a \"true\" or \"false\" in boolean."), index, throwErrors);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("Could not convert token to bool."), booleanCST.token_index, throwErrors);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("Unexpected end of file in integer."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if (source[index] == '-') {
		index++;
		
		if (index >= stop_index) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Unexpected end of file in integer."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}
	
	/* No error */ dl_string_isDigit(&tempBool, source[index]);
	if (!tempBool) {
		eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit in integer."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	while (index < stop_index) {
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit in integer."), index, throwErrors);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("Could not convert token to int."), integerCST.token_index, throwErrors);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("Unexpected end of fragment in float."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if (source[index] == '-') {
		index++;
		
		if (index >= stop_index) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit after minus sign."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}
	
	// Try .1
	if (source[index] == '.') {
		index++;
		
		if (index >= stop_index) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit after decimal point."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
		
		while ((index < stop_index) && (dl_string_toLower(source[index]) != 'e')) {
			
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_push(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
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
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
		
		while ((index < stop_index) && (dl_string_toLower(source[index]) != 'e') && (source[index] != '.')) {
			
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_push(duckLisp, DL_STR("Expected digit in float."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
			
			index++;
		}
		
		if (source[index] == '.') {
			index++;
			
			if (index >= stop_index) {
				// eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit after decimal point."), index);
				// e = eError ? eError : dl_error_bufferOverflow;
				// This is expected. 1. 234.e61  435. for example.
				goto l_cleanup;
			}
		}
		
		while ((index < stop_index) && (dl_string_toLower(source[index]) != 'e')) {
			
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit in float."), index, throwErrors);
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
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected an integer in exponent of float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		
		if (source[index] == '-') {
			index++;
			
			if (index >= stop_index) {
				eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit after minus sign."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
		}
		
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit in exponent of float."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
		
		while (index < stop_index) {
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit in exponent of float."), index, throwErrors);
				e = eError ? eError : dl_error_invalidValue;
				goto l_cleanup;
			}
			
			index++;
		}
	}
	
	if (index != stop_index) {
		// eError = duckLisp_error_push(duckLisp, DL_STR("."), index);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("Could not convert token to float."), floatingPointCST.token_index, throwErrors);
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
					// eError = duckLisp_error_push(duckLisp, DL_STR("Could not parse number."), index, throwErrors);
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
		eError = duckLisp_error_push(duckLisp, DL_STR("Zero length fragment."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if (source[index] == '\'') {
		index++;
		
		if (index >= stop_index) {
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected characters after stringify operator."), index, throwErrors);
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
					eError = duckLisp_error_push(duckLisp, DL_STR("Expected character in string escape sequence."), index, throwErrors);
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
			eError = duckLisp_error_push(duckLisp, DL_STR("Expected end of fragment after quote."), index, throwErrors);
			e = eError ? eError : dl_error_invalidValue;
			goto l_cleanup;
		}
	}
	else {
		eError = duckLisp_error_push(duckLisp, DL_STR("Not a string."), index, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	string->token_index = start_index;
	string->token_length = length;
	
	l_cleanup:
	
	return e;
}

static void cst_print_string(duckLisp_t duckLisp, duckLisp_cst_string_t string) {
	if (string.token_length == 0) {
		puts("{NULL}");
		return;
	}
	
	for (dl_size_t i = string.token_index; i < string.token_index + string.token_length; i++) {
		putchar(((char *) duckLisp.source.elements)[i]);
	}
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
	
	for (dl_size_t i = 0; i < string.value_length; i++) {
		putchar(string.value[i]);
	}
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
			// eError = duckLisp_error_push(duckLisp, DL_STR("Not a constant."), index, throwErrors);
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
				// eError = duckLisp_error_push(duckLisp, DL_STR("Not a compound expression."), index, throwErrors);
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
		e = ast_generate_expression(duckLisp, &compoundExpression->value.expression, compoundExpressionCST.value.expression, throwErrors);
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


static dl_error_t cst_append(duckLisp_t *duckLisp, const int index, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	char *source = duckLisp->source.elements;
	const dl_size_t source_length = duckLisp->source.elements_length;
	
	e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->cst.compoundExpressions,
	               (duckLisp->cst.compoundExpressions_length + 1) * sizeof(duckLisp_cst_compoundExpression_t));
	if (e) {
		goto l_cleanup;
	}
	duckLisp->cst.compoundExpressions_length++;
	
	e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->ast.compoundExpressions,
	               (duckLisp->ast.compoundExpressions_length + 1) * sizeof(duckLisp_ast_compoundExpression_t));
	if (e) {
		goto l_cleanup;
	}
	duckLisp->ast.compoundExpressions_length++;
	
	e = cst_parse_compoundExpression(duckLisp, &duckLisp->cst.compoundExpressions[duckLisp->cst.compoundExpressions_length - 1], source, index,
	                                 source_length - index, throwErrors);
	if (e) {
		eError = duckLisp_error_push(duckLisp, DL_STR("Error parsing expression."), 0, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	e = ast_generate_compoundExpression(duckLisp, &duckLisp->ast.compoundExpressions[duckLisp->ast.compoundExpressions_length - 1],
	                                    duckLisp->cst.compoundExpressions[duckLisp->cst.compoundExpressions_length - 1], throwErrors);
	if (e) {
		eError = duckLisp_error_push(duckLisp, DL_STR("Error converting CST to AST."), 0, throwErrors);
		e = eError ? eError : dl_error_invalidValue;
		goto l_cleanup;
	}
	
	l_cleanup:
	
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
	
	array_init(&duckLisp->source, &duckLisp->memoryAllocation, sizeof(char), array_strategy_fit);
	
	/* No error */ cst_expression_init(&duckLisp->cst);
	/* No error */ ast_expression_init(&duckLisp->ast);
	/* No error */ array_init(&duckLisp->errors, &duckLisp->memoryAllocation, sizeof(duckLisp_error_t), array_strategy_fit);
	
	// duckLisp->ast.node.nodes = dl_null;
	// duckLisp->ast.node.nodes_length = 0;
	
	error = dl_error_ok;
	l_cleanup:
	
	return error;
}

void duckLisp_quit(duckLisp_t *duckLisp) {
	
	// Don't bother freeing since we are going to run `dl_memory_quit`.
	// /* e = */ array_quit(&duckLisp->source);
	// duckLisp->cst.type = cst_compoundExpression_type_none;
	// /* e = */ array_quit(&duckLisp->errors);
	
	/* No error */ dl_memory_quit(&duckLisp->memoryAllocation);
}

dl_error_t duckLisp_cst_print(duckLisp_t *duckLisp) {
	dl_error_t e = dl_error_ok;
	
	e = cst_print_expression(*duckLisp, duckLisp->cst);
	putchar('\n');
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

dl_error_t duckLisp_ast_print(duckLisp_t *duckLisp) {
	dl_error_t e = dl_error_ok;
	
	e = ast_print_expression(*duckLisp, duckLisp->ast);
	putchar('\n');
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}

dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, dl_ptrdiff_t *handle, const char *source, const dl_size_t source_length) {
	dl_error_t e = dl_error_ok;
	// struct {
	// 	dl_bool_t duckLispMemory;
	// } d = {0};
	
	dl_ptrdiff_t index = -1;
	char tempChar;
	
	/* Save copy of source. */
	
	if (duckLisp->cst.compoundExpressions_length != 0) {
		// Add space between two segments of code.
		tempChar = ' ';
		e = array_pushElement(&duckLisp->source, (char *) &tempChar);
		if (e) {
			goto l_cleanup;
		}
	}
	
	index = duckLisp->source.elements_length;
	
	e = array_pushElements(&duckLisp->source, (char *) source, source_length);
	if (e) {
		goto l_cleanup;
	}
	
	*handle = duckLisp->cst.compoundExpressions_length;
	
	e = cst_append(duckLisp, index, dl_true);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	if (e) {
		*handle = -1;
	}
	
	return e;
}
