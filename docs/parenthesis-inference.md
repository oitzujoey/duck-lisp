# Parenthesis inference

Duck-lisp has an optional parenthesis inferrer. This is yet another method of removing the parentheses from lisp, but unlike most of the others I have seen, this one is based on Forth and ignores formatting.

### Advantages

* Noobs can't complain about parentheses when there are none!
* Macros can declare variables using declaration scripts.
* Inferred duck-lisp is almost as easy to type on the command line as Forth.
* Formatting has no affect on how code is compiled.

### Disadvantages

* Noobs will now be horrified by the lack of parentheses and any sort of inherent visual structure. To be perfectly
  honest, I'm horrified too.
* No editor support. While the syntax is specifically designed to work nicely with sufficiently intelligent editors,
  support for this syntax must be written from scratch.
* Declaration scripts are Turing complete, though this is specific to this implementation and not to all Forth-like
  inference systems. It may be possible to substitute a regular or context-free language that can be proven to halt or
  else throw an inference error. I have not attempted this as the duck-lisp inferrer is a proof of concept.
* Not all common macros can be perfectly described with declaration scripts.

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
defun + (a &rest b)
  if b
     + a (apply #self b)  ; Optional parentheses for clarity
     a
declare + (I &rest 1 I)

print + + 1 2
        + 3 4  ; ⇒ 10
print "\n"
print (+ 1 2 3 4)  ; ⇒ 10
print "\n"
```

```lisp
;; Command line example. All functions used here have been previously defined and declared.
apply #+ map lambda (x) * 2 x (list 1 2 3 4)  ; ⇒ 20
```

```tcl
## Same example in Tcl
expr [join [lmap x {1 2 3 4} {expr 2*$x}] "+"]  ;# ⇒ 20
## Alternatively…
::tcl::mathop::+ {*}[lmap x {1 2 3 4} {expr 2*$x}]  ;# ⇒ 20
```

## Model

### Static types

Inference works by counting arguments. The inference stage is run between the parsing and compilation stages.

Types are declared with the keyword `declare`. It accepts two arguments by default. Inference is disabled for those arguments.

```lisp
declare * (I I)
declare if (I I I)
declare setq (L I)
```

Say this declaration exists:

```lisp
declare f (I I)
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

To be clear, parentheses are not inserted into the source code. Instead, the inferrer creates new expressions in the
AST.

There are two basic types:

`I`: Infer — If used in an expression type, inference will be run on this argument. If used as the top-level of the
type, then the identifier is a non-function variable.  
`L`: Literal — If used in an expression type, Inference will not be run on this argument or its sub-forms. If used as
the top-level of the type, then the identifier is treated as a variable.  

Normal variables have the type `L` or `I`. Functions have an expression as their type. The expression can be empty, or
it can contain a combination of `L` and `I`. Variadic functions are indicated by `&rest` in the third-to-last position
of the type. The default number of arguments for a variadic function is in the second-to-last position. The last
position in a variadic function type is the type that is used for each variadic argument. Some examples:
`L`, `()`, `(I I I)`, `(L I)`, `(L L L &rest 1 I)`.

`I` is usually the type desired for function parameters. `L` is useful for certain macro parameters.

`var` is an example of a macro where `L` is useful. `var` has the type `(L I)`, meaning that it accepts two arguments
but inference is not run on the first argument. This allows `var` to treat the `name` argument as a literal identifier
and redefine that previously defined variable.

```lisp
(defun double (v) (* 2 v))
;; `defun' will actually declare `double` as `(I)`, so this line is redundant in this case.
declare double (I)

(defmacro global (name value) (list (quote __global) name value))
;; `defmacro` will declare this as `(I I)`, so it has to be redeclared.
declare global (L I)

;; Define and declare `x' as a function.
global x (lambda (y) (- y 1))
;; This is *not* redundant.
declare x (I)
(println x 4)  ; ⇒ 3
;; Redefine and declare `x' as an integer.
global x double 5  ; If the first argument of `global was `I', `x' would consume `double' as an argument.
declare x I
(println x)  ; ⇒ 10
;; Since `x' is declared as `I', it cannot be called as a function. The next line of code would throw an inference error.
;; (println (x))

;; Declare `y' as `L'.
global y (lambda (a b) (* a b))
declare y L
;; `y' can be passed to functions as an argument.
(println (apply y (list 3 4)))  ; ⇒ 12
;; It can also be called without causing an inference error.
(println (y 4 5))  ; ⇒ 20
```

### Disabling inference

Types can be overridden using `#symbol` or `#(expression)`. In both cases inference and arity checking is skipped for these forms. This is convenient when passing a function as a literal value, calling a normal variable as a function, or preventing the inferrer from declaring certain variables.

```lisp
;; Call the declared function `+` using `apply`.
declare + (I &rest 1 I)
(print apply #+ (list 1 2 3 4))
```

```lisp
;; Call a variable as a function without causing a type error.
global double (lambda (x) (* 2 x))
;; Declare `double` as a normal variable.
declare double L

(print (#double 5))  ; ⇒ 10
```

```lisp
;; Don't infer or type check an expression or any of its arguments. In other words, treat it as normal lisp code.
declare + (I &rest 1 I)
#(print (apply + (list 1 2 3 4)))
```

### Declaration scripts

Unfortunately the use of these static type declarations is limited due to the existence of macros. To allow the type
system to understand macros such as `defun`, `declare` can be passed a duck-lisp script as its third argument that
dynamically declares identifiers. When a declared function with an associated script is called, the script parses and
analyzes the arguments in order to declare additional identifiers used by arguments, or by forms that occur in the same
declaration scope. Duck-lisp scripts are run at inference-time. This is a separate instance of the duck-lisp compiler
and VM than is used in the rest of the language. This instance is used solely in the inferrer.

The inference-time instance of duck-lisp has three more functions than what is included by default in duck-lisp:

`(infer-and-get-next-argument)::Any` — C callback — Switches to the next argument and runs inference on that
argument. Returns the resulting AST.  
`(declare-identifier name::(Symbol String) type::(Symbol List))::Nil` — C callback — Declares the provided symbol
`name` as an identifier in the current declaration scope with a type specified by `type`.  
`(declaration-scope body::Any*)::Any` — Generator — Create a new declaration scope. Identifiers declared in the body
using `declare-identifier` are automatically deleted when the scope is exited.

Declaration script examples:

```lisp
;; `var' itself makes a declaration, so it requires a script. This declaration will persist until the end of the
;; scope `var' was used in.
(declare var (L I)
         (declare-identifier (infer-and-get-next-argument)
                             (quote #L)))
```

```lisp
;; `defmacro' declares each of its parameters as a normal variable, and the provided name and `self` as functions of
;; the specified type. The parameters and `self` are scoped to the body while the declaration of the macro itself
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
```

This system cannot recognize some macros due to the simplicity of the parsing API provided to inference-time scripts. It
cannot currently infer a complicated form like `let`.

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

* Create separate declaration environments for compile-time and runtime identifiers.
* Require declaration scripts to always terminate.
* Allow nested types, such as `((&rest 1 (L I)) &rest 1 I)`, which would greatly improve the quality of inference of `let`.
* Modify the type system to allow infix notation.
* Create new scopes using curly braces and add semicolons. 😜
