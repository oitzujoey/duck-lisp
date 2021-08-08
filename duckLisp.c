
#include "duckLisp.h"
#include "DuckLib/string.h"
#include <stdio.h>

/*
===============
Error reporting
===============
*/

static dl_error_t duckLisp_error_push(duckLisp_t *duckLisp, const char *message, const dl_size_t message_length, const dl_size_t index, dl_bool_t throwErrors) {
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
	
	if ((source[index] != '(') || (source[stop_index - 1] != ')')) {
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
		
		if (justPopped && (bracketStack.elements_length == 0)) {	
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


static void cst_identifier_init(duckLisp_cst_identifier_t *identifier) {
	identifier->token = dl_null;
	identifier->string_index = 0;
	identifier->token_length = 0;
}

static dl_error_t cst_identifier_quit(duckLisp_t *duckLisp, duckLisp_cst_identifier_t *identifier) {
	dl_error_t e = dl_error_ok;
	
	identifier->string_index = 0;
	identifier->token_length = 0;
	e = dl_free(&duckLisp->memoryAllocation, (void **) &identifier->token);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
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
	
	identifier->string_index = index;
	identifier->token = source;
	identifier->token_length = length;
	
	l_cleanup:
	
	return e;
}


static void cst_bool_init(duckLisp_cst_bool_t *boolean) {
	boolean->token = dl_null;
	boolean->token_length = 0;
	boolean->string_index = 0;
}

static dl_error_t cst_bool_quit(duckLisp_t *duckLisp, duckLisp_cst_bool_t *boolean) {
	dl_error_t e = dl_error_ok;
	
	boolean->string_index = 0;
	boolean->token_length = 0;
	e = dl_free(&duckLisp->memoryAllocation, (void **) &boolean->token);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
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
	
	boolean->string_index = index;
	boolean->token = source;
	boolean->token_length = length;
	
	l_cleanup:
	
	return e;
}


static void cst_int_init(duckLisp_cst_integer_t *integer) {
	integer->token = dl_null;
	integer->token_length = 0;
	integer->string_index = 0;
}

static dl_error_t cst_int_quit(duckLisp_t *duckLisp, duckLisp_cst_integer_t *integer) {
	dl_error_t e = dl_error_ok;
	
	integer->string_index = 0;
	integer->token_length = 0;
	e = dl_free(&duckLisp->memoryAllocation, (void **) &integer->token);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
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
	
	integer->string_index = index;
	integer->token = source;
	integer->token_length = length;
	
	l_cleanup:
	
	return e;
}


static void cst_float_init(duckLisp_cst_float_t *floatingPoint) {
	floatingPoint->token = dl_null;
	floatingPoint->token_length = 0;
	floatingPoint->string_index = 0;
}

static dl_error_t cst_float_quit(duckLisp_t *duckLisp, duckLisp_cst_float_t *floatingPoint) {
	dl_error_t e = dl_error_ok;
	
	floatingPoint->string_index = 0;
	floatingPoint->token_length = 0;
	e = dl_free(&duckLisp->memoryAllocation, (void **) &floatingPoint->token);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
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
	
	if (index != length) {
		// eError = duckLisp_error_push(duckLisp, DL_STR("."), index);
		// e = eError ? eError : dl_error_cantHappen;
		e = dl_error_cantHappen;
		goto l_cleanup;
	}
	
	floatingPoint->string_index = index;
	floatingPoint->token = source;
	floatingPoint->token_length = length;
	
	l_cleanup:
	
	return e;
}


static void cst_number_init(duckLisp_cst_number_t *number) {
	number->type = cst_number_type_none;
}

static dl_error_t cst_number_quit(duckLisp_t *duckLisp, duckLisp_cst_number_t *number) {
	dl_error_t e = dl_error_ok;
	
	switch (number->type) {
	case cst_number_type_bool:
		e = cst_bool_quit(duckLisp, &number->value.boolean);
		break;
	case cst_number_type_int:
		e = cst_int_quit(duckLisp, &number->value.integer);
		break;
	case cst_number_type_float:
		e = cst_float_quit(duckLisp, &number->value.floatingPoint);
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
			number->type = cst_number_type_float;
		}
		number->type = cst_number_type_int;
	}
	number->type = cst_number_type_bool;
	
	l_cleanup:
	
	return e;
}


static void cst_string_init(duckLisp_cst_string_t *string) {
	string->token = dl_null;
	string->token_length = 0;
	string->string_index = 0;
}

static dl_error_t cst_string_quit(duckLisp_t *duckLisp, duckLisp_cst_string_t *string) {
	dl_error_t e = dl_error_ok;
	
	string->string_index = 0;
	string->token_length = 0;
	e = dl_free(&duckLisp->memoryAllocation, (void **) &string->token);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
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
		
		while ((index < stop_index) && (source[index] != '"')) {
			if (source[index] == '\\') {
				index++;
				
				if (index >= stop_index) {
					eError = duckLisp_error_push(duckLisp, DL_STR("Expected character in string escape sequence."), index, throwErrors);
					e = eError ? eError : dl_error_invalidValue;
					goto l_cleanup;
				}
				
				// Eat character.
			}
			
			index++;
		}
		
		if (index != length) {
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
	
	string->string_index = index;
	string->token = source;
	string->token_length = length;
	
	l_cleanup:
	
	return e;
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
		e = cst_string_quit(duckLisp, &constant->value.string);
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
		constant->type = cst_constant_type_string;
	}
	constant->type = cst_constant_type_number;
	
	l_cleanup:
	
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
		e = cst_identifier_quit(duckLisp, &compoundExpression->value.identifier);
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
			compoundExpression->type = cst_compoundExpression_type_expression;
		}
		compoundExpression->type = cst_compoundExpression_type_identifier;
	}
	compoundExpression->type = cst_compoundExpression_type_constant;
	
	l_cleanup:
	
	return e;
}


static dl_error_t cst_append(duckLisp_t *duckLisp, const int index, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	
	char *source = duckLisp->sources[index];
	const char source_length = duckLisp->source_lengths[index];
	
	e = cst_parse_compoundExpression(duckLisp, &duckLisp->cst, source, 0, source_length, throwErrors);
	if (e) {
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
	
	duckLisp->sources = dl_null;
	duckLisp->source_lengths = dl_null;
	duckLisp->sources_length = 0;
	/* No error */ cst_compoundExpression_init(&duckLisp->cst);
	/* No error */ array_init(&duckLisp->errors, &duckLisp->memoryAllocation, sizeof(duckLisp_error_t), array_strategy_fit);
	
	// duckLisp->ast.node.nodes = dl_null;
	// duckLisp->ast.node.nodes_length = 0;
	
	error = dl_error_ok;
	l_cleanup:
	
	return error;
}

void duckLisp_quit(duckLisp_t *duckLisp) {
	
	// Don't bother freeing since we are going to run `dl_memory_quit`.
	duckLisp->sources = dl_null;
	duckLisp->source_lengths = dl_null;
	duckLisp->sources_length = 0;
	duckLisp->cst.type = cst_compoundExpression_type_none;
	/* e = */ array_quit(&duckLisp->errors);
	
	/* No error */ dl_memory_quit(&duckLisp->memoryAllocation);
}

dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, dl_ptrdiff_t *script_handle, const char *source, const dl_size_t source_length) {
	dl_error_t e = dl_error_ok;
	// struct {
	// 	dl_bool_t duckLispMemory;
	// } d = {0};
	
	// dl_size_t source_length = 0;
	
	// /**/ dl_strlen(&source_length, source);
	
	/* Save copy of source. */
	
	// Create a new source element.
	e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->sources, (duckLisp->sources_length + 1) * sizeof(char *));
	if (e) {
		goto l_cleanup;
	}
	
	// Allocate some memory for the source.
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) duckLisp->sources, source_length * sizeof(char));
	if (e) {
		goto l_cleanup;
	}
	
	*script_handle = duckLisp->sources_length;
	e = dl_memcopy(duckLisp->sources[duckLisp->sources_length], source, source_length);
	if (e) {
		goto l_cleanup;
	}
	// Save copy of length.
	e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->source_lengths, (duckLisp->sources_length + 1) * sizeof(dl_size_t));
	if (e) {
		goto l_cleanup;
	}
	duckLisp->source_lengths[duckLisp->sources_length] = source_length;
	duckLisp->sources_length++;
	
	// for (dl_size_t i = 0; source[i] != '\0'; i++) {
	// 	ast_reserveTokens()
	// }
	
	e = cst_append(duckLisp, duckLisp->sources_length - 1, dl_true);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}
