
#include "duckLisp.h"
#include "DuckLib/string.h"
#include "DuckLib/array.h"

/*
======
Parser
======
*/

static void cst_compoundExpression_init(duckLisp_cst_compoundExpression_t *compoundExpression);
static dl_error_t cst_parse_compoundExpression(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length);


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
                                       const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eCleanup = dl_error_ok;
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
	
	// Quick syntax checks.
	if (index - stop_index < 2) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}
	
	if ((source[index] != '(') || (source[length - 1] != ')')) {
		e = dl_error_invalidValue;
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
	
	while (index < length) {
		// Set start index.
		/**/ dl_string_isSpace(&tempBool, source[index]);
		if ((bracketStack.elements_length == 0) && tempBool) {
			child_start_index = index;
		}
		
		// Manage brackets.
		if (source[index] == '(') {
			e = array_pushElement(&bracketStack, (void *) &source[index]);
			if (e) {
				goto l_cleanup;
			}
		}
		else if (source[index] == ')') {
			// Check for opening parenthesis.
			e = array_getTop(&bracketStack, (void *) &topChar);
			if (e) {
				goto l_cleanup;
			}
			if (topChar != '(') {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			
			// Pop opening parenthesis.
			e = array_popElement(&bracketStack, dl_null);
			if (e) {
				goto l_cleanup;
			}
		}
		else if (source[index] == '"') {
			e = array_getTop(&bracketStack, (void *) &topChar);
			if (e) {
				goto l_cleanup;
			}
			if (topChar != source[index]) {
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
		}
		
		index++;
		
		if ((bracketStack.elements_length == 0)) {	
			if (index >= length) {
				e = dl_error_bufferOverflow;
				goto l_cleanup;
			}
			
			/**/ dl_string_isSpace(&tempBool, source[index]);
			if (tempBool) {
				child_length = index - start_index;
			}
			
			// We have you now!
			
			// Allocate an expression.
			e = dl_realloc(&duckLisp->memoryAllocation, (void **) &expression->compoundExpressions,
			               expression->compoundExpressions_length * sizeof(duckLisp_cst_compoundExpression_t));
			if (e) {
				goto l_cleanup;
			}
			/**/ cst_compoundExpression_init(&expression->compoundExpressions[expression->compoundExpressions_length]);
			
			e = cst_parse_compoundExpression(duckLisp, &expression->compoundExpressions[expression->compoundExpressions_length], source, child_start_index,
			                                 child_length);
			if (e) {
				goto l_cleanup;
			}
			
			expression->compoundExpressions_length++;
		}
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
                                               const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	if (index >= length) {
		e = dl_error_bufferOverflow;
		goto l_cleanup;
	}
	
	/* No error */ dl_string_isDigit(&tempBool, source[index]);
	if (!tempBool) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}
	/* No error */ cst_isIdentifierSymbol(&tempBool, source[index]);
	if (!tempBool) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}
	index++;
	
	while (index < length) {
		
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto l_cleanup;
		}
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto l_cleanup;
		}
		/* No error */ cst_isIdentifierSymbol(&tempBool, source[index]);
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto l_cleanup;
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
                                               const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	/* No error */ dl_string_compare(&tempBool, &source[start_index], length, DL_STR("true"));
	if (!tempBool) {
		/* No error */ dl_string_compare(&tempBool, &source[start_index], length, DL_STR("false"));
		if (!tempBool) {
			e = dl_error_invalidValue;
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
                                               const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	if (index >= length) {
		e = dl_error_bufferOverflow;
		goto l_cleanup;
	}
	
	if (source[index] == '-') {
		index++;
		
		if (index >= length) {
			e = dl_error_bufferOverflow;
			goto l_cleanup;
		}
	}
	
	/* No error */ dl_string_isDigit(&tempBool, source[index]);
	if (!tempBool) {
		e = dl_error_invalidValue;
		goto l_cleanup;
	}
	
	while (index < length) {
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			e = dl_error_invalidValue;
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
                                               const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	if (index >= length) {
		e = dl_error_bufferOverflow;
		goto l_cleanup;
	}
	
	if (source[index] == '-') {
		index++;
		
		if (index >= length) {
			e = dl_error_bufferOverflow;
			goto l_cleanup;
		}
	}
	
	// Try .1
	if (source[index] == '.') {
		index++;
		
		if (index >= length) {
			e = dl_error_bufferOverflow;
			goto l_cleanup;
		}
		
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
		
		while ((index < length) && (dl_string_toLower(source[index]) != 'e')) {
			
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			
			index++;
		}
	}
	// Try 1.2, 1., and 1
	else {
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
		
		while ((index < length) && (dl_string_toLower(source[index]) != 'e') && (source[index] != '.')) {
			
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			
			index++;
		}
		
		if (source[index] == '-') {
			index++;
			
			if (index >= length) {
				e = dl_error_bufferOverflow;
				goto l_cleanup;
			}
		}
		
		while ((index < length) && (dl_string_toLower(source[index]) != 'e')) {
			
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			
			index++;
		}
	}
	
	// …e3
	if (dl_string_toLower(source[index]) == 'e') {
		index++;
		
		if (index >= length) {
			e = dl_error_bufferOverflow;
			goto l_cleanup;
		}
		
		/* No error */ dl_string_isDigit(&tempBool, source[index]);
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto l_cleanup;
		}
		index++;
		
		while (index < length) {
			/* No error */ dl_string_isDigit(&tempBool, source[index]);
			if (!tempBool) {
				e = dl_error_invalidValue;
				goto l_cleanup;
			}
			
			index++;
		}
	}
	
	if (index != length) {
		e = dl_error_invalidValue;
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
                                               const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	
	// Try parsing as a bool.
	e = cst_parse_bool(duckLisp, &number->value.boolean, source, start_index, length);
	if (e) {
		if (e != dl_error_invalidValue) {
			goto l_cleanup;
		}
		
		// Try parsing as an int.
		e = cst_parse_int(duckLisp, &number->value.integer, source, start_index, length);
		if (e) {
			if (e != dl_error_invalidValue) {
				goto l_cleanup;
			}
			
			// Try parsing as a float.
			e = cst_parse_float(duckLisp, &number->value.floatingPoint, source, start_index, length);
			if (e) {
				if (e != dl_error_invalidValue) {
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
                                   const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	dl_bool_t tempBool;
	
	if (index >= length) {
		e = dl_error_bufferOverflow;
		goto l_cleanup;
	}
	
	if (source[index] == '\'') {
		index++;
		
		if (index >= length) {
			e = dl_error_bufferOverflow;
			goto l_cleanup;
		}
		
		/*
		You know what? I'm feeling lazy, and I think I can get away with making
		everything after the `'` a string.
		*/
	}
	else if (source[index] == '"') {
		index++;
		
		while ((index < length) && (source[index] != '"')) {
			if (source[index] == '\\') {
				index++;
				
				if (index >= length) {
					e = dl_error_bufferOverflow;
					goto l_cleanup;
				}
				
				// Eat character.
			}
			
			index++;
		}
		
		if (index != length) {
			e = dl_error_bufferOverflow;
			goto l_cleanup;
		}
	}
	else {
		e = dl_error_invalidValue;
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
                                               const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	
	// Try parsing as a number.
	e = cst_parse_number(duckLisp, &constant->value.number, source, start_index, length);
	if (e) {
		if (e != dl_error_invalidValue) {
			goto l_cleanup;
		}
		
		// Try parsing as a string.
		e = cst_parse_string(duckLisp, &constant->value.string, source, start_index, length);
		if (e) {
			if (e != dl_error_invalidValue) {
				goto l_cleanup;
			}
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
                                               const dl_ptrdiff_t start_index, const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	
	// Try parsing as a constant. Do this first because of boolean values that look like identifiers.
	e = cst_parse_constant(duckLisp, &compoundExpression->value.constant, source, start_index, length);
	if (e) {
		if (e != dl_error_invalidValue) {
			goto l_cleanup;
		}
		
		// Try parsing as an identifier.
		e = cst_parse_identifier(duckLisp, &compoundExpression->value.identifier, source, start_index, length);
		if (e) {
			if (e != dl_error_invalidValue) {
				goto l_cleanup;
			}
			
			// Try parsing as an expression.
			e = cst_parse_expression(duckLisp, &compoundExpression->value.expression, source, start_index, length);
			if (e) {
				if (e != dl_error_invalidValue) {
					goto l_cleanup;
				}
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


static dl_error_t cst_append(duckLisp_t *duckLisp, const int index) {
	dl_error_t e = dl_error_ok;
	
	char *source = duckLisp->sources[index];
	const char source_length = duckLisp->source_lengths[index];
	
	e = cst_parse_compoundExpression(duckLisp, &duckLisp->cst, source, 0, source_length);
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
	
	e = cst_append(duckLisp, duckLisp->sources_length - 1);
	if (e) {
		goto l_cleanup;
	}
	
	l_cleanup:
	
	return e;
}
