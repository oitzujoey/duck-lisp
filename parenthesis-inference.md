# Parenthesis inference

Duck-lisp has an optional parenthesis inferrer. This is yet another method of removing the parentheses from lisp, but unlike most of the others I have seen, this one is based on Forth and ignores indentation and newlines.

### Advantages

* Noobs can't complain about parentheses when there are none!
* Macros can declare variables using declaration scripts.
* Inferred duck-lisp is almost as easy to type on the command line as Forth.
* Indentation and newlines have no affect on how code is compiled.

### Disadvantages

* Noobs will now be horrified by the lack of parentheses and any sort of inherent visual structure. To be perfectly
  honest, I'm horrified too.
* No editor support. While the syntax is specifically designed to work nicely with sufficiently intelligent editors,
  support for this syntax must be written from scratch.
* Declaration scripts are Turing complete, though this is specific to this implementation and not to all Forth-like
  inference systems. It may be possible to substitute a regular or context-free language that can be proven to halt or
  else throw an inference error. I have not attempted this as the duck-lisp inferrer is a proof of concept.
* Not all macros can be perfectly described with declaration scripts.

## Examples

```lisp
;; All functions used here have been previously defined and declared.
(defun println (&rest 1 args)
  dolist (arg args)
    print arg
  print "\n"
  args)

println "Hello, world!"  ; ⇒ Hello, world!
(println 1 2 3)  ; ⇒ (1 2 3)
```

```lisp
;; Fresh environment
__defun + (a &rest b)
        __if b
             __+ a (__apply #self b)  ; Optional parentheses for clarity
             a
__declare + (I &rest 1 I)

print + + 1 2
        + 3 4  ; ⇒ 10
print "\n"
print (+ 1 2 3 4)  ; ⇒ 10
print "\n"
```

```lisp
;; Command line example. All functions used here have been previously defined and declared.
reduce #+ map lambda (x) * 2 x (list 1 2 3 4)  ; ⇒ 20
```

```tcl
## Same example in Tcl
expr [join [lmap x {1 2 3 4} {expr 2*$x}] "+"]  ;# ⇒ 20
## Alternatively…
::tcl::mathop::+ {*}[lmap x {1 2 3 4} {expr 2*$x}]  ;# ⇒ 20
```

## Model

### Static types

Inference works by counting arguments. The inference stage is run between the parsing stage and the compilation stages.

Types are declared with the keyword `__declare`. It accepts two arguments by default. Inference is disabled for those arguments.

```lisp
__declare __* (I I)
__declare __if (I I I)
__declare __setq (L I)
```

Say this declaration exists:

```lisp
__declare f (I I)
```

`(I I)` means to infer two arguments when `f` is encountered. In the code below, the identifier `f` is followed by three integers.

```lisp
f 1 2 3
```

Since the type says to infer two arguments, the parentheses will be wrapped around `f` and the first two integers.

```lisp
(f 1 2) 3
```

Nested functions are inferred depth-first.

```lisp
f f 1 2 f 3 4
```

```lisp
(f (f 1 2) (f 3 4))
```

To be clear, parentheses are not actually inserted into the source code. Instead, inference creates new expressions in the AST.

There are two basic types:

`I`: Infer — If used in an expression type, inference will be run on this argument. If used as the top-level of the
type, then the identifier is a non-function variable.  
`L`: Literal — If used in an expression type, Inference will not be run on this argument or its sub-forms. If used as
the top-level of the type, then the identifier is treated as a variable.  

Normal variables have the type `L` or `I`. Functions have an expression as their type. The expression can be empty, or
it can contain a combination of `L` and `I`. Variadic functions are indicated by `&rest` in the third-to-last position
of the type. The default number of arguments for a variadic function is in the second-to-last position. Some examples:
`L`, `()`, `(I I I)`, `(L I)`, `(L L L &rest 1 I)`.

`I` is usually the type desired for function parameters. `L` is useful for certain macro parameters.

`var` is an example of a macro where `L` is useful. `var` has the type `(L I)`, meaning that it accepts two arguments
but inference is not run on the first argument. This allows `var` to treat the `name` argument as a literal identifier
and redefine that previously defined variable.

```lisp
(__defun double (v) (__* 2 v))
;; `__defun' will actually declare `double` as `(I)`, so this line is redundant in this case.
__declare double (I)

(__defmacro var (name value) (__list (__quote __var) name value))
;; `__defmacro` will declare this as `(I I)`, so it has to be redeclared.
__declare var (L I)

;; Define and declare `x' as a function.
var x (__lambda (y) (__- y 1))
;; This is *not* redundant.
__declare x (I)
(println x 4)  ; ⇒ 3
;; Redefine and declare `x' as an integer.
var x double 5  ; If the first argument of `var' was `I', `x' would consume `double' as an argument.
__declare x I
(println x)  ; ⇒ 10
;; Since `x' is declared as `I', it cannot be called as a function. The next line of code would throw an inference error.
;; (println (x))

;; Declare `y' as `L'.
var y (__lambda (a b) (__* a b))
__declare y L
;; `y' can be passed to functions as an argument.
(println (__apply y (__list 3 4)))  ; ⇒ 12
;; It can also be called without causing an inference error.
(println y 4 5)  ; ⇒ 20
```

### Disabling inference

Types can be overridden using `#symbol` or `#(expression)`. In both cases inference and arity checking is skipped for these forms. This is convenient when passing a function as a literal value, calling a normal variable as a function, or preventing the inferrer from declaring certain variables.

```lisp
;; Call the declared function `+` using `__apply`.
__declare + (I &rest 1 I)
(print __apply #+ (__list 1 2 3 4))
```

```lisp
;; Call a variable as a function without causing a type error.
__var double (__lambda (x) (__* 2 x))
;; Declare `double` as a normal variable.
__declare double L

(print (#double 5))  ; ⇒ 10
```

```lisp
;; Don't infer or type check an expression or any of its arguments. In other words, treat it as normal lisp code.
__declare + (I &rest 1 I)
#(print (__apply + (__list 1 2 3 4)))
```

### Declaration scripts

Unfortunately the use of these static type declarations is limited due to the existence of macros. To allow the type
system to understand macros such as `defun`, `__declare` can be passed a duck-lisp script as its third argument that
dynamically declares identifiers. When a declared function with an associated script is called, the script parses and
analyzes the arguments in order to declare additional identifiers used by arguments, or by forms that occur in the same
declaration scope. Duck-lisp scripts are run at inference-time. This is a separate instance of the duck-lisp compiler
and VM than is used in the rest of the language. This instance is used solely in the inferrer.

The inference-time instance of duck-lisp has three more functions than what is included by default in duck-lisp:

`(__infer-and-get-next-argument)::Any` — C callback — Switches to the next argument and runs inference on that
argument. Returns the resulting AST.  
`(__declare-identifier name::(Symbol String) type::(Symbol List))::Nil` — C callback — Declares the provided symbol
`name` as an identifier in the current declaration scope with a type specified by `type`.  
`(__declaration-scope body::Any*)::Any` — Generator — Create a new declaration scope. Identifiers declared in the body
using `__declare-identifier` are automatically deleted when the scope is exited.

Declaration script examples:

```lisp
;; `var' itself makes a declaration, so it requires a script. This declaration will persist until the end of the
;; scope `var' was used in.
(__declare var (L I)
           (__declare-identifier (__infer-and-get-next-argument)
                                 (__quote #L)))
```

```lisp
;; `defmacro' declares each of its parameters as a normal variable, and the provided name and `self` as functions of
;; the specified type. The parameters and `self` are scoped to the body while the declaration of the macro itself
;; persists until the end of the scope `defmacro' was used in.
(__declare defmacro (L L L &rest 1 I)
           (
            (__var name (__infer-and-get-next-argument))
            (__var parameters (__infer-and-get-next-argument))
            (__var type (__infer-and-get-next-argument))
            (__declaration-scope
             (__while parameters
                      __var parameter __car parameters
                      (__unless (__= (__quote #&rest) parameter)
                                (__declare-identifier parameter (__quote #L)))
                      (__setq parameters (__cdr parameters)))
             (__declare-identifier (__quote #self) type)
             (__infer-and-get-next-argument))
            (__declare-identifier name type)))
```

This system cannot recognize some macros due to the simplicity of the parsing functions used in inference-time
scripts. It cannot currently infer a complicated form like `let`.

## Tricks

### Fake symbol macros

Variables and functions that accept zero arguments look exactly the same.

```lisp
var      x       "Llama comma"  println x
defun    x ()    "Llama comma"  println x
defmacro x () () "Llama comma"  println x
```

In the last case, `x` acts like a Common Lisp symbol macro despite it being defined as a normal macro.

## Potential improvements

* Require declaration scripts to always terminate.
* Allow nested types, such as `((&rest 1 (L I)) &rest 1 I)`, which would greatly improve the quality of inference of `let`.
* Modify the type system to allow infix notation.
* Create new scopes using curly braces and add semicolons. 😜
