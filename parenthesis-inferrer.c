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

#include "parenthesis-inferrer.h"
#include "DuckLib/string.h"
#include "duckLisp.h"
#include "duckVM.h"
#include "parser.h"


/* This contains the parenthesis inferrer for duck-lisp. When Forth-like syntax mode is enabled the raw AST from the
   reader is only partially complete. Forth syntax is not context free, so another stage (this one) is desirable. It
   *is* possible to implement the algorithm entirely in duck-lisp's recursive descent parser, but keeping the inferrer
   separate from the parser improves modularity and readability. The algorithm used for tree correction is similar to
   Forth's algorithm, the primary difference being that this variant requires a static type system since it operates on
   the entire tree at compile-time.

   Basic rules:
   * Literal value types are not touched: bool, int, float, string
   * Parenthesized expressions are not touched. Use of parentheses for a function call opts out of inference for that
     call. Parentheses are still inferred for the arguments if appropriate and the arity of the form is checked against
     the type.
   * Callbacks are converted to identifiers. Callbacks are not candidates for inference.
   * Literal expressions are converted to expressions. Neither literal expressions nor their arguments are candidates
     for inference.
   * Identifiers that do not occur at the start of a parenthesized expression may be candidates for inference.

   When an identifier that is a candidate for inference is encountered, its name is looked up in the function dictionary
   stack. If it was not found, then the identifier is assumed not to be a function call and inference is stopped. If it
   was found, then the type is returned.

   There are two basic types:

   I: Infer -- Inference will be run on this argument.
   L: Literal -- Inference will not be run on this argument or its sub-forms.

   Normal variables have the type `L`. Functions have a list as their type. It can be empty, or it can contain a
   combination of `L` and `I`. Variadic functions are indicated by `&rest` in the third-to-last position of the
   type. The default number of arguments for a variadic function is in the second-to-last position.
   Some examples: `()`, `(I I I)`, `(L I)`, `(L L L &rest 1 I)`.

   Types are declared with the keyword `declare`. It accepts two arguments by default.

   declare * (I I)
   declare if (I I I)
   declare setq (L I)

   Unfortunately the use of these static type declarations is limited due to the existence of macros. To allow the type
   system to understand macros such as `defun`, `declare` can be passed a duck-lisp script. When `declare` is passed
   four arguments, the last argument is interpreted as the body of the script. When the declared function is used in a
   call, the script should parse and analyze the arguments in order to declare additional identifiers used by arguments
   or by forms that occur in the same declaration scope. The duck-lisp scripts that occur in the body of a `declare`
   form are run at inference-time. This is a separate instance of the duck-lisp compiler and VM than is used in the rest
   of the language. This instance is used solely in the inferrer.

   The inference-time instance of duck-lisp is defined with three additional functions:

   (infer-and-get-next-argument)::Any -- C callback -- Switches to the next argument and runs inference on the current
   argument. Returns the resulting AST.
   (declare-identifier name::(Symbol String) type::(Symbol List))::Nil -- C callback -- Declares the provided symbol
   `name` as an identifier in the current declaration scope with a type specified by `type`.
   (declaration-scope body::Any*)::Any -- Generator -- Create a new declaration scope. Identifiers declared in the
   body using `declare-identifier` are automatically deleted when the scope is exited.

   Examples:

   ;; `var' itself makes a declaration, so it requires a script. This declaration will persist until the end of the
   ;; scope `var' was used in.
   (declare var (L I)
            (declare-identifier (infer-and-get-next-argument)
                                (quote #L)))

   ;; `defmacro' declares each of its parameters as a normal variable, and the provided name and `self` as functions of
   ;; the specified type. `self` and the parameters are scoped to the body while the declaration of the macro itself
   ;; persists until the end of the scope `defmacro' was used in.
   (declare defmacro (L L L &rest 1 I)
            (
             (var name (infer-and-get-next-argument))
             (var parameters (infer-and-get-next-argument))
             (var type (infer-and-get-next-argument))
             (declaration-scope
              (while parameters
                     var parameter car parameters
                     (unless (= (quote #&rest) parameter)
                             (declare-identifier parameter (quote #L)))
                     (setq parameters (cdr parameters)))
              (declare-identifier (quote #self) type)
              (infer-and-get-next-argument))
             (declare-identifier name type)))

   (declare let ((&rest 1 (L I)) &rest 1 I)
            ;; The bindings of the `let' statement is the first argument. It will be inferred using the type above.
            (var bindings (infer-and-get-next-argument))
            ;; Create a new scope. All declarations in the scope will be deleted when the scope exits.
            (declaration-scope
             (dolist binding bindings
                     ;; Declare each new identifier in the `let' as a variable.
                     (declare-identifier (first binding) (quote L)))
             ;; Infer body in scope.
             (infer-and-get-next-argument)))

   This system cannot recognize some macros due to the simplicity of the parsing functions used in inference-time
   scripts. It can correctly infer a complicated form like `let`, but is unable to infer `let*`. */

dl_error_t duckLisp_parenthesisInferrer_declarationPrototype_prettyPrint(dl_array_t *string_array,
                                                                         duckLisp_parenthesisInferrer_declarationPrototype_t declarationPrototype) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(duckLisp_parenthesisInferrer_declarationPrototype_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_uint8_t name["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, declarationPrototype.name_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = "));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, declarationPrototype.name, declarationPrototype.name_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_uint8_t type["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, declarationPrototype.type_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = "));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, declarationPrototype.type, declarationPrototype.type_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_uint8_t script["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, declarationPrototype.script_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = "));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, declarationPrototype.script, declarationPrototype.script_length);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}

#ifdef USE_PARENTHESIS_INFERENCE

typedef enum {
	inferrerTypeSymbol_L,
	inferrerTypeSymbol_I
} inferrerTypeSymbol_t;

#if 0
static dl_error_t inferrerTypeSymbol_prettyPrint(dl_array_t *string_array, inferrerTypeSymbol_t inferrerTypeSymbol) {
	switch (inferrerTypeSymbol) {
	case inferrerTypeSymbol_L:
		return dl_array_pushElements(string_array, DL_STR("inferrerTypeSymbol_L"));
	case inferrerTypeSymbol_I:
		return dl_array_pushElements(string_array, DL_STR("inferrerTypeSymbol_I"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}
#endif

typedef enum {
	inferrerTypeSignature_type_expression,
	inferrerTypeSignature_type_symbol
} inferrerTypeSignature_type_t;

#if 0
static dl_error_t inferrerTypeSignature_type_prettyPrint(dl_array_t *string_array,
                                                  inferrerTypeSignature_type_t inferrerTypeSignature_type) {
	switch (inferrerTypeSignature_type) {
	case inferrerTypeSignature_type_expression:
		return dl_array_pushElements(string_array, DL_STR("inferrerTypeSignature_type_expression"));
	case inferrerTypeSignature_type_symbol:
		return dl_array_pushElements(string_array, DL_STR("inferrerTypeSignature_type_symbol"));
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}
#endif

typedef struct inferrerTypeSignature_s {
	inferrerTypeSignature_type_t type;
	union {
		struct {
			/* '(L L I) or '(L L &rest 2 I) or '((&rest 1 (L I)) &rest 1 I) &c. */
			struct inferrerTypeSignature_s *positionalSignatures;
			dl_size_t positionalSignatures_length;
			struct inferrerTypeSignature_s *restSignature;  /* Contains a max of one element */
			dl_ptrdiff_t defaultRestLength;
			dl_bool_t variadic;
		} expression;
		/* 'I or 'L */
		inferrerTypeSymbol_t symbol;
	} value;
} inferrerTypeSignature_t;

#if 0
static dl_error_t inferrerTypeSignature_prettyPrint(dl_array_t *string_array,
                                                    inferrerTypeSignature_t inferrerTypeSignature) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(inferrerTypeSignature_t) {"));
	if (e) goto cleanup;

	if (inferrerTypeSignature.type == inferrerTypeSignature_type_symbol) {
		e = inferrerTypeSymbol_prettyPrint(string_array, inferrerTypeSignature.value.symbol);
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("("));
		if (e) goto cleanup;

		DL_DOTIMES(j, inferrerTypeSignature.value.expression.positionalSignatures_length) {
			e = inferrerTypeSignature_prettyPrint(string_array,
			                                      inferrerTypeSignature.value.expression.positionalSignatures[j]);
			if (e) goto cleanup;
			if ((dl_size_t) j != inferrerTypeSignature.value.expression.positionalSignatures_length - 1) {
				e = dl_array_pushElements(string_array, DL_STR(" "));
				if (e) goto cleanup;
			}
		}

		if (inferrerTypeSignature.value.expression.variadic) {
			e = dl_array_pushElements(string_array, DL_STR(" "));
			if (e) goto cleanup;

			e = dl_string_fromPtrdiff(string_array, inferrerTypeSignature.value.expression.defaultRestLength);
			if (e) goto cleanup;

			e = dl_array_pushElements(string_array, DL_STR(" "));
			if (e) goto cleanup;

			e = inferrerTypeSignature_prettyPrint(string_array,
			                                      *inferrerTypeSignature.value.expression.restSignature);
			if (e) goto cleanup;
		}

		e = dl_array_pushElements(string_array, DL_STR(")"));
		if (e) goto cleanup;
	}

 cleanup: return e;
}
#endif


typedef struct {
	dl_uint8_t *bytecode;
	dl_size_t bytecode_length;
	inferrerTypeSignature_t type;
} inferrerType_t;

#if 0
static dl_error_t inferrerType_prettyPrint(dl_array_t *string_array, inferrerType_t inferrerType) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(inferrerType_t) {"));
	if (e) goto cleanup;

	e = inferrerTypeSignature_prettyPrint(string_array, inferrerType.type);
	if (e) goto cleanup;

	e = duckVM_bytecode_prettyPrint(string_array, inferrerType.bytecode);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif

typedef struct {
	dl_trie_t identifiersTrie;  /* A:int-->types[A],bytecode[A] */
	dl_array_t types;  /* dl_array_t:inferrerType_t */
} inferrerScope_t;

#if 0
static dl_error_t inferrerScope_prettyPrint(dl_array_t *string_array, inferrerScope_t inferrerScope) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(inferrerScope_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("identifiersTrie = "));
	if (e) goto cleanup;
	e = dl_trie_prettyPrint(string_array, inferrerScope.identifiersTrie);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("types = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, inferrerScope.types.elements_length) {
		e = inferrerType_prettyPrint(string_array, DL_ARRAY_GETADDRESS(inferrerScope.types, inferrerType_t, i));
		if (e) goto cleanup;
		if ((dl_size_t) i != inferrerScope.types.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif

typedef struct {
	dl_memoryAllocation_t *memoryAllocation;
	duckLisp_t duckLisp;
	duckVM_t duckVM;
	dl_array_t *errors;  /* dl_array_t:duckLisp_error_t */
	dl_array_t *log;  /* dl_array_t:dl_uint8_t */
	const dl_uint8_t *fileName;
	dl_size_t fileName_length;
	dl_array_t scopeStack;  /* dl_array_t:inferrerScope_t */
} inferrerState_t;

#if 0
static dl_error_t inferrerState_prettyPrint(dl_array_t *string_array, inferrerState_t inferrerState) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(inferrerState_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("duckLisp = "));
	if (e) goto cleanup;
	e = duckLisp_prettyPrint(string_array, inferrerState.duckLisp);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("duckVM = "));
	if (e) goto cleanup;
	e = duckVM_prettyPrint(string_array, inferrerState.duckVM);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("error = "));
	if (e) goto cleanup;
	if (inferrerState.errors == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("{"));
		if (e) goto cleanup;

		DL_DOTIMES(i, inferrerState.errors->elements_length) {
			e = duckLisp_error_prettyPrint(string_array,
			                               DL_ARRAY_GETADDRESS(*inferrerState.errors, duckLisp_error_t, i));
			if (e) goto cleanup;
			if ((dl_size_t) i != inferrerState.errors->elements_length - 1) {
				e = dl_array_pushElements(string_array, DL_STR(", "));
				if (e) goto cleanup;
			}
		}

		e = dl_array_pushElements(string_array, DL_STR("}"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("log = "));
	if (e) goto cleanup;
	if (inferrerState.log == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("\""));
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, inferrerState.log->elements, inferrerState.log->elements_length);
		if (e) goto cleanup;
		e = dl_array_pushElements(string_array, DL_STR("\""));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("fileName = \""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, inferrerState.fileName, inferrerState.fileName_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("scopeStack = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, inferrerState.scopeStack.elements_length) {
		e = inferrerScope_prettyPrint(string_array, DL_ARRAY_GETADDRESS(inferrerState.scopeStack, inferrerScope_t, i));
		if (e) goto cleanup;
		if ((dl_size_t) i != inferrerState.scopeStack.elements_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif

typedef struct {
	inferrerState_t *state;
	dl_uint8_t *fileName;
	dl_size_t fileName_length;
	inferrerType_t type;
	dl_ptrdiff_t *type_index;
	duckLisp_ast_expression_t *expression;
	dl_ptrdiff_t *expression_index;
	dl_array_t *newExpression;  /* dl_array_t:duckLisp_ast_compoundExpression_t */
	dl_size_t *newLength;
	dl_bool_t parenthesized;
	dl_bool_t infer;
} vmContext_t;

#if 0
static dl_error_t vmContext_prettyPrint(dl_array_t *string_array, vmContext_t vmContext) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(vmContext_t) {"));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("state = "));
	if (e) goto cleanup;
	if (vmContext.state == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = inferrerState_prettyPrint(string_array, *vmContext.state);
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("fileName = \""));
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, vmContext.fileName, vmContext.fileName_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("\""));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("type = "));
	if (e) goto cleanup;
	e = inferrerType_prettyPrint(string_array, vmContext.type);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("type_index = "));
	if (e) goto cleanup;
	if (vmContext.type_index == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
	}
	else {
		e = dl_string_fromPtrdiff(string_array, *vmContext.type_index);
	}
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("expression = "));
	if (e) goto cleanup;
	if (vmContext.expression == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
	}
	else {
		e = duckLisp_ast_expression_prettyPrint(string_array, *vmContext.expression);
	}
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("expression_index = "));
	if (e) goto cleanup;
	if (vmContext.expression_index == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
	}
	else {
		e = dl_string_fromPtrdiff(string_array, *vmContext.expression_index);
	}
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("newExpression = "));
	if (e) goto cleanup;
	if (vmContext.newExpression == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(string_array, DL_STR("{"));
		if (e) goto cleanup;

		DL_DOTIMES(i, vmContext.newExpression->elements_length) {
			e = duckLisp_ast_compoundExpression_prettyPrint(string_array, DL_ARRAY_GETADDRESS(*vmContext.newExpression, duckLisp_ast_compoundExpression_t, i));
			if (e) goto cleanup;
			if ((dl_size_t) i != vmContext.newExpression->elements_length - 1) {
				e = dl_array_pushElements(string_array, DL_STR(", "));
				if (e) goto cleanup;
			}
		}

		e = dl_array_pushElements(string_array, DL_STR("}"));
		if (e) goto cleanup;
	}

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("newLength = "));
	if (e) goto cleanup;
	if (vmContext.newLength == dl_null) {
		e = dl_array_pushElements(string_array, DL_STR("NULL"));
	}
	else {
		e = dl_string_fromSize(string_array, *vmContext.newLength);
	}
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("parenthesized = "));
	if (e) goto cleanup;
	e = dl_string_fromBool(string_array, vmContext.parenthesized);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("infer = "));
	if (e) goto cleanup;
	e = dl_string_fromBool(string_array, vmContext.infer);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif


static dl_error_t duckLisp_error_pushInference(inferrerState_t *inferrerState,
                                               const dl_uint8_t *message,
                                               const dl_size_t message_length);


static dl_error_t inferrerState_log(inferrerState_t *inferrerState,
                                    const dl_uint8_t *message,
                                    const dl_size_t message_length) {
	return dl_array_pushElements(inferrerState->log, message, message_length);
}


static void inferrerTypeSignature_init(inferrerTypeSignature_t *inferrerTypeSignature) {
	inferrerTypeSignature->type = inferrerTypeSignature_type_symbol;
	inferrerTypeSignature->value.symbol = inferrerTypeSymbol_L;
}

static dl_error_t inferrerTypeSignature_quit(inferrerTypeSignature_t *inferrerTypeSignature,
                                             dl_memoryAllocation_t *memoryAllocation) {
	dl_error_t e = dl_error_ok;
	if (inferrerTypeSignature == dl_null) {
		/* OK */
		return e;
	}
	if (inferrerTypeSignature->type == inferrerTypeSignature_type_symbol) {
		/* OK */
		return e;
	}

	if (inferrerTypeSignature->value.expression.variadic) {
		e = inferrerTypeSignature_quit(inferrerTypeSignature->value.expression.restSignature, memoryAllocation);
		if (e) return e;
		e = DL_FREE(memoryAllocation, &inferrerTypeSignature->value.expression.restSignature);
		if (e) return e;
	}
	inferrerTypeSignature->value.expression.variadic = dl_false;
	inferrerTypeSignature->value.expression.defaultRestLength = 0;

	DL_DOTIMES(i, inferrerTypeSignature->value.expression.positionalSignatures_length) {
		e = inferrerTypeSignature_quit(inferrerTypeSignature->value.expression.positionalSignatures + i,
		                               memoryAllocation);
		if (e) break;
	}
	e = DL_FREE(memoryAllocation, &inferrerTypeSignature->value.expression.positionalSignatures);
	if (e) return e;

	return e;
}

dl_error_t inferrerTypeSignature_serialize(inferrerState_t *state, inferrerTypeSignature_t inferrerTypeSignature) {
	dl_error_t e = dl_error_ok;

	if (inferrerTypeSignature.type == inferrerTypeSignature_type_symbol) {
		if (inferrerTypeSignature.value.symbol == inferrerTypeSymbol_L) {
			e = inferrerState_log(state, DL_STR("L"));
		}
		else {
			e = inferrerState_log(state, DL_STR("I"));
		}
		if (e) goto cleanup;
	}
	else {
		dl_bool_t first = dl_true;
		e = inferrerState_log(state, DL_STR("("));
		if (e) goto cleanup;
		DL_DOTIMES(j, inferrerTypeSignature.value.expression.positionalSignatures_length) {
			if (first) {
				first = dl_false;
			}
			else {
				e = inferrerState_log(state, DL_STR(" "));
				if (e) goto cleanup;
			}
			inferrerTypeSignature_serialize(state, inferrerTypeSignature.value.expression.positionalSignatures[j]);
		}
		if (inferrerTypeSignature.value.expression.variadic) {
			if (inferrerTypeSignature.value.expression.positionalSignatures_length > 0) {
				e = inferrerState_log(state, DL_STR(" "));
				if (e) goto cleanup;
			}
			e = inferrerState_log(state, DL_STR("&rest "));
			if (e) goto cleanup;
			e = dl_string_fromPtrdiff(state->log, inferrerTypeSignature.value.expression.defaultRestLength);
			if (e) goto cleanup;
			e = inferrerState_log(state, DL_STR(" "));
			if (e) goto cleanup;
			inferrerTypeSignature_serialize(state, *inferrerTypeSignature.value.expression.restSignature);
		}
		if (e) goto cleanup;
		e = inferrerState_log(state, DL_STR(")"));
	}
 cleanup: return e;
}

static dl_error_t inferrerTypeSignature_fromAst(inferrerState_t *state,
                                                inferrerTypeSignature_t *inferrerTypeSignature,
                                                duckLisp_ast_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	switch (compoundExpression.type) {
	case duckLisp_ast_type_identifier:
		/* Fall through */
	case duckLisp_ast_type_callback: {
		dl_bool_t result;
		duckLisp_ast_identifier_t identifier = compoundExpression.value.identifier;
		(void) dl_string_compare(&result,
		                         DL_STR("L"),
		                         identifier.value,
		                         identifier.value_length);
		if (result) {
			inferrerTypeSignature->value.symbol = inferrerTypeSymbol_L;
		}
		else {
			(void) dl_string_compare(&result,
			                         DL_STR("I"),
			                         identifier.value,
			                         identifier.value_length);
			if (result) {
				inferrerTypeSignature->value.symbol = inferrerTypeSymbol_I;
			}
			else {
				e = dl_error_invalidValue;
				eError = duckLisp_error_pushInference(state, DL_STR("Invalid type symbol"));
				if (eError) e = eError;
				goto cleanup;
			}
		}
		inferrerTypeSignature->type = inferrerTypeSignature_type_symbol;
		break;
	}
	case duckLisp_ast_type_expression: {
		duckLisp_ast_expression_t expression = compoundExpression.value.expression;
		inferrerTypeSignature_t subInferrerTypeSignature;
		(void) inferrerTypeSignature_init(&subInferrerTypeSignature);
		dl_array_t subInferrerTypeSignaturesArray;
		(void) dl_array_init(&subInferrerTypeSignaturesArray,
		                     state->memoryAllocation,
		                     sizeof(inferrerTypeSignature_t),
		                     dl_array_strategy_double);
		dl_bool_t variadic = dl_false;
		dl_ptrdiff_t variadic_arg = 0;
		DL_DOTIMES(j, expression.compoundExpressions_length) {
			if (variadic_arg == 1) {
				variadic_arg++;
				if (expression.compoundExpressions[j].type != duckLisp_ast_type_int) {
					e = dl_error_invalidValue;
					eError = duckLisp_error_pushInference(state, DL_STR("Default argument length is not an integer"));
					if (eError) e = eError;
					goto expressionCleanup;
				}
				inferrerTypeSignature->value.expression.defaultRestLength = (expression.compoundExpressions[j]
				                                                             .value.integer.value);
				continue;
			}
			if (variadic_arg == 2) {
				variadic_arg++;
				e = inferrerTypeSignature_fromAst(state, &subInferrerTypeSignature, expression.compoundExpressions[j]);
				if (e) goto expressionCleanup;
				e = DL_MALLOC(state->memoryAllocation,
				              &inferrerTypeSignature->value.expression.restSignature,
				              1,
				              inferrerTypeSignature_t);
				if (e) goto expressionCleanup;
				*inferrerTypeSignature->value.expression.restSignature = subInferrerTypeSignature;
				continue;
			}
			if ((expression.compoundExpressions[j].type == duckLisp_ast_type_identifier)
			    || (expression.compoundExpressions[j].type == duckLisp_ast_type_callback)) {
				dl_bool_t result;
				duckLisp_ast_identifier_t identifier = expression.compoundExpressions[j].value.identifier;
				(void) dl_string_compare(&result,
				                         DL_STR("&rest"),
				                         identifier.value,
				                         identifier.value_length);
				if (result) {
					if (variadic) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state, DL_STR("Duplicate \"&rest\" in type specifier"));
						if (eError) e = eError;
						goto expressionCleanup;
					}
					if ((dl_size_t) j != (expression.compoundExpressions_length - 3)) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state,
						                                      DL_STR("Exactly two forms should follow \"&rest\"."));
						if (eError) e = eError;
						goto expressionCleanup;
					}
					variadic = dl_true;
					variadic_arg++;
					continue;
				}
			}
			e = inferrerTypeSignature_fromAst(state, &subInferrerTypeSignature, expression.compoundExpressions[j]);
			if (e) goto expressionCleanup;
			e = dl_array_pushElement(&subInferrerTypeSignaturesArray, &subInferrerTypeSignature);
			if (e) goto expressionCleanup;
		}

		inferrerTypeSignature->type = inferrerTypeSignature_type_expression;
		inferrerTypeSignature->value.expression.positionalSignatures = subInferrerTypeSignaturesArray.elements;
		(inferrerTypeSignature
		 ->value.expression.positionalSignatures_length) = subInferrerTypeSignaturesArray.elements_length;
		inferrerTypeSignature->value.expression.variadic = variadic;
		expressionCleanup:
		if (e) {
			eError = dl_array_quit(&subInferrerTypeSignaturesArray);
			if (eError) e = eError;
			eError = inferrerTypeSignature_quit(&subInferrerTypeSignature, state->memoryAllocation);
			if (eError) e = eError;
		}
		break;
	}
	case duckLisp_ast_type_bool:
		/* Fall through */
	case duckLisp_ast_type_int:
		/* Fall through */
	case duckLisp_ast_type_float:
		/* Fall through */
	case duckLisp_ast_type_string:
		e = dl_error_invalidValue;
		// Log error.
		goto cleanup;
	default:
		e = dl_error_cantHappen;
		// Log error.
		goto cleanup;
	}

 cleanup:
	if (e) {
		eError = inferrerTypeSignature_quit(inferrerTypeSignature, state->memoryAllocation);
		if (eError) e = eError;
	}
	return e;
}

static void inferrerType_init(inferrerType_t *inferrerType) {
	(void) inferrerTypeSignature_init(&inferrerType->type);
	inferrerType->bytecode = dl_null;
	inferrerType->bytecode_length = 0;
}

static dl_error_t inferrerType_quit(inferrerType_t *inferrerType, dl_memoryAllocation_t *memoryAllocation) {
	dl_error_t e = inferrerTypeSignature_quit(&inferrerType->type, memoryAllocation);
	e = DL_FREE(memoryAllocation, &inferrerType->bytecode);
	inferrerType->bytecode_length = 0;
	return e;
}

static void inferrerScope_init(inferrerScope_t *inferrerScope, dl_memoryAllocation_t *ma) {
	(void) dl_trie_init(&inferrerScope->identifiersTrie, ma, -1);
	(void) dl_array_init(&inferrerScope->types, ma, sizeof(inferrerType_t), dl_array_strategy_double);
}

static dl_error_t inferrerScope_quit(inferrerScope_t *inferrerScope, dl_memoryAllocation_t *ma) {
	dl_error_t e = dl_trie_quit(&inferrerScope->identifiersTrie);
	DL_DOTIMES(i, inferrerScope->types.elements_length) {
		inferrerType_t inferrerType;
		e = dl_array_get(&inferrerScope->types, &inferrerType, i);
		if (e) break;
		e = inferrerType_quit(&inferrerType, ma);
	}
	e = dl_array_quit(&inferrerScope->types);
	return e;
}



static dl_error_t inferrerState_init(inferrerState_t *inferrerState,
                                     dl_memoryAllocation_t *memoryAllocation,
                                     dl_size_t maxComptimeVmObjects,
                                     dl_array_t *errors,
                                     dl_array_t *log,
                                     const dl_uint8_t *fileName,
                                     const dl_size_t fileName_length) {
	dl_error_t e = dl_error_ok;

	dl_array_t *inferrerContext = dl_null;
	e = DL_MALLOC(memoryAllocation, &inferrerContext, 1, vmContext_t);
	if (e) goto cleanup;
	(void) dl_array_init(inferrerContext,
	                     memoryAllocation,
	                     sizeof(vmContext_t),
	                     dl_array_strategy_double);
	inferrerState->memoryAllocation = memoryAllocation;
	e = duckLisp_init(&inferrerState->duckLisp, memoryAllocation, maxComptimeVmObjects, maxComptimeVmObjects);
	if (e) goto cleanup;
	e = duckVM_init(&inferrerState->duckVM, memoryAllocation, maxComptimeVmObjects);
	if (e) goto cleanup;
	inferrerState->duckVM.inferrerContext = inferrerContext;
	inferrerState->errors = errors;
	inferrerState->log = log;
	inferrerState->fileName = fileName;
	inferrerState->fileName_length = fileName_length;
	(void) dl_array_init(&inferrerState->scopeStack,
	                     memoryAllocation,
	                     sizeof(inferrerScope_t),
	                     dl_array_strategy_double);

 cleanup: return e;
}

static dl_error_t inferrerState_quit(inferrerState_t *inferrerState) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	{
		dl_size_t length = inferrerState->scopeStack.elements_length;
		DL_DOTIMES(i, length) {
			inferrerScope_t inferrerScope;
			e = dl_array_get(&inferrerState->scopeStack, &inferrerScope, i);
			if (e) break;
			e = inferrerScope_quit(&inferrerScope, inferrerState->memoryAllocation);
		}
	}
	eError = dl_array_quit(&inferrerState->scopeStack);
	if (eError) e = eError;
	inferrerState->fileName_length = 0;
	inferrerState->fileName = dl_null;
	inferrerState->errors = dl_null;
	inferrerState->log = dl_null;
	e = dl_array_quit((dl_array_t *) inferrerState->duckVM.inferrerContext);
	eError = DL_FREE(inferrerState->memoryAllocation, &inferrerState->duckVM.inferrerContext);
	if (eError) e = eError;
	(void) duckVM_quit(&inferrerState->duckVM);
	(void) duckLisp_quit(&inferrerState->duckLisp);
	inferrerState->memoryAllocation = dl_null;
	return e;
}


static dl_error_t pushDeclarationScope(inferrerState_t *state, inferrerScope_t *scope) {
	return dl_array_pushElement(&state->scopeStack, scope);
}

static dl_error_t popDeclarationScope(inferrerState_t *state, inferrerScope_t *scope) {
	if (scope == dl_null) {
		inferrerScope_t scope;
		dl_error_t e = dl_array_popElement(&state->scopeStack, &scope);
		if (e) goto cleanup;
		e = inferrerScope_quit(&scope, state->memoryAllocation);
		if (e) goto cleanup;
	cleanup: return e;
	}
	else {
		return dl_array_popElement(&state->scopeStack, scope);
	}
}

static dl_error_t findDeclaration(inferrerState_t *state,
                                  dl_bool_t *found,
                                  inferrerType_t *type,
                                  duckLisp_ast_identifier_t identifier) {
	dl_error_t e = dl_error_ok;

	*found = dl_false;

	dl_array_t scopeStack = state->scopeStack;
	for (dl_ptrdiff_t i = state->scopeStack.elements_length - 1; i >= 0; --i) {
		inferrerScope_t scope;
		e = dl_array_get(&scopeStack, &scope, i);
		if (e) goto cleanup;
		dl_ptrdiff_t index = -1;
		(void) dl_trie_find(scope.identifiersTrie, &index, identifier.value, identifier.value_length);
		if (index != -1) {
			e = dl_array_get(&scope.types, type, index);
			if (e) goto cleanup;
			*found = dl_true;
			break;
		}
	}

 cleanup: return e;
}

static dl_error_t addDeclaration(inferrerState_t *state,
                                 const dl_uint8_t *name,
                                 const dl_size_t name_length,
                                 duckLisp_ast_compoundExpression_t typeAst,
                                 dl_uint8_t *bytecode,
                                 dl_size_t bytecode_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	inferrerType_t type;
	inferrerScope_t scope;
	(void) inferrerType_init(&type);

	e = popDeclarationScope(state, &scope);
	if (e) goto cleanup;

	type.bytecode = bytecode;
	type.bytecode_length = bytecode_length;

	e = inferrerTypeSignature_fromAst(state, &type.type, typeAst);
	if (e) goto cleanup;
	if ((type.type.type == inferrerTypeSignature_type_symbol) && (bytecode_length > 0)) {
		e = dl_error_invalidValue;
		(eError
		 = duckLisp_error_pushInference(state,
		                                DL_STR("Adding an inference script to an identifier with a symbol type is disallowed.")));
		if (eError) e = eError;
		goto cleanup;
	}

	e = dl_trie_insert(&scope.identifiersTrie, name, name_length, scope.types.elements_length);
	if (e) goto cleanup;
	e = dl_array_pushElement(&scope.types, &type);
	if (e) goto cleanup;

	e = pushDeclarationScope(state, &scope);
	if (e) goto cleanup;

 cleanup: return e;
}

static dl_error_t compileAndAddDeclaration(inferrerState_t *state,
                                           const dl_uint8_t *fileName,
                                           const dl_size_t fileName_length,
                                           const dl_uint8_t *name,
                                           const dl_size_t name_length,
                                           const duckLisp_ast_compoundExpression_t typeAst,
                                           const duckLisp_ast_compoundExpression_t scriptAst) {
	dl_error_t e = dl_error_ok;
	(void) fileName;
	(void) fileName_length;

	dl_uint8_t *bytecode = dl_null;
	dl_size_t bytecode_length = 0;

	if (scriptAst.type != duckLisp_ast_type_none) {
		dl_array_t bytecodeArray;
		duckLisp_compileState_t compileState;

		(void) duckLisp_compileState_init(&state->duckLisp, &compileState);
		e = duckLisp_compileAST(&state->duckLisp, &compileState, &bytecodeArray, scriptAst);
		if (e) goto cleanup;
		e = duckLisp_compileState_quit(&state->duckLisp, &compileState);
		if (e) goto cleanup;

		bytecode = bytecodeArray.elements;
		bytecode_length = bytecodeArray.elements_length;
	}
	e = addDeclaration(state,
	                   name,
	                   name_length,
	                   typeAst,
	                   bytecode,
	                   bytecode_length);
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t duckLisp_error_pushInference(inferrerState_t *inferrerState,
                                               const dl_uint8_t *message,
                                               const dl_size_t message_length) {
	dl_error_t e = dl_error_ok;

	const dl_uint8_t inferrerPrefix[] = "Inference error: ";
	const dl_size_t inferrerPrefix_length = sizeof(inferrerPrefix)/sizeof(*inferrerPrefix) - 1;

	if (inferrerState->errors->elements_length > 0) {
		e = dl_array_pushElements(inferrerState->errors, DL_STR("\n"));
		if (e) goto cleanup;
	}
	e = dl_array_pushElements(inferrerState->errors, inferrerState->fileName, inferrerState->fileName_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(inferrerState->errors, DL_STR("\n"));
	if (e) goto cleanup;
	e = dl_array_pushElements(inferrerState->errors, inferrerPrefix, inferrerPrefix_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(inferrerState->errors, message, message_length);
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t inferArgument(inferrerState_t *state,
                                const dl_uint8_t *fileName,
                                const dl_size_t fileName_length,
                                duckLisp_ast_expression_t *expression,
                                dl_ptrdiff_t *index,
                                dl_bool_t parenthesized,
                                dl_bool_t infer);
static dl_error_t infer_compoundExpression(inferrerState_t *state,
                                           const dl_uint8_t *fileName,
                                           const dl_size_t fileName_length,
                                           duckLisp_ast_compoundExpression_t *compoundExpression,
                                           dl_bool_t infer);
static dl_error_t inferArguments(inferrerState_t *state,
                                 const dl_uint8_t *fileName,
                                 const dl_size_t fileName_length,
                                 duckLisp_ast_expression_t *expression,
                                 dl_ptrdiff_t index,
                                 dl_bool_t infer);

static dl_error_t infer_callback(inferrerState_t *state,
                                 duckLisp_ast_compoundExpression_t *compoundExpression,
                                 dl_bool_t infer) {
	(void) state;
	(void) infer;
	compoundExpression->type = duckLisp_ast_type_identifier;
	return dl_error_ok;
}


			/* expr index
			   type index
			*/

static dl_error_t runVm(inferrerState_t *state,
                        inferrerType_t type,
                        dl_ptrdiff_t *type_index,
                        duckLisp_ast_expression_t *expression,
                        dl_ptrdiff_t *expression_index,
                        dl_array_t *newExpression,  /* dl_array_t:duckLisp_ast_compoundExpression_t */
                        dl_size_t *newLength,
                        dl_bool_t parenthesized,
                        dl_bool_t infer) {
	dl_error_t e = dl_error_ok;

	vmContext_t context;
	context.state = state;
	context.type = type;
	context.type_index = type_index;
	context.expression = expression;
	context.expression_index = expression_index;
	context.newExpression = newExpression;
	context.newLength = newLength;
	context.parenthesized = parenthesized;
	context.infer = infer;

	e = dl_array_pushElement((dl_array_t *) state->duckVM.inferrerContext, &context);
	if (e) goto cleanup;
	/* puts(duckLisp_disassemble(state->memoryAllocation, type.bytecode, type.bytecode_length)); */
	e = duckVM_execute(&state->duckVM, dl_null, type.bytecode, type.bytecode_length);
	if (e) goto cleanup;
	e = dl_array_popElement((dl_array_t *) state->duckVM.inferrerContext, dl_null);
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t inferIncrementally(inferrerState_t *state,
                                     const dl_uint8_t *fileName,
                                     const dl_size_t fileName_length,
                                     inferrerType_t type,
                                     dl_ptrdiff_t *type_index,
                                     duckLisp_ast_expression_t *expression,
                                     dl_ptrdiff_t *expression_index,
                                     dl_array_t *newExpression,  /* dl_array_t:duckLisp_ast_compoundExpression_t */
                                     dl_size_t *newExpression_length,
                                     dl_bool_t parenthesized,
                                     dl_bool_t infer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	if ((dl_size_t) *type_index <= type.type.value.expression.positionalSignatures_length) {
		/* A coroutine could be nice here, but this works too. */
		if ((dl_size_t) *type_index < type.type.value.expression.positionalSignatures_length) {
			if ((dl_size_t) *expression_index >= expression->compoundExpressions_length) {
				e = dl_error_invalidValue;
				(eError
				 = duckLisp_error_pushInference(state,
				                                DL_STR("Too few arguments for declared identifier.")));
				if (eError) e = eError;
				goto cleanup;
			}
			inferrerTypeSignature_t argSignature = (type.type.value.expression.positionalSignatures
			                                        [*type_index]);
			if (argSignature.type == inferrerTypeSignature_type_symbol) {
				dl_ptrdiff_t lastIndex = *expression_index;
				e = inferArgument(state,
				                  fileName,
				                  fileName_length,
				                  expression,
				                  expression_index,
				                  dl_false,
				                  (argSignature.value.symbol == inferrerTypeSymbol_I) ? infer : dl_false);
				if (e) goto cleanup;
				if (!parenthesized) {
					e = dl_array_pushElement(newExpression, &expression->compoundExpressions[lastIndex]);
					if (e) goto cleanup;
					(*newExpression_length)++;
				}
			}
			else {
				e = dl_error_invalidValue;
				(eError
				 = duckLisp_error_pushInference(state,
				                                DL_STR("Nested expression types are not yet supported.")));
				if (eError) e = eError;
			}
		}
		else {
			// This does not need to be incremental, but it does need to be treated as the parg, and pargs
			// are inferred incrementally.
			if (type.type.value.expression.variadic) {
				if (!parenthesized && (0 > type.type.value.expression.defaultRestLength)) {
					e = dl_error_invalidValue;
					(eError
					 = duckLisp_error_pushInference(state,
					                                DL_STR("This variadic function may not be called without parentheses.")));
					if (eError) e = eError;
					goto cleanup;
				}
				inferrerTypeSignature_t argSignature = *type.type.value.expression.restSignature;
				for (dl_ptrdiff_t l = 0;
				     (parenthesized
				      ? ((dl_size_t) *expression_index < expression->compoundExpressions_length)
				      : (l < type.type.value.expression.defaultRestLength));
				     l++) {
					if (argSignature.type == inferrerTypeSignature_type_symbol) {
						dl_ptrdiff_t lastIndex = *expression_index;
						e = inferArgument(state,
						                  fileName,
						                  fileName_length,
						                  expression,
						                  expression_index,
						                  dl_false,
						                  ((argSignature.value.symbol == inferrerTypeSymbol_I)
						                   ? infer
						                   : dl_false));
						if (e) goto cleanup;
						if (!parenthesized) {
							e = dl_array_pushElement(newExpression,
							                         &expression->compoundExpressions[lastIndex]);
							if (e) goto cleanup;
							(*newExpression_length)++;
						}
					}
					else {
						e = dl_error_invalidValue;
						(eError
						 = duckLisp_error_pushInference(state,
						                                DL_STR("Nested expression types are not yet supported.")));
						if (eError) e = eError;
					}
				}
			}
		}
		(*type_index)++;
	}

 cleanup: return e;
}

/* Check if this is a declaration and declare the given identifier if it is. */
dl_error_t interpretDeclare(inferrerState_t *state,
                            const dl_uint8_t *fileName,
                            const dl_size_t fileName_length,
                            duckLisp_ast_compoundExpression_t compoundExpression) {
	dl_error_t e = dl_error_ok;

	/* `declare` interpreter */
	if ((compoundExpression.type == duckLisp_ast_type_expression)
	    && ((compoundExpression.value.expression.compoundExpressions_length == 3)
	        || (compoundExpression.value.expression.compoundExpressions_length == 4))
	    && (compoundExpression.value.expression.compoundExpressions[0].type == duckLisp_ast_type_identifier)
	    && (compoundExpression.value.expression.compoundExpressions[1].type == duckLisp_ast_type_identifier)) {
		dl_bool_t result;
		duckLisp_ast_expression_t expression = compoundExpression.value.expression;
		duckLisp_ast_identifier_t keyword = expression.compoundExpressions[0].value.identifier;
		duckLisp_ast_identifier_t identifier = expression.compoundExpressions[1].value.identifier;
		duckLisp_ast_compoundExpression_t typeAst = expression.compoundExpressions[2];
		(void) dl_string_compare(&result,
		                         DL_STR("__declare"),
		                         keyword.value,
		                         keyword.value_length);
		if (!result) {
			(void) dl_string_compare(&result,
			                         DL_STR("declare"),
			                         keyword.value,
			                         keyword.value_length);
		}
		if (result) {
			duckLisp_ast_compoundExpression_t scriptAst;
			(void) duckLisp_ast_compoundExpression_init(&scriptAst);
			if (compoundExpression.value.expression.compoundExpressions_length == 4) {
				scriptAst = compoundExpression.value.expression.compoundExpressions[3];
			}
			e = compileAndAddDeclaration(state,
			                             fileName,
			                             fileName_length,
			                             identifier.value,
			                             identifier.value_length,
			                             typeAst,
			                             scriptAst);
			if (e) goto cleanup;
		}
	}

 cleanup: return e;
}

/* Infer a single argument from the tokens/forms. */
static dl_error_t inferArgument(inferrerState_t *state,
                                const dl_uint8_t *fileName,
                                const dl_size_t fileName_length,
                                duckLisp_ast_expression_t *expression,
                                dl_ptrdiff_t *index,
                                dl_bool_t parenthesized,
                                dl_bool_t infer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	dl_ptrdiff_t localIndex = *index;
	dl_ptrdiff_t startLocalIndex = localIndex;
	dl_size_t newLength = 0;
	if ((dl_size_t) localIndex >= expression->compoundExpressions_length) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushInference(state, DL_STR("To few arguments for declared identifier."));
		if (eError) e = eError;
		goto cleanup;
	}
	duckLisp_ast_compoundExpression_t *compoundExpression = expression->compoundExpressions + localIndex;
	localIndex++;
	if (compoundExpression->type == duckLisp_ast_type_identifier) {
		inferrerType_t type;
		dl_bool_t found = dl_false;
		e = findDeclaration(state, &found, &type, compoundExpression->value.identifier);
		if (e) goto cleanup;

		if (!infer) {
			e = inferrerState_log(state, DL_STR("\x1B[34m"));
		}
		else if (found && infer) {
			e = inferrerState_log(state, DL_STR("\x1B[32m"));
		}
		else {
			e = inferrerState_log(state, DL_STR("\x1B[31m"));
		}
		if (e) goto cleanup;
		e = inferrerState_log(state,
		                      compoundExpression->value.identifier.value,
		                      compoundExpression->value.identifier.value_length);
		if (e) goto cleanup;

		if (found && infer) {
			/* Declared */
			if (type.bytecode_length > 0) {
				/* Execute bytecode */
			}

			e = inferrerState_log(state, DL_STR("::"));
			if (e) goto cleanup;
			e = inferrerTypeSignature_serialize(state, type.type);
			if (e) goto cleanup;
			e = inferrerState_log(state, DL_STR("\x1B[0m\n"));
			if (e) goto cleanup;

			/* lol */
			if (type.type.type == inferrerTypeSignature_type_expression) {
				dl_array_t newExpression;  /* dl_array_t:duckLisp_ast_compoundExpression_t */
				(void) dl_array_init(&newExpression,
				                     state->memoryAllocation,
				                     sizeof(duckLisp_ast_compoundExpression_t),
				                     dl_array_strategy_double);
				if (!parenthesized) {
					e = dl_array_pushElement(&newExpression, compoundExpression);
					if (e) goto expressionCleanup;
				}

				dl_ptrdiff_t type_index = 0;

				/* This part may do some inference. */
				if (type.bytecode_length > 0) {
					e = runVm(state,
					          type,
					          &type_index,
					          expression,
					          &localIndex,
					          &newExpression,
					          &newLength,
					          parenthesized,
					          infer);
					if (e) goto cleanup;
				}

				/* This part does whatever inference the script didn't do. */
				while ((dl_size_t) type_index <= type.type.value.expression.positionalSignatures_length) {
					e = inferIncrementally(state,
					                       fileName,
					                       fileName_length,
					                       type,
					                       &type_index,
					                       expression,
					                       &localIndex,
					                       &newExpression,
					                       &newLength,
					                       parenthesized,
					                       infer);
					if (e) goto expressionCleanup;
				}

			expressionCleanup:
				if (e) {
					eError = dl_array_quit(&newExpression);
					if (eError) e = eError;
				}
				else if (parenthesized) {
					if ((dl_size_t) localIndex > expression->compoundExpressions_length) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state, DL_STR("Too few arguments for identifier."));
						if (eError) e = eError;
					}
					else if ((dl_size_t) localIndex < expression->compoundExpressions_length) {
						e = dl_error_invalidValue;
						eError = duckLisp_error_pushInference(state, DL_STR("Too many arguments for identifier."));
						if (eError) e = eError;
					}
				}
				else {
					duckLisp_ast_compoundExpression_t ce;
					ce.type = duckLisp_ast_type_expression;
					ce.value.expression.compoundExpressions_length = newExpression.elements_length;
					ce.value.expression.compoundExpressions = newExpression.elements;
					expression->compoundExpressions[startLocalIndex] = ce;
					DL_DOTIMES(n, expression->compoundExpressions_length - startLocalIndex - newLength - 1) {
						(expression->compoundExpressions[startLocalIndex + n + 1]
						 = expression->compoundExpressions[startLocalIndex + newLength + n + 1]);
					}
					expression->compoundExpressions_length -= newLength;
					localIndex = startLocalIndex + 1;
				}
				if (e) goto cleanup;

				e = interpretDeclare(state,
				                     fileName,
				                     fileName_length,
				                     expression->compoundExpressions[startLocalIndex]);
				if (e) goto cleanup;
			}
			else if (parenthesized) {
				if (type.type.value.symbol == inferrerTypeSymbol_L) {
					// This could probably be moved to `infer_expression`.
					e = inferArguments(state, fileName, fileName_length, expression, 1, infer);
					if (e) goto cleanup;
				}
				else {
					e = dl_error_invalidValue;
					eError = duckLisp_error_pushInference(state, DL_STR("Cannot call an identifier of type \"I\"."));
					if (eError) e = eError;
					goto cleanup;
				}
			}
			else {
				/* Do nothing */
			}
		}
		else {
			if (found) {
				/* Inference disabled */
				e = inferrerState_log(state, DL_STR("::"));
				if (e) goto cleanup;
				inferrerTypeSignature_serialize(state, type.type);
				e = inferrerState_log(state, DL_STR("\x1B[0m\n"));
				if (e) goto cleanup;
				e = infer_compoundExpression(state, fileName, fileName_length, compoundExpression, infer);
				if (e) goto cleanup;
			}
			else {
				/* Undeclared */
				e = inferrerState_log(state, DL_STR("::Undeclared\x1B[0m\n"));
				if (e) goto cleanup;
				e = duckLisp_error_pushInference(state, DL_STR("Undeclared identifier. See inference log.\n"));
				if (e) goto cleanup;
				e = infer_compoundExpression(state, fileName, fileName_length, compoundExpression, infer);
				if (e) goto cleanup;
				if (infer) {
					e = dl_error_invalidValue;
					goto cleanup;
				}
			}
		}
	}
	else {
		/* Anything not an identifier */
		e = infer_compoundExpression(state, fileName, fileName_length, compoundExpression, infer);
		if (e) goto cleanup;
		e = interpretDeclare(state, fileName, fileName_length, *compoundExpression);
		if (e) goto cleanup;
	}
	*index = localIndex;

 cleanup: return e;
}

/* Infer a single argument from the tokens/forms. */
static dl_error_t inferArguments(inferrerState_t *state,
                                 const dl_uint8_t *fileName,
                                 const dl_size_t fileName_length,
                                 duckLisp_ast_expression_t *expression,
                                 dl_ptrdiff_t index,
                                 dl_bool_t infer) {
	dl_error_t e = dl_error_ok;

	while ((dl_size_t) index < expression->compoundExpressions_length) {
		/* Run inference on the current identifier. */
		dl_ptrdiff_t startIndex = index;
		/* The function needs the whole expression. It is given the pointer to the current index. */
		e = inferArgument(state, fileName, fileName_length, expression, &index, dl_false, infer);
		if (e) goto cleanup;
		e = interpretDeclare(state, fileName, fileName_length, expression->compoundExpressions[startIndex]);
		if (e) goto cleanup;
	}

 cleanup: return e;
}

static dl_error_t infer_expression(inferrerState_t *state,
                                   const dl_uint8_t *fileName,
                                   const dl_size_t fileName_length,
                                   duckLisp_ast_compoundExpression_t *compoundExpression,
                                   dl_bool_t infer) {
	dl_error_t e = dl_error_ok;

	if (!infer) {
		/* OK */
		goto cleanup;
	}

	duckLisp_ast_expression_t *expression = &compoundExpression->value.expression;

	/* Expression inference:
	     Is an identifier? — Done
	       Is declared? — Done
	         Has declarator? — Done
	           Run declarator.

	   TODO: declarator scripts
	         nested types

	   Two functions can only be used if the right data structure exists. Or maybe I can copy the tree instead of
	   modifying it?

	   declare m (L I)
	   m a 1  —  Infer, infer second arg
	   (m a 1)  —  Check arity, infer second arg
	   (#m a 1)  —  Untyped, infer both args
	   #(m a 1)  —  Untyped, don't infer any args
	*/

	if (expression->compoundExpressions_length == 0) {
		/* OK. Is a Nil. */
		goto cleanup;
	}

	/* This pointer is weak. */
	duckLisp_ast_compoundExpression_t *head = expression->compoundExpressions;

	if (compoundExpression->type == duckLisp_ast_type_literalExpression) {
		/* Don't type-check. */
		e = infer_callback(state, head, dl_false);
		if (e) goto cleanup;
		/* Run argument inference */

		e = inferArguments(state, fileName, fileName_length, expression, 1, dl_false);
		if (e) goto cleanup;

		compoundExpression->type = duckLisp_ast_type_expression;
	}
	else if (head->type == duckLisp_ast_type_callback) {
		/* Don't type-check. */
		e = infer_callback(state, head, dl_false);
		if (e) goto cleanup;
		/* Run argument inference */

		e = inferArguments(state, fileName, fileName_length, expression, 1, infer);
		if (e) goto cleanup;
	}
	else if (head->type == duckLisp_ast_type_identifier) {
		/* This is a function call. It needs to be type checked. */
		duckLisp_ast_identifier_t expressionIdentifier = head->value.identifier;
		inferrerType_t type;

		dl_bool_t found = dl_false;
		e = findDeclaration(state, &found, &type, expressionIdentifier);
		if (e) goto cleanup;

		if (found) {
			/* Declared. */
			dl_ptrdiff_t expression_index = 0;
			e = inferArgument(state, fileName, fileName_length, expression, &expression_index, dl_true, infer);
			if (e) goto cleanup;
		}
		else {
			/* Undeclared. */
			e = inferrerState_log(state, DL_STR("\x1B[31m"));
			if (e) goto cleanup;
			e = inferrerState_log(state, expressionIdentifier.value, expressionIdentifier.value_length);
			if (e) goto cleanup;
			e = inferrerState_log(state, DL_STR("::Undeclared\x1B[0m\n"));
			if (e) goto cleanup;
			e = duckLisp_error_pushInference(state, DL_STR("Undeclared identifier. See inference log.\n"));
			if (e) goto cleanup;
			/* Argument inference stuff goes here. */
			e = inferArguments(state, fileName, fileName_length, expression, 1, infer);
			if (e) goto cleanup;
			e = dl_error_invalidValue;
			goto cleanup;
		}
	}
	else {
		/* This is a scope. */
		inferrerScope_t scope;
		(void) inferrerScope_init(&scope, state->memoryAllocation);
		e = pushDeclarationScope(state, &scope);
		if (e) goto cleanup;

		e = inferArguments(state, fileName, fileName_length, expression, 0, infer);
		if (e) goto cleanup;

		e = popDeclarationScope(state, dl_null);
		if (e) goto cleanup;
	}

 cleanup: return e;
}

static dl_error_t infer_compoundExpression(inferrerState_t *state,
                                           const dl_uint8_t *fileName,
                                           const dl_size_t fileName_length,
                                           duckLisp_ast_compoundExpression_t *compoundExpression,
                                           dl_bool_t infer) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	/* Due to how the parser works, which is to only read one form, we can only infer for forms inside an expression. If
	   the outermost type is not an expression, then we can't infer anything. Because of this, all inference happens
	   inside the inference function for expressions. */

	switch (compoundExpression->type) {
	case duckLisp_ast_type_none:
		eError = duckLisp_error_pushInference(state, DL_STR("Node type is \"None\"."));
		e = eError ? eError : dl_error_cantHappen;
		break;
	case duckLisp_ast_type_bool:
		/* Fall through */
	case duckLisp_ast_type_int:
		/* Fall through */
	case duckLisp_ast_type_float:
		/* Fall through */
	case duckLisp_ast_type_string:
		/* Fall through */
	case duckLisp_ast_type_identifier:
		break;
	case duckLisp_ast_type_callback:
		e = infer_callback(state, compoundExpression, infer);
		break;
	case duckLisp_ast_type_literalExpression:
		/* Fall through */
	case duckLisp_ast_type_expression:
		e = infer_expression(state, fileName, fileName_length, compoundExpression, infer);
		break;
	default:
		eError = duckLisp_error_pushInference(state, DL_STR("Illegal type."));
		e = eError ? eError : dl_error_cantHappen;
	}
	if (e) goto cleanup;

 cleanup: return e;
}


static dl_error_t callback_declareIdentifier(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	/* stack: identifier type */
	duckVM_object_type_t identifierObject_type;
	duckVM_object_t typeObject;
	vmContext_t context;
	inferrerState_t *state = dl_null;

	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto cleanup;
	state = context.state;

	e = duckVM_object_pop(vm, &typeObject);
	if (e) goto cleanup;
	/* stack: identifier */
	if ((typeObject.type != duckVM_object_type_symbol)
	    && (typeObject.type != duckVM_object_type_list)) {
		e = dl_error_invalidValue;
		(eError
		 = duckVM_error_pushRuntime(vm,
		                            DL_STR("Second argument of `declare-identifier` should be a type signature.")));
		if (eError) e = eError;
		goto cleanup;
	}

	e = duckVM_typeOf(vm, &identifierObject_type);
	if (e) goto cleanup;
	if ((identifierObject_type != duckVM_object_type_symbol)
	    && (identifierObject_type != duckVM_object_type_string)) {
		e = dl_error_invalidValue;
		(eError
		 = duckVM_error_pushRuntime(vm,
		                            DL_STR("First argument of `declare-identifier` should be an identifier or a string.")));
		if (eError) e = eError;
		goto cleanup;
	}

	duckLisp_ast_compoundExpression_t typeAst;
	e = duckLisp_objectToAST(&state->duckLisp, &typeAst, &typeObject, dl_true);
	if (e) goto cleanup;

	{
		/* stack: identifier */
		dl_uint8_t *string = dl_null;
		dl_size_t length = 0;
		if (identifierObject_type == duckVM_object_type_symbol) {
			e = duckVM_copySymbolName(vm, &string, &length);
			if (e) goto cleanup;
		}
		else {
			e = duckVM_copyString(vm, &string, &length);
			if (e) goto cleanup;
		}
		/* stack: identifier */
		e = duckVM_pop(vm);
		if (e) goto cleanup;
		/* stack: _ */
		e = addDeclaration(state, string, length, typeAst, dl_null, 0);
		if (e) goto cleanup;
		e = DL_FREE(vm->memoryAllocation, &string);
		if (e) goto cleanup;
	}
	/* stack: _ */

	e = duckLisp_ast_compoundExpression_quit(state->memoryAllocation, &typeAst);
	if (e) goto cleanup;

	e = duckVM_pushNil(vm);
	if (e) goto cleanup;
	/* stack: () */

 cleanup: return e;
}

static dl_error_t callback_inferAndGetNextArgument(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;

	vmContext_t context;
	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto preDefinitionCleanup;

	inferrerState_t *state = context.state;
	const dl_uint8_t *fileName = context.fileName;
	const dl_size_t fileName_length = context.fileName_length;
	inferrerType_t type = context.type;
	dl_ptrdiff_t *type_index = context.type_index;
	duckLisp_ast_expression_t *expression = context.expression;
	dl_ptrdiff_t *expression_index = context.expression_index;
	dl_array_t *newExpression = context.newExpression;  /* dl_array_t:duckLisp_ast_compoundExpression_t */
	dl_size_t *newLength = context.newLength;
	const dl_bool_t parenthesized = context.parenthesized;
	const dl_bool_t infer = context.infer;

	duckLisp_t *duckLisp = &state->duckLisp;

	/* infer */

	e = inferIncrementally(state,
	                       fileName,
	                       fileName_length,
	                       type,
	                       type_index,
	                       expression,
	                       expression_index,
	                       newExpression,
	                       newLength,
	                       parenthesized,
	                       infer);
	if (e) goto postDefinitionCleanup;

	/* AndGetNextArgument */

	duckVM_object_t object;
	e = duckLisp_astToObject(duckLisp, vm, &object, expression->compoundExpressions[*expression_index - 1]);
	if (e) goto postDefinitionCleanup;

	e = duckVM_object_push(vm, &object);
	if (e) goto postDefinitionCleanup;

 postDefinitionCleanup:
 preDefinitionCleanup: return e;
}

static dl_error_t callback_pushScope(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;

	inferrerScope_t scope;
	vmContext_t context;

	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto cleanup;

	(void) inferrerScope_init(&scope, context.state->memoryAllocation);
	e = pushDeclarationScope(context.state, &scope);
	if (e) goto cleanup;

	e = duckVM_pushNil(vm);
	if (e) goto cleanup;

 cleanup: return e;
}

static dl_error_t callback_popScope(duckVM_t *vm) {
	dl_error_t e = dl_error_ok;

	vmContext_t context;

	e = dl_array_getTop(vm->inferrerContext, &context);
	if (e) goto cleanup;

	e = popDeclarationScope(context.state, dl_null);
	if (e) goto cleanup;

	e = duckVM_pushNil(vm);
	if (e) goto cleanup;

 cleanup: return e;
}

static dl_error_t generator_declarationScope(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_ast_expression_t *expression) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	/* `(declaration-scope ,@body) */
	/* `(
	     (push-declaration-scope)
	     ,@body
	     (pop-declaration-scope)) */

	duckLisp_ast_compoundExpression_t pushC;
	pushC.type = duckLisp_ast_type_identifier;
	pushC.value.identifier.value = (dl_uint8_t *) "\0__push-declaration-scope";
	pushC.value.identifier.value_length = sizeof("\0__push-declaration-scope")/sizeof(dl_uint8_t) - 1;
	duckLisp_ast_expression_t pushE;
	pushE.compoundExpressions_length = 1;
	pushE.compoundExpressions = &pushC;
	duckLisp_ast_compoundExpression_t popC;
	popC.type = duckLisp_ast_type_identifier;
	popC.value.identifier.value = (dl_uint8_t *) "\0__pop-declaration-scope";
	popC.value.identifier.value_length = sizeof("\0__pop-declaration-scope")/sizeof(dl_uint8_t) - 1;
	duckLisp_ast_expression_t popE;
	popE.compoundExpressions_length = 1;
	popE.compoundExpressions = &popC;
	duckLisp_ast_expression_t scopeE;
	scopeE.compoundExpressions_length = 0;
	e = DL_MALLOC(duckLisp->memoryAllocation,
	              &scopeE.compoundExpressions,
	              expression->compoundExpressions_length + 1,
	              duckLisp_ast_compoundExpression_t);
	if (e) goto cleanup;
	scopeE.compoundExpressions_length = expression->compoundExpressions_length + 1;

	scopeE.compoundExpressions[0].type = duckLisp_ast_type_expression;
	scopeE.compoundExpressions[0].value.expression = pushE;
	DL_DOTIMES(i, expression->compoundExpressions_length - 1) {
		scopeE.compoundExpressions[i + 1] = expression->compoundExpressions[i + 1];
	}
	scopeE.compoundExpressions[scopeE.compoundExpressions_length - 1].type = duckLisp_ast_type_expression;
	scopeE.compoundExpressions[scopeE.compoundExpressions_length - 1].value.expression = popE;

	e = duckLisp_generator_expression(duckLisp,
	                                  compileState,
	                                  assembly,
	                                  &scopeE);
	if (e) goto cleanup;

 cleanup:
	if (scopeE.compoundExpressions_length > 0) {
		eError = DL_FREE(duckLisp->memoryAllocation, &scopeE.compoundExpressions);
		if (eError) e = eError;
	}

	return e;
}


dl_error_t duckLisp_inferParentheses(dl_memoryAllocation_t *memoryAllocation,
                                     const dl_size_t maxComptimeVmObjects,
                                     dl_array_t *errors,
                                     dl_array_t *log,
                                     const dl_uint8_t *fileName,
                                     const dl_size_t fileName_length,
                                     duckLisp_ast_compoundExpression_t *ast,
                                     dl_array_t *externalDeclarations  /* dl_array_t:duckLisp_parenthesisInferrer_declarationPrototype_t */) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	inferrerState_t state;
	e = inferrerState_init(&state, memoryAllocation, maxComptimeVmObjects, errors, log, fileName, fileName_length);
	if (e) return e;

	struct {
		dl_uint8_t *name;
		const dl_size_t name_length;
		dl_error_t (*callback)(duckVM_t *);
	} callbacks[] = {
		{DL_STR("__declare-identifier"),          callback_declareIdentifier},
		{DL_STR("declare-identifier"),            callback_declareIdentifier},
		{DL_STR("__infer-and-get-next-argument"), callback_inferAndGetNextArgument},
		{DL_STR("infer-and-get-next-argument"),   callback_inferAndGetNextArgument},
		{DL_STR("\0__push-declaration-scope"),    callback_pushScope},
		{DL_STR("\0__pop-declaration-scope"),     callback_popScope},
	};
	DL_DOTIMES(i, sizeof(callbacks)/sizeof(*callbacks)) {
		e = duckLisp_linkCFunction(&state.duckLisp,
		                           callbacks[i].callback,
		                           callbacks[i].name,
		                           callbacks[i].name_length,
		                           dl_null,
		                           0);
		if (e) goto cleanup;
		dl_ptrdiff_t id = duckLisp_symbol_nameToValue(&state.duckLisp, callbacks[i].name, callbacks[i].name_length);
		e = duckVM_linkCFunction(&state.duckVM, id, callbacks[i].callback);
		if (e) goto cleanup;
	}

	e = duckLisp_addGenerator(&state.duckLisp,
	                          generator_declarationScope,
	                          DL_STR("__declaration-scope"),
	                          dl_null,
	                          0,
	                          dl_null,
	                          0);
	if (e) goto cleanup;
	e = duckLisp_addGenerator(&state.duckLisp,
	                          generator_declarationScope,
	                          DL_STR("declaration-scope"),
	                          dl_null,
	                          0,
	                          dl_null,
	                          0);
	if (e) goto cleanup;

	{
		inferrerScope_t scope;
		(void) inferrerScope_init(&scope, memoryAllocation);
		e = pushDeclarationScope(&state, &scope);
		if (e) goto cleanup;
	}

	{
		duckLisp_parenthesisInferrer_declarationPrototype_t declarations[] = {
			{DL_STR("__declare"),                     DL_STR("(L L &rest 0 I)"), dl_null, 0},
			{DL_STR("declare"),                       DL_STR("(L L &rest 0 I)"), dl_null, 0},
			{DL_STR("__infer-and-get-next-argument"), DL_STR("()"),              dl_null, 0},
			{DL_STR("infer-and-get-next-argument"),   DL_STR("()"),              dl_null, 0},
			{DL_STR("__declare-identifier"),          DL_STR("(I I)"),           dl_null, 0},
			{DL_STR("declare-identifier"),            DL_STR("(I I)"),           dl_null, 0},
			{DL_STR("__declaration-scope"),           DL_STR("(&rest 1 I)"),     dl_null, 0},
			{DL_STR("declaration-scope"),             DL_STR("(&rest 1 I)"),     dl_null, 0}};
		DL_DOTIMES(i, sizeof(declarations)/sizeof(*declarations)) {
			duckLisp_ast_compoundExpression_t typeAst;
			duckLisp_ast_compoundExpression_t scriptAst;
			(void) duckLisp_ast_compoundExpression_init(&typeAst);
			(void) duckLisp_ast_compoundExpression_init(&scriptAst);

			e = duckLisp_read(&state.duckLisp,
			                  dl_false,
			                  0,
			                  dl_null,
			                  DL_STR("<INFERRER>"),
			                  declarations[i].type,
			                  declarations[i].type_length,
			                  &typeAst,
			                  0,
			                  dl_true);
			if (e) goto cleanup;

			if (declarations[i].script_length > 0) {
				e = duckLisp_read(&state.duckLisp,
				                  dl_false,
				                  0,
				                  dl_null,
				                  DL_STR("<INFERRER>"),
				                  declarations[i].script,
				                  declarations[i].script_length,
				                  &scriptAst,
				                  0,
				                  dl_true);
				if (e) goto cleanup;
			}

			e = compileAndAddDeclaration(&state,
			                             fileName,
			                             fileName_length,
			                             declarations[i].name,
			                             declarations[i].name_length,
			                             typeAst,
			                             scriptAst);
			if (e) goto cleanup;

			e = duckLisp_ast_compoundExpression_quit(memoryAllocation, &typeAst);
			if (e) goto cleanup;
			if (scriptAst.type != duckLisp_ast_type_none) {
				e = duckLisp_ast_compoundExpression_quit(memoryAllocation, &scriptAst);
				if (e) goto cleanup;
			}
		}
		if (externalDeclarations) {
			DL_DOTIMES(i, externalDeclarations->elements_length) {
				duckLisp_parenthesisInferrer_declarationPrototype_t prototype;
				duckLisp_ast_compoundExpression_t typeAst;
				duckLisp_ast_compoundExpression_t scriptAst;
				(void) duckLisp_ast_compoundExpression_init(&typeAst);
				(void) duckLisp_ast_compoundExpression_init(&scriptAst);

				e = dl_array_get(externalDeclarations, &prototype, i);
				if (e) goto cleanup;

				e = duckLisp_read(&state.duckLisp,
				                  dl_false,
				                  0,
				                  dl_null,
				                  DL_STR("<INFERRER>"),
				                  prototype.type,
				                  prototype.type_length,
				                  &typeAst,
				                  0,
				                  dl_true);
				if (e) goto cleanup;

				if (prototype.script_length > 0) {
					e = duckLisp_read(&state.duckLisp,
					                  dl_false,
					                  0,
					                  dl_null,
					                  DL_STR("<INFERRER>"),
					                  prototype.script,
					                  prototype.script_length,
					                  &scriptAst,
					                  0,
					                  dl_true);
					if (e) goto cleanup;
				}

				e = compileAndAddDeclaration(&state,
				                             fileName,
				                             fileName_length,
				                             prototype.name,
				                             prototype.name_length,
				                             typeAst,
				                             scriptAst);
				if (e) goto cleanup;

				e = duckLisp_ast_compoundExpression_quit(memoryAllocation, &typeAst);
				if (e) goto cleanup;
				if (scriptAst.type != duckLisp_ast_type_none) {
					e = duckLisp_ast_compoundExpression_quit(memoryAllocation, &scriptAst);
					if (e) goto cleanup;
				}
			}
		}
	}


	e = infer_compoundExpression(&state, fileName, fileName_length, ast, dl_true);
	if (e) goto cleanup;

 cleanup:
	eError = dl_array_pushElements(errors, state.duckLisp.errors.elements, state.duckLisp.errors.elements_length);
	if (eError) e = eError;
	state.duckLisp.errors.elements = dl_null;
	state.duckLisp.errors.elements_length = 0;
	eError = dl_array_pushElements(errors, state.duckVM.errors.elements, state.duckVM.errors.elements_length);
	if (eError) e = eError;
	state.duckVM.errors.elements = dl_null;
	state.duckVM.errors.elements_length = 0;

	eError = inferrerState_quit(&state);
	if (eError) e = eError;

	return e;
}

#endif /* USE_PARENTHESIS_INFERENCE */
