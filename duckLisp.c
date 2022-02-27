
#include "duckLisp.h"
#include "DuckLib/array.h"
#include "DuckLib/core.h"
#include "DuckLib/memory.h"
#include "DuckLib/string.h"
#include "DuckLib/sort.h"
#include "DuckLib/trie.h"
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

static dl_error_t cst_parse_expression(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source, const dl_ptrdiff_t start_index,
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
		
		if (index < stop_index) /**/ dl_string_isSpace(&tempBool, source[index]);
		
		if ((bracketStack.elements_length == 0) &&
		    ((index >= stop_index) || (tempBool && !wasWhitespace) || justPopped)) {
			// if (index >= stop_index) {
			// 	eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("File ended in expression or string brackets."), index);
			// 	e = eError ? eError : dl_error_invalidValue;
			// 	goto l_cleanup;
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

static dl_error_t cst_parse_identifier(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
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

static dl_error_t cst_parse_bool(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
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

static dl_error_t cst_parse_int(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
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

static dl_error_t cst_parse_float(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
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


static void cst_string_init(duckLisp_cst_string_t *string) {
	string->token_length = 0;
	string->token_index = 0;
}

static void cst_string_quit(duckLisp_t *duckLisp, duckLisp_cst_string_t *string) {
	string->token_index = 0;
	string->token_length = 0;
}

static dl_error_t cst_parse_string(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
                                   const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
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
				// Eat character.
				index++;
				
				if (index >= stop_index) {
					eError = duckLisp_error_pushSyntax(duckLisp, DL_STR("Expected character in string escape sequence."), index, throwErrors);
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
	
	void *destination = dl_null;
	void *source = dl_null;
	char *s = dl_null;
	dl_bool_t escape = dl_false;
	
	string->value_length = 0;
	
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) &string->value, stringCST.token_length * sizeof(char));
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


static void cst_compoundExpression_init(duckLisp_cst_compoundExpression_t *compoundExpression) {
	compoundExpression->type = duckLisp_ast_type_none;
}

static dl_error_t cst_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression) {
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
		e = cst_expression_quit(duckLisp, &compoundExpression->value.expression);
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

static dl_error_t cst_parse_compoundExpression(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *compoundExpression, char *source,
                                               const dl_ptrdiff_t start_index, const dl_size_t length, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	dl_ptrdiff_t index = start_index;
	dl_ptrdiff_t stop_index = start_index + length;
	
	cst_compoundExpression_init(compoundExpression);

	typedef struct {
		dl_error_t (*reader) (duckLisp_t *, duckLisp_cst_compoundExpression_t *, char *, const dl_ptrdiff_t start_index, const dl_size_t, dl_bool_t);
		duckLisp_ast_type_t type;
	} readerStruct_t;

	readerStruct_t readerStruct[] = {
		{.reader = cst_parse_bool,       .type = duckLisp_ast_type_bool},
		{.reader = cst_parse_int,        .type = duckLisp_ast_type_int},
		{.reader = cst_parse_float,      .type = duckLisp_ast_type_float},
		{.reader = cst_parse_string,     .type = duckLisp_ast_type_string},
		{.reader = cst_parse_identifier, .type = duckLisp_ast_type_identifier},
		{.reader = cst_parse_expression, .type = duckLisp_ast_type_expression},
	};

	// We have a reader! I'll need to make it generate AST though.
	for (dl_ptrdiff_t i = 0; i < sizeof(readerStruct)/sizeof(readerStruct_t); i++) {
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

static dl_error_t cst_print_compoundExpression(duckLisp_t duckLisp, duckLisp_cst_compoundExpression_t compoundExpression) {
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

static void ast_compoundExpression_init(duckLisp_ast_compoundExpression_t *compoundExpression) {
	compoundExpression->type = duckLisp_ast_type_none;
}

static dl_error_t ast_compoundExpression_quit(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression) {
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
		e = ast_expression_quit(duckLisp, &compoundExpression->value.expression);
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

static dl_error_t ast_generate_compoundExpression(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *compoundExpression,
                                                  duckLisp_cst_compoundExpression_t compoundExpressionCST, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	
	switch (compoundExpressionCST.type) {
	case duckLisp_ast_type_bool:
		compoundExpression->type = duckLisp_ast_type_bool;
		e = ast_generate_bool(duckLisp, &compoundExpression->value.boolean, compoundExpressionCST.value.boolean, throwErrors);
		break;
	case duckLisp_ast_type_int:
		compoundExpression->type = duckLisp_ast_type_int;
		e = ast_generate_int(duckLisp, &compoundExpression->value.integer, compoundExpressionCST.value.integer, throwErrors);
		break;
	case duckLisp_ast_type_float:
		compoundExpression->type = duckLisp_ast_type_float;
		e = ast_generate_float(duckLisp, &compoundExpression->value.floatingPoint, compoundExpressionCST.value.floatingPoint, throwErrors);
		break;
	case duckLisp_ast_type_string:
		compoundExpression->type = duckLisp_ast_type_string;
		e = ast_generate_string(duckLisp, &compoundExpression->value.string, compoundExpressionCST.value.string, throwErrors);
		break;
	case duckLisp_ast_type_identifier:
		compoundExpression->type = duckLisp_ast_type_identifier;
		e = ast_generate_identifier(duckLisp, &compoundExpression->value.identifier, compoundExpressionCST.value.identifier, throwErrors);
		break;
	case duckLisp_ast_type_expression:
		compoundExpression->type = duckLisp_ast_type_expression;
		// This declares `()` == `0`
		if (compoundExpressionCST.value.expression.compoundExpressions_length == 0) {
			compoundExpression->type = duckLisp_ast_type_int;
			compoundExpression->value.integer.value = 0;
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

static dl_error_t ast_print_compoundExpression(duckLisp_t duckLisp, duckLisp_ast_compoundExpression_t compoundExpression) {
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


static dl_error_t cst_append(duckLisp_t *duckLisp, duckLisp_cst_compoundExpression_t *cst, const dl_ptrdiff_t index, dl_bool_t throwErrors) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	char *source = duckLisp->source.elements;
	dl_size_t source_length = duckLisp->source.elements_length;
	dl_bool_t isSpace;
	
	// e = dl_realloc(&duckLisp->memoryAllocation, (void **) &duckLisp->cst.compoundExpressions,
	//                (duckLisp->cst.compoundExpressions_length + 1) * sizeof(duckLisp_cst_compoundExpression_t));
	// if (e) {
	// 	goto l_cleanup;
	// }
	// duckLisp->cst.compoundExpressions_length++;
	
	// e = cst_parse_compoundExpression(duckLisp, &duckLisp->cst.compoundExpressions[duckLisp->cst.compoundExpressions_length - 1], source, index,
	//                                  source_length - index, throwErrors);
	
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

static dl_error_t ast_append(duckLisp_t *duckLisp, duckLisp_ast_compoundExpression_t *ast, duckLisp_cst_compoundExpression_t *cst, const dl_ptrdiff_t index,
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
	/**/ dl_trie_init(&scope->locals_trie, &duckLisp->memoryAllocation, -1);
	/**/ dl_trie_init(&scope->statics_trie, &duckLisp->memoryAllocation, -1);
	/**/ dl_trie_init(&scope->generators_trie, &duckLisp->memoryAllocation, -1);
	scope->generators_length = 0;
	/**/ dl_trie_init(&scope->functions_trie, &duckLisp->memoryAllocation, -1);
	scope->functions_length = 0;
	/**/ dl_trie_init(&scope->labels_trie, &duckLisp->memoryAllocation, -1);
}

dl_error_t duckLisp_pushScope(duckLisp_t *duckLisp, duckLisp_scope_t *scope) {
	if (scope == dl_null) {
		duckLisp_scope_t localScope;
		/**/ scope_init(duckLisp, &localScope);
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

/*
Failure if return value is set or index is -1.
 */
dl_error_t duckLisp_scope_getLocalIndexFromName(duckLisp_t *duckLisp, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length) {
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
		
		/**/ dl_trie_find(scope.locals_trie, index, name, name_length);
		if (*index != -1) {
			break;
		}
	}
	
	return e;
}

dl_error_t duckLisp_scope_getStaticIndexFromName(duckLisp_t *duckLisp, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length) {
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

static dl_error_t scope_getFunctionFromName(duckLisp_t *duckLisp, duckLisp_functionType_t *functionType, dl_ptrdiff_t *index, const char *name, const dl_size_t name_length) {
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
		tempPtrdiff = tempPtrdiff;
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

static dl_error_t scope_getLabelFromName(duckLisp_t *duckLisp, dl_ptrdiff_t *index, char *name, dl_size_t name_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	duckLisp_scope_t scope;
	dl_ptrdiff_t scope_index = duckLisp->scope_stack.elements_length;
	duckLisp_label_t label;
	
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
	
	l_cleanup:
	
	return e;
}

/*
========
Emitters
========
*/

dl_error_t duckLisp_emit_add(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_ptrdiff_t source_index1, const dl_ptrdiff_t source_index2) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_add;
	
	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = source_index1;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = source_index2;
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

dl_error_t duckLisp_emit_nop(duckLisp_t *duckLisp, dl_array_t *assembly) {
	
	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_nop;
	
	// Push arguments into instruction.
	
	// Push instruction into list.
	return dl_array_pushElement(assembly, &instruction);
}

dl_error_t duckLisp_emit_move(duckLisp_t *duckLisp, dl_array_t *assembly, const dl_ptrdiff_t destination_index, const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_move;
	
	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = source_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = destination_index;
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

dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp, dl_array_t *assembly, dl_ptrdiff_t *stackIndex, const dl_ptrdiff_t integer) {
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

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
	
	l_cleanup:
	
	return e;
}

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

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length;
		
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

dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp, dl_array_t *assembly, dl_ptrdiff_t *stackIndex, const dl_ptrdiff_t index) {
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

	if (stackIndex != dl_null) *stackIndex = duckLisp->locals_length++;
	
	l_cleanup:
	
	return e;
}

// We do label scoping in the emitters because scope will have no meaning during assembly.

dl_error_t duckLisp_emit_jump(duckLisp_t *duckLisp, dl_array_t *assembly, char *label, dl_size_t label_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	duckLisp_scope_t scope;
	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
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

dl_error_t duckLisp_emit_label(duckLisp_t *duckLisp, dl_array_t *assembly, char *label, dl_size_t label_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	duckLisp_scope_t scope;
	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args, &duckLisp->memoryAllocation, sizeof(duckLisp_instructionArgClass_t), dl_array_strategy_double);
	
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

/*
==========
Generators
==========
*/

dl_error_t duckLisp_generator_comment(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;

	expression->compoundExpressions_length = 0;

	return 0;
}

dl_error_t duckLisp_generator_nop(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	return duckLisp_emit_nop(duckLisp, assembly);
}

dl_error_t duckLisp_generator_label(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t string_index = -1;
	dl_array_t *assemblyFragment = dl_null;
	duckLisp_scope_t scope;
	
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
	
	e = duckLisp_emit_label(duckLisp, assembly, expression->compoundExpressions[1].value.string.value,
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
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t string_index = -1;
	dl_array_t *assemblyFragment = dl_null;
	duckLisp_scope_t scope;
	
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
	
	e = duckLisp_emit_jump(duckLisp, assembly, expression->compoundExpressions[1].value.string.value,
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

dl_error_t duckLisp_generator_pushScope(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	
	/* Check arguments for call and type errors. */
	
	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 1);
	if (e) {
		goto l_cleanup;
	}
	
	// Push a new scope.
	e = duckLisp_pushScope(duckLisp, dl_null);
	
	l_cleanup:
	
	return e;
}

dl_error_t duckLisp_generator_popScope(duckLisp_t *duckLisp, dl_array_t *assembly, duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	dl_ptrdiff_t identifier_index = -1;
	dl_ptrdiff_t string_index = -1;
	dl_array_t *assemblyFragment = dl_null;
	
	/* Check arguments for call and type errors. */
	
	e = duckLisp_checkArgsAndReportError(duckLisp, *expression, 1);
	if (e) {
		goto l_cleanup;
	}
	
	// Push a new scope.
	e = duckLisp_popScope(duckLisp, dl_null);
	
	l_cleanup:
	
	eError = dl_array_quit(&eString);
	if (eError) {
		e = eError;
	}
	
	return e;
}

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
	
//	l_cleanup:
	
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
	for (int i = 1; i < expression->compoundExpressions_length; i++) {
		switch (expression->compoundExpressions[i].type) {
		case duckLisp_ast_type_identifier:
			e = duckLisp_scope_getLocalIndexFromName(duckLisp, &argument_index,
			                                 expression->compoundExpressions[i].value.string.value,
			                                 expression->compoundExpressions[i].value.string.value_length);
			if (e || argument_index == -1) {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("callback: Could not find callback name."));
				if (eError) {
					e = eError;
				}
				goto l_cleanup;
			}
			
			e = duckLisp_emit_pushIndex(duckLisp, assembly, dl_null, argument_index);
			if (e) {
				goto l_cleanup;
			}
			break;
		case duckLisp_ast_type_int:
			e = duckLisp_emit_pushInteger(duckLisp, assembly, dl_null, expression->compoundExpressions[i].value.integer.value);
			if (e) goto l_cleanup;
			break;
		case duckLisp_ast_type_string:
			e = duckLisp_emit_pushString(duckLisp, assembly, dl_null, expression->compoundExpressions[i].value.string.value,
										 expression->compoundExpressions[i].value.string.value_length);
			if (e) goto l_cleanup;
			break;
		case duckLisp_ast_type_expression:
			/* Do nothing? */
			break;
		default:
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("callback: Unsupported expression type."));
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
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);
	
	dl_bool_t result = dl_false;
	duckLisp_scope_t scope;
	dl_ptrdiff_t label_index = -1;
	
	duckLisp_ast_expression_t newExpression;
	dl_ptrdiff_t l;
	
	// Push a new scope.
	e = duckLisp_pushScope(duckLisp, dl_null);
	if (e) {
		goto l_cleanup;
	}
	
	/* Labels */
	
	for (dl_ptrdiff_t i = 0; i < expression->compoundExpressions_length; i++) {
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
					
					/**/ dl_array_init(&label.sources, &duckLisp->memoryAllocation, sizeof(dl_ptrdiff_t), dl_array_strategy_double);
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
	
	/* Queue a `pop-scope`. */
	
	newExpression.compoundExpressions_length = expression->compoundExpressions_length + 1;
	
	e = dl_malloc(&duckLisp->memoryAllocation, (void **) &newExpression.compoundExpressions,
	              newExpression.compoundExpressions_length * sizeof(duckLisp_ast_compoundExpression_t));
	if (e) {
		goto l_cleanup;
	}
	
	// // (push-scope)
	// newExpression.compoundExpressions[0].type = ast_compoundExpression_type_expression;
	// newExpression.compoundExpressions[0].value.expression.compoundExpressions_length = 1;
	// e = dl_malloc(&duckLisp->memoryAllocation,
	//               (void **) &newExpression.compoundExpressions[0].value.expression.compoundExpressions,
	//               newExpression.compoundExpressions[0].value.expression.compoundExpressions_length * sizeof(duckLisp_ast_compoundExpression_t));
	// if (e) {
	// 	goto l_cleanup;
	// }
	// newExpression.compoundExpressions[0].value.expression.compoundExpressions[0].type = ast_compoundExpression_type_identifier;
	// newExpression.compoundExpressions[0].value.expression.compoundExpressions[0].value.identifier.value = "push-scope";
	// newExpression.compoundExpressions[0].value.expression.compoundExpressions[0].value.identifier.value_length = 10;
	
	// // Append original array of expressions.
	// Copy original arguments into the new array.
	for (dl_ptrdiff_t i = 0; i < expression->compoundExpressions_length; i++) {
		newExpression.compoundExpressions[i] = expression->compoundExpressions[i];
	}
	
	// (pop-scope)
	l = expression->compoundExpressions_length;
	newExpression.compoundExpressions[l].type = duckLisp_ast_type_expression;
	newExpression.compoundExpressions[l].value.expression.compoundExpressions_length = 1;
	e = dl_malloc(&duckLisp->memoryAllocation,
	              (void **) &newExpression.compoundExpressions[l].value.expression.compoundExpressions,
	              newExpression.compoundExpressions[l].value.expression.compoundExpressions_length * sizeof(duckLisp_ast_compoundExpression_t));
	if (e) {
		goto l_cleanup;
	}
	newExpression.compoundExpressions[l].value.expression.compoundExpressions[0].type = duckLisp_ast_type_identifier;
	newExpression.compoundExpressions[l].value.expression.compoundExpressions[0].value.identifier.value = "pop-scope";
	newExpression.compoundExpressions[l].value.expression.compoundExpressions[0].value.identifier.value_length = 9;
	
	// Set newExpression as the current expression.
	e = dl_free(&duckLisp->memoryAllocation, (void **) &expression->compoundExpressions);
	if (e) {
		goto l_cleanup;
	}
	*expression = newExpression;
	
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
	dl_ptrdiff_t source;    // Points to the array (not list) index.
	dl_ptrdiff_t target;    // Points to the array (not list) index.
	dl_uint8_t size;    // Can hold values 1-4.
	dl_bool_t forward;  // True if a forward reference.
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
	const dl_ptrdiff_t left = left_pointer->type == jumpLinkPointers_type_target ? linkArray->links[left_pointer->index].target : linkArray->links[left_pointer->index].source;
	const dl_ptrdiff_t right = right_pointer->type == jumpLinkPointers_type_target ? linkArray->links[right_pointer->index].target : linkArray->links[right_pointer->index].source;
	return left - right;
}

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
	
	typedef struct {
		dl_uint8_t byte;
		dl_ptrdiff_t next;
		dl_ptrdiff_t prev;
	} byteLink_t;
	
	dl_array_t bytecodeList; // byteLink_t
	/**/ dl_array_init(&bytecodeList, &duckLisp->memoryAllocation, sizeof(byteLink_t), dl_array_strategy_double);
	
	byteLink_t tempByteLink;
	
	// dl_array_t bytecode; // unsigned char
	/**/ dl_array_init(bytecode, &duckLisp->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);
	
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
	
	// Bootstrap.
	currentExpression.type = duckLisp_ast_type_expression;
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
			if (currentExpression.value.expression.compoundExpressions[j].type == duckLisp_ast_type_expression) {
				
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
		case duckLisp_ast_type_bool:
		case duckLisp_ast_type_int:
		case duckLisp_ast_type_float:
		case duckLisp_ast_type_string:
			e = dl_error_invalidValue;
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Constants as function names are not supported."));
			if (eError) {
				e = eError;
			}
			goto l_cleanup;
		case duckLisp_ast_type_identifier:
			// Run function generator.
			functionName = currentExpression.value.expression.compoundExpressions[0].value.identifier;
			e = scope_getFunctionFromName(duckLisp, &functionType, &functionIndex, functionName.value, functionName.value_length);
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
		case duckLisp_ast_type_expression:
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
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < instructionList.elements_length; i++) {
		ia = DL_ARRAY_GETADDRESS(instructionList, dl_array_t, i);
		printf("{\n");
		for (dl_ptrdiff_t j = 0; (dl_size_t) j < ia.elements_length; j++) {
			io = DL_ARRAY_GETADDRESS(ia, duckLisp_instructionObject_t, j);
			printf("    {\n");
			printf("        Instruction class: %s\n",
				(char *[9]){
					"nop",
					"pushString",
					"pushInteger",
					"pushIndex",
					"ccall",
					"jump",
					"move",
					"add",
					"label",
				}[io.instructionClass]);
			printf("        [\n");
			duckLisp_instructionArgClass_t ia;
			for (dl_ptrdiff_t k = 0; (dl_size_t) k < io.args.elements_length; k++) {
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
	
	/*	typedef struct{
		dl_ptrdiff_t 
	} jumpLink_t;*/
	
	dl_ptrdiff_t byte_index = 0;
	byteLink_t currentInstruction;
	dl_array_t currentArgs; // unsigned char
	linkArray_t linkArray = {0};
	currentInstruction.prev = -1;
	/**/ dl_array_init(&currentArgs, &duckLisp->memoryAllocation, sizeof(unsigned char), dl_array_strategy_double);
	for (dl_ptrdiff_t i = instructionList.elements_length - 1; i >= 0; --i) {
		dl_array_t instructions = DL_ARRAY_GETADDRESS(instructionList, dl_array_t, i);
		for (dl_ptrdiff_t j = 0; (dl_size_t) j < instructions.elements_length; j++) {
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
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = (args[0].value.index >> 8*n) & 0xFFU;
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
			case duckLisp_instructionClass_pushInteger: {
				switch (args[0].type) {
				case duckLisp_instructionArgClass_type_integer:
					if ((unsigned long) args[0].value.integer < 0x100UL) {
						currentInstruction.byte = duckLisp_instruction_pushInteger8;
						byte_length = 1;
					}
					else if ((unsigned int) args[0].value.integer < 0x10000UL) {
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
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = (args[0].value.integer >> 8*n) & 0xFFU;
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
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = (args[0].value.integer >> 8*n) & 0xFFU;
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
			case duckLisp_instructionClass_move: {
				if ((args[0].type == duckLisp_instructionArgClass_type_index) && (args[1].type == duckLisp_instructionArgClass_type_index)) {
					if (((unsigned long) args[0].value.index < 0x100UL) && ((unsigned long) args[1].value.index < 0x100UL)) {
						currentInstruction.byte = duckLisp_instruction_move8;
						byte_length = 1;
					}
					else if (((unsigned int) args[0].value.index < 0x10000UL) && ((unsigned int) args[1].value.index < 0x10000UL)) {
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
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = (args[0].value.index >> 8*n) & 0xFFU;
					}
					for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = (args[1].value.index >> 8*n) & 0xFFU;
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
				if ((args[0].type == duckLisp_instructionArgClass_type_index) && (args[1].type == duckLisp_instructionArgClass_type_index)) {
					if (((unsigned long) args[0].value.index < 0x100UL) && ((unsigned long) args[1].value.index < 0x100UL)) {
						currentInstruction.byte = duckLisp_instruction_add8;
						byte_length = 1;
					}
					else if (((unsigned int) args[0].value.index < 0x10000UL) && ((unsigned int) args[1].value.index < 0x10000UL)) {
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
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = (args[0].value.index >> 8*n) & 0xFFU;
					}
					for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = (args[1].value.index >> 8*n) & 0xFFU;
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
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = (args[0].value.integer >> 8*n) & 0xFFU;
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
			// @TODO: Redo scoping. Tries in parent scopes need to be searched as well.
			case duckLisp_instructionClass_pseudo_label:
			case duckLisp_instructionClass_jump: {
				duckLisp_scope_t scope;
				dl_ptrdiff_t label_index = -1;
				duckLisp_label_t label;
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
				else {
					tempPtrdiff++;  // `++` for opcode. This is so the optimizer can be used with generic address links.
					e = dl_array_pushElement(&label.sources, &tempPtrdiff);
					if (e) {
						goto l_cleanup;
					}
					linkArray.links_length++;
				}
				DL_ARRAY_GETADDRESS(duckLisp->labels, duckLisp_label_t, label_index) = label;
				
				if (instruction.instructionClass == duckLisp_instructionClass_pseudo_label) {
					continue;
				}
				else {
					// First guess: Jump is < 128 B away.
					currentInstruction.byte = duckLisp_instruction_jump8;
					// currentInstruction.byte = duckLisp_instruction_nop;
					/* e = dl_array_pushElements(&currentArgs, dl_null, 1); */
					/* if (e) { */
					/* 	goto l_cleanup; */
					/* } */
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
			for (dl_ptrdiff_t k = 0; k < currentArgs.elements_length; k++) {
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
	}
	if (bytecodeList.elements_length > 0) {
		DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = -1;
	}
	
	// Resolve jumps here.

	if (linkArray.links_length) {
		e = dl_malloc(&duckLisp->memoryAllocation, (void **) &linkArray.links, linkArray.links_length * sizeof(jumpLink_t));
		if (e) goto l_cleanup;
	
		{
			dl_ptrdiff_t index = 0;
			for (dl_ptrdiff_t i = 0; i < duckLisp->labels.elements_length; i++) {
				duckLisp_label_t label = DL_ARRAY_GETADDRESS(duckLisp->labels, duckLisp_label_t, i);
				for (dl_ptrdiff_t j = 0; j < label.sources.elements_length; j++) {
					linkArray.links[index].target = label.target;
					linkArray.links[index].source = DL_ARRAY_GETADDRESS(label.sources, dl_ptrdiff_t, j);
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
		
		/* Create a copy of the original linkArray. This gives us a one-to-one mapping of the new goto addresses to the current goto addresses. */
		
		linkArray_t newLinkArray;
		newLinkArray.links_length = linkArray.links_length;
		e = dl_malloc(&duckLisp->memoryAllocation, (void **) &newLinkArray.links, newLinkArray.links_length * sizeof(jumpLink_t));
		if (e) goto l_cleanup;
		
		/**/ dl_memcopy_noOverlap(newLinkArray.links, linkArray.links, linkArray.links_length * sizeof(jumpLink_t));
		
		/* Create array double the size as jumpLink. */
		
		jumpLinkPointer_t *jumpLinkPointers = dl_null;
		e = dl_malloc(&duckLisp->memoryAllocation, (void **) &jumpLinkPointers, 2 * linkArray.links_length * sizeof(jumpLinkPointer_t));
		if (e) goto l_cleanup;
		
		
		/* Fill array with each jumpLink index and index type. At the same time, sort array so that indexes in jumpLink are in ascending order. */
		for (dl_ptrdiff_t i = 0; i < linkArray.links_length; i++) {
			jumpLinkPointers[i].index = i;
			jumpLinkPointers[i].type = jumpLinkPointers_type_address;
		}
		
		for (dl_ptrdiff_t i = 0; i < linkArray.links_length; i++) {
			jumpLinkPointers[i + linkArray.links_length].index = i;
			jumpLinkPointers[i + linkArray.links_length].type = jumpLinkPointers_type_target;
		}
		
		/* I suspect a simple linked list would have been faster than all this junk. */
		
		/**/ quicksort_hoare(jumpLinkPointers, 2 * linkArray.links_length, sizeof(jumpLinkPointer_t), 0, 2 * linkArray.links_length - 1, jumpLink_less, &linkArray);
		
		for (dl_ptrdiff_t i = 0; i < 2 * linkArray.links_length; i++) {
			printf("%lld %u  ", jumpLinkPointers[i].index, jumpLinkPointers[i].type);
		}
		putchar('\n');
		putchar('\n');
		
		/* Optimize addressing size. */
		
		dl_ptrdiff_t offset;
		do {
			printf("\n");
			offset = 0;
			for (dl_ptrdiff_t j = 0; j < 2 * newLinkArray.links_length; j++) {
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
					printf("t %lli  index l%lli  offset %lli\n", link.target, index, offset);
				}
				else {
					link.source += offset;
					
					/* Range calculation */
					/* difference = jumpLink[i].target - (jumpLink.address[i] + jumpLink.size[i]); */
					difference = link.target - (link.source + link.size);
					
					/* Size calculation */
					if ((DL_INT8_MAX >= difference) || (difference >= DL_INT8_MIN)) {
						newSize = 1;  /* +1 for opcode. */
					}
					else if ((DL_INT16_MAX >= difference) || (difference >= DL_INT16_MIN)) {
						newSize = 2;
					}
					else {
						newSize = 4;
					}
					/* if (newSize > jumpLink[i].size) { */
					/* 	jumpLink[i].size = newSize; */
					/* 	sizeChanged = dl_true; */
					/* } */
					printf("t %lli  index j%lli  offset %lli  difference %lli  size %u  newSize %u\n", link.source, index, offset, difference, link.size, newSize);
					if (newSize > link.size) {
						offset += newSize - link.size;
						link.size = newSize;
					}
				}
				newLinkArray.links[index] = link;
			}
		} while (offset != 0);
		putchar('\n');
		
		for (dl_ptrdiff_t i = 0; i < linkArray.links_length; i++) {
			printf("%s%lli⇒%lli ; ", linkArray.links[i].forward ? "f" : "", linkArray.links[i].source, linkArray.links[i].target);
		}
		printf("\n");
		
		for (dl_ptrdiff_t i = 0; i < newLinkArray.links_length; i++) {
			printf("%s%lli⇒%lli ; ", newLinkArray.links[i].forward ? "f" : "", newLinkArray.links[i].source, newLinkArray.links[i].target);
		}
		printf("\n\n");
		
		
		/* Insert addresses into bytecode. */
		
		for (dl_ptrdiff_t i = 0; i < linkArray.links_length; i++) {
			/* The bytecode list is a linked list, but there is no problem
			   addressing it as an array since the links were inserted
			   in order. Additional links will be placed on the end of
			   the array and will not affect the indices of the earlier links. */
			
			dl_ptrdiff_t base_address = linkArray.links[i].source - 1;  // ` - 1` because we want to insert the links *in place of* the target link.
			dl_bool_t array_end = DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).next == -1;
			
			if (newLinkArray.links[i].size == 1) {
			}
			else if (newLinkArray.links[i].size == 2) {
				DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).byte += 1;
			}
			else {
				DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).byte += 2;
			}
			
			for (dl_uint8_t j = 1; j <= newLinkArray.links[i].size; j++) {
				byteLink_t byteLink;
				byteLink.byte = ((newLinkArray.links[i].target - (newLinkArray.links[i].source + newLinkArray.links[i].size)) >> 8*(j - 1)) & 0xFFU;
				byteLink.prev = base_address + j - 1;
				byteLink.next = linkArray.links[i].source;
				
				DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address + j - 1).next = bytecodeList.elements_length;
				DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address + j).prev = bytecodeList.elements_length;
				
				e = dl_array_pushElement(&bytecodeList, &byteLink);
				if (e) goto l_cleanup;
			}

			/* DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).next */
			/* DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address + 1).prev = bytecodeList.elements_length; */
			if (array_end) DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, bytecodeList.elements_length - 1).next = -1;
		}
		
#endif
		// Clean up, clean up, everybody do your share…
		e = dl_free(&duckLisp->memoryAllocation, (void *) &newLinkArray.links);
		if (e) goto l_cleanup;
		e = dl_free(&duckLisp->memoryAllocation, (void *) &linkArray.links);
		if (e) goto l_cleanup;
		e = dl_free(&duckLisp->memoryAllocation, (void *) &jumpLinkPointers);
		if (e) goto l_cleanup;
	} /* End address space optimization. */

	
	// Adjust the opcodes for the address size and set address.
	// i.e. rewrite the whole instruction.
	
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
	dl_uint8_t tempChar = duckLisp_instruction_return;
	e = dl_array_pushElement(bytecode, &tempChar);
	if (e) {
		goto l_cleanup;
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
	
	// All language-defined generators go here.
	struct {
		const char *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckLisp_t*, dl_array_t*, duckLisp_ast_expression_t*);
	} generators[] = {
		{DL_STR("comment"),     duckLisp_generator_comment},
		{DL_STR("nop"),         duckLisp_generator_nop},
		{DL_STR("push-scope"),  duckLisp_generator_pushScope},
		{DL_STR("pop-scope"),   duckLisp_generator_popScope},
		{DL_STR("goto"),        duckLisp_generator_goto},
		{DL_STR("label"),       duckLisp_generator_label},
		{dl_null, 0,            dl_null}
	};
	
	error = dl_memory_init(&duckLisp->memoryAllocation, memory, size, dl_memoryFit_best);
	if (error) {
		goto l_cleanup;
	}
	
	/* No error */ dl_array_init(&duckLisp->source, &duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_fit);
	// /* No error */ cst_expression_init(&duckLisp->cst);
	// /* No error */ ast_expression_init(&duckLisp->ast);
	/* No error */ dl_array_init(&duckLisp->errors, &duckLisp->memoryAllocation, sizeof(duckLisp_error_t), dl_array_strategy_fit);
	// /* No error */ dl_array_init(&duckLisp->stack, &duckLisp->memoryAllocation, sizeof(duckLisp_object_t), dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->scope_stack, &duckLisp->memoryAllocation, sizeof(duckLisp_scope_t), dl_array_strategy_fit);
	/* No error */ dl_array_init(&duckLisp->bytecode, &duckLisp->memoryAllocation, sizeof(dl_uint8_t), dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->generators_stack, &duckLisp->memoryAllocation,sizeof(dl_error_t (*)(duckLisp_t*, duckLisp_ast_expression_t*)),
	                             dl_array_strategy_double);
	/* No error */ dl_array_init(&duckLisp->labels, &duckLisp->memoryAllocation, sizeof(duckLisp_label_t), dl_array_strategy_double);
	
	duckLisp->locals_length = 0;
	duckLisp->statics_length = 0;
	
	for (dl_ptrdiff_t i = 0; generators[i].name != dl_null; i++) {
		error = duckLisp_addGenerator(duckLisp, generators[i].callback, generators[i].name, generators[i].name_length);
		if (error) {
			printf("Could not register generator. (%s)\n", dl_errorString[error]);
		}
	}
	
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
dl_error_t duckLisp_loadString(duckLisp_t *duckLisp, unsigned char **bytecode, dl_size_t *bytecode_length,
                               char *source, const dl_size_t source_length) {
	dl_error_t e = dl_error_ok;
	// struct {
	// 	dl_bool_t duckLispMemory;
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
	
	e = dl_array_pushElements(&duckLisp->source, (char *) source, source_length);
	if (e) {
		goto l_cleanup;
	}
	
	/* Parse. */
	
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
	
	e = compile(duckLisp, &bytecodeArray, ast);
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
	duckLisp->locals_length++;
	
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


char *duckLisp_disassemble(dl_memoryAllocation_t *memoryAllocation, const dl_uint8_t *bytecode, const dl_size_t length) {
	dl_error_t e = dl_error_ok;

	dl_array_t disassembly;
	/**/ dl_array_init(&disassembly, memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_uint8_t opcode;
	dl_ptrdiff_t arg = 0;
	dl_uint8_t tempChar;
	dl_size_t tempSize;
	for (dl_ptrdiff_t i = 0; i < length; i++) {
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
				e = dl_array_pushElements(&disassembly, DL_STR("push-string.8  "));
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
		case duckLisp_instruction_pushInteger8:
			switch (arg) {
			case 0: 
				e = dl_array_pushElements(&disassembly, DL_STR("push-integer.8 "));
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
		case duckLisp_instruction_pushIndex8:
			switch (arg) {
			case 0: 
				e = dl_array_pushElements(&disassembly, DL_STR("push-index.8   "));
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
		case duckLisp_instruction_call8:
			switch (arg) {
			case 0: 
				e = dl_array_pushElements(&disassembly, DL_STR("call.8 "));
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
		case duckLisp_instruction_ccall8:
			switch (arg) {
			case 0: 
				e = dl_array_pushElements(&disassembly, DL_STR("c-call.8       "));
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
		case duckLisp_instruction_jump8:
			switch (arg) {
			case 0: 
				e = dl_array_pushElements(&disassembly, DL_STR("jump.8         "));
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
		case duckLisp_instruction_move8:
			switch (arg) {
			case 0: 
				e = dl_array_pushElements(&disassembly, DL_STR("move.8         "));
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
				e = dl_array_pushElements(&disassembly, DL_STR("move.16        "));
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
				e = dl_array_pushElements(&disassembly, DL_STR("move.32        "));
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

		case duckLisp_instruction_add8:
			switch (arg) {
			case 0: 
				e = dl_array_pushElements(&disassembly, DL_STR("add.8          "));
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
				e = dl_array_pushElements(&disassembly, DL_STR("add.16        "));
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
				e = dl_array_pushElements(&disassembly, DL_STR("add.32         "));
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

		case duckLisp_instruction_return:
			e = dl_array_pushElements(&disassembly, DL_STR("return\n"));
			if (e) return dl_null;
			arg = 0;
			continue;
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
