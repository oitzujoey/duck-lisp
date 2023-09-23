# Language

Welcome to duck-lisp!

I intend for you to read this manual top-down, but sometimes I felt the need to use advanced concepts before they are defined. You may need to skip around or reread sections to properly understand the manual.

## Syntax

S-expressions are taken to the extreme. Not even `quote` has special syntax. Every function and keyword follows the form `(verb noun noun …)`. `verb` must be an identifier. There's an alternative syntax available though, so take a look at "parenthesis-inference.md" when you tire of the parentheses.

Despite what is shown in this document, all keywords are preceded by two underscores. "language-reference.md" contains the full list of keywords and built-in functions. Every form in that document is mentioned in this one as well, but without the underscores since underscores are unsightly.

## Comments

```lisp
; This is a comment.
```

## Data types

Integers, floats, booleans, strings, cons, symbols, closures, vectors, types, composites

The type of a value can be queried using the `type-of` keyword. The type of the returned value is a type.

```lisp
(type-of 5)  ; ⇒ ::2
```

## Arithmetic

`+`, `-`, `*`, `/`, `>`, `<`, `=`, `not` are the built-in arithmetic operators. All arithmetic operators except `not` require exactly two arguments. `=` tests equality. It does not perform assignment.

Arithmetic operators are generators, not functions, so they have no value. However, if you do want to treat them as functions, there is a decent workaround.

```lisp
;; Functions names are defined after the function body is defined, so the `+'
;; being called is the operator.
(defun + (a b)
  (+ a b))

;; Now use it with a higher-order function
(defun apply2 (f a b)
  (f a b))

(print (apply2 + 4 5))  ; ⇒ 9
```

## Variables

Duck-lisp has both lexically and globally scoped variables. Callbacks from C are created as globals and can be accessed by name from C. Global variables persist between bytecode executions in the VM.

Scopes are created as in C, but using parentheses instead of curly braces. Confusing? Yes.  
Lexical variables are created as in C but using the `var` keyword. Global variables are created using the `global` keyword. The value argument is required for both `var` and `global`.

```lisp
(var x 5)
(print x)  ; ⇒ 5
(
 (print x)  ; ⇒ 5
 (var x 6)
 (print x))  ; ⇒ 6
(print x)  ; ⇒ 5

;; `y' will still exist when newly-compiled bytecode is run on the VM.
(global y 7)
```

Duck-lisp is a modified lisp-2. A lisp-2 separates functions and normal variables into two separate namespaces. Duck-lisp puts functions in their own namespace, but functions are also placed in the namespace for variables.

```lisp
;; `add-function' is placed into both the function and variable namespaces.
(defun add-function (a b) (+ a b))
add-function  ; ⇒ (closure $2 #2)
(funcall add-function 1 2)  ; ⇒ 3
(add-function 1 2)  ; ⇒ 3

;; `add-variable' is only placed into the variable namespace.
(var add-variable (lambda (a b) (+ a b)))
add-variable  ; ⇒ (closure $2 #2)
(funcall add-variable 1 2)  ; ⇒ 3
(add-variable 1 2)  ; Error. `add-variable` is not defined as a function, so it is assumed to be a global function.
```

These are the rules for each type of variable:

* Global variables can be called as functions and used as normal variables.
* Normal variables cannot be called as functions except using `funcall`.
* Functions can be called as functions and used as normal variables.
* Macros can be called as functions (triggering a macro expansion) and used as normal variables. Calling macros using funcall calls them like a normal function instead of causing a macro expansion.

If a variable, function, or macro is not found to be defined, then the identifier is assumed to be a global variable.

### Functions

`defun` generates lexically scoped functions. Functions are first class. Variadic functions are created like in Common Lisp, using the `&rest` keyword. They can be called using `funcall` and `apply`, which also come from Common Lisp. Recursion is possible (using `self`), but mutual recursion between two functions requires a third function to do the setup. The duck-lisp compiler does not perform tail call optimization.

```lisp
;; Basic usage
(defun mod (a b)
  (- a (* (/ a b) b)))
(mod 47 12)  ; ⇒ 11

;; Variadic functions
(include "../scripts/library.dl")  ; Import `println'
(defun print-args (&rest args)
  (print (length args))
  (println args)
  args)
(print-args 1 2 3 4 5)  ; Prints 5(1 2 3 4 5)

;; Recursion
(defun fact (n)
  (if (= n 1)
    1
    (* n (self (- n 1)))))
(fact 5)  (; ⇒ 120)

;; Mutual recursion
;; `f2' must be declared first, and then set.
(var f2 ())
;; Since `f2' is defined as a variable, it must be called using `funcall'.
(defun f1 () (funcall f2))
(setq f2 (lambda () (f1)))
(f1)  ; Does not return

;; Higher-order functions
(defun apply1 (f n) (f n))
(apply1 fact 5)  ; ⇒ 120
```

Anonymous functions are created with lambdas.

```lisp
(var f (lambda () 5))
(funcall f)  ; ⇒ 5
```

Expressions are never treated as functions. Attempting to call them will wrap another scope around them, which does nothing.

```lisp
((lambda () 5))  ; ⇒ (closure 2)
```

The above merely returns the function. It is not called. `funcall` must used to call expressions as functions.

```lisp
(funcall (lambda () 5))  ; ⇒ 5
```

If you need to destructure a list to use the elements as arguments, use `apply`.

```lisp
(apply >= (list 2 2))  ; ⇒ true
```

`funcall` and `apply` do not work on function-like keywords.

```lisp
;; `+' is a keyword, not a function.
(apply + (list 2 2))  ; compoundExpression: Could not find variable "+".
```

### Assignment

Assignment is done with `setq`. It acts like `=` in C.

```lisp
(var x 0)  ; ⇒ 0
(setq x 5)  ; ⇒ 5
x  ; ⇒ 5
```

Assignment can be used to modify captured variables in closures.

```lisp
(var x 0)
(defun f ()
  (setq x 5)))
x  ; ⇒ 0
(f)
x  ; ⇒ 5
```

## Primitive types

### Booleans

There are two literal boolean values: `true` and `false`.

### Integers

The minimum and maximum integer values an object can contain is unspecified.

Literal decimal integer values are defined by this regex: `-?[0-9]+`  
Literal hexadecimal integer values are defined by this regex: `-0[xX]?[0-9]+`

### Floats

The internal format of floating point values is unspecified. Floating point math is not IEEE-754 compliant.

Literal floating point values are defined by this regex: `-?(([0-9]+\.[0-9]*)|([0-9]*\.[0-9]+)(e-?[0-9]+)?)|([0-9]e-?[0-9]+)`

### Symbols

Symbols are created by quoting an identifier.

```lisp
(quote I'm-a-symbol!)  ; ⇒ I'm-a-symbol!→15
```

They can be checked for equality with other symbols.

```lisp
(= (quote I'm-a-symbol!) (quote I'm-a-symbol!))  ; ⇒ true
(= (quote I'm-a-symbol!) (quote I'm-another-symbol!))  ; ⇒ false
```

An integer unique to the symbol can be retrieved using `symbol-id`. The symbol's name can be retrieved using `symbol-string`. Symbols are immutable.

```lisp
(symbol-id (quote I'm-a-symbol!))  ; ⇒ 15
(symbol-string (quote I'm-a-symbol!))  ; ⇒ "I'm-a-symbol!"
```


## Sequence types

### Conses and lists

The most common sequence type is the cons cell. A cons cell is a pair of values.

```lisp
(cons 4 2)  ; ⇒ (4 . 2)
```

The dot notation above is how a cons cell is printed. Duck-lisp cannot parse that syntax.

To access the first value, use `car`.

```lisp
(car (cons 4 2))  ; ⇒ 4
```

To access the second value, use `cdr`.

```lisp
(cdr (cons 4 2))  ; ⇒ 2
```

They can also be nested.

```lisp
(cons (cons 1 2) (cons 3 4))  ; ⇒ ((1 . 2) . (3 . 4))
```

`()` is the syntax for nil. It is used as an end-marker for lists, and as a general default/error value. They keyword `null?` can be used to check if a value is nil.

```lisp
()  (; ⇒ nil)
(cons 1 (cons 2 (cons 3 (cons 4 ()))))  ; ⇒ (1 2 3 4)
(null? (cons 1 2))  (; ⇒ false)
(null? ())  (; ⇒ true)
```

If nil is added to the end of a chain of conses, it becomes a list.  
`list` offers a shorthand for writing chains of conses terminated by a nil.

```lisp
(list 1 2 3 4)  ; ⇒ (1 2 3 4)
```

A list without a nil on the end is called a dotted list. In general, dotted lists are less useful than normal lists since it's not as simple to tell where the end is.

```lisp
(cons 1 (cons 2 (cons 3 (cons 4 5))))  ; ⇒ (1 2 3 4 . 5)
```

### Vectors

Vectors have O(1) element access, as opposed to lists which have O(n) element access.

Most list operations work the same on vectors as on lists. Currently, the only major difference is that `set-cdr` works on vectors only if the "cdr" is being set to `()` or `[]`.

My guess is that the speed of `cdr`ing down a vector is similar to that of a list.

Both `()` and `[]` are null values, but vectors do not have a quoted read syntax, so the only way to create `[]` is to do `(vector)`.

Vectors are created using the `vector` keyword.

```lisp
(print (vector 1 2 3 4 5))  ; ⇒ [1 2 3 4 5]
```

To create an array of arbitrary size at runtime, use `make-vector`. The first argument is the length, and the second argument is the object to initialize each element with.

```lisp
(make-vector 5 3)  ; ⇒ [3 3 3 3 3]
(make-vector 4 (vector 1 2 3))  ; ⇒ [[1 2 3] [1 2 3] [1 2 3] [1 2 3]]
```

The length can be obtained with the `length` keyword.

```lisp
(length (vector () () () () ()))  ; ⇒ 5
```

Instead of using `car` and `cdr` to access elements, `get-vector-element` can be used instead. The first argument is the vector, and the second element is the index.

```lisp
(get-vector-element (vector 5 4 3 2 1) 3)  ; ⇒ 2
```

To set an element, use `set-vector-element`. Its usage is the same as `get-vector-element`, but its third argument is the value to set the element to.

```lisp
(var x (vector 1 2 3))  ; ⇒ [1 2 3]
(set-vector-element x 1 0)  ; ⇒ 0
(print x)  ; ⇒ [1 0 3]
```

Like `car`, `cdr`, `set-car`, and `set-cdr`, these two operations can be used together on trees.

```lisp
(var x (make-vector 4 (vector 1 2 3)))  ; ⇒ [[1 2 3] [1 2 3] [1 2 3] [1 2 3]]
(get-vector-element x 3)  ; ⇒ [1 2 3]
(set-vector-element (get-vector-element x 3) 2 10)  ; ⇒ 10
(print x)  ; ⇒ [[1 2 10] [1 2 10] [1 2 10] [1 2 10]]
```

### Strings

Literal strings are any sequence of characters surrounded by quotes. Quotes are also permitted in strings as long as each one is preceded by a backslash character. The character 'n' preceded by a backslash is converted into a newline character.

All vector operations work on strings. List operations work on strings, with the same limitations as vectors. `set-car` and `set-vector-element` have the additional limitation that the argument must be an integer between 0 and 255. Like with vectors, calling `null?` on an empty string returns true.

Strings and symbols can be concatenated using the `concatenate` keyword.

```lisp
(concatenate "Hello, " "world!\n")  ; ⇒ "Hello, world!"
(concatenate (quote Hello,) (quote world!\n))  ; ⇒ "Hello,world!\n"
```

A portion of a string can be extracted using the `substring` command. The first argument is the string, the second is the start index, and the third is the end index. The last returned character originates from the index just before the end index.

```lisp
(substring "0123456789" 3 6)  ; ⇒ "345"
```


## Composites

Composites are user-defined types that have nearly the same language support as native types. Unlike OOP objects, they do not provide encapsulation or a convenient way to structure data. Data still has to be structured using built-in sequences. There are two advantages to placing data in composites instead of storing raw data. The first is that `type-of` returns a type value that is distinct from built-in types. The second is that composites can be called as functions.

To create a composite, first create a new type. For this example, we will implement a composite that stores a complex number.

```lisp
(var complex-type (make-type))  ; ⇒ ::20
```

Next, prepare the data to store in the composite.

```lisp
(var real-part 3.0)
(var imag-part 4.0)
(var internal-complex (cons real-part imag-part))
```

Now create a function to use when the composite is called. Since it doesn't really make sense to call a complex number, let's just return nil.

```lisp
(defun complex-function () ())
```

And finally, create the composite.

```lisp
(make-instance complex-type internal-complex complex-function)  ; ⇒ (composite ::20 (cons 3.0 4.0) (closure 2))
```

It's probably best to create a proper constructor.

```lisp
(defun make-complex (real imag)
  (make-instance complex-type (cons real imag) (lambda () ())))

(var complex-number (make-complex 1.2 -3.3))
```

`type-of` works on this new data type.

```lisp
(= complex-type (type-of complex-number))  ; ⇒ true
```

`composite-value` and `composite-function` return the value and function slots of the composite. `set-composite-value` and `set-composite-function` set the value and function slots.

```lisp
(defun real-part (complex)
  (car (composite-value complex)))
(defun imag-part (complex)
  (cdr (composite-value complex)))

;; There isn't generally a good reason to do this for a complex type.
(set-composite-value complex (cons -5.9 -6.3))
```

Combined with function redefinition, we now have the ability to create something like dynamic dispatch.

```lisp
(defun + (a b)
  (if (and (= complex-type (type-of a))
           (= complex-type (type-of b)))
      (
       (var a (composite-value a))
       (var b (composite-value b))
       (make-complex (+ (car a) (car b))
                     (+ (cdr a) (cdr b))))
      (+ a b)))
```

Using the ability to call composites like functions, we can simulate message passing.

```lisp
(var inc (quote inc))
(var dec (quote dec))
;; Unfortunately, we need to declare the object beforehand so that the
;; lambda can capture it, but this wordiness can be fixed with a macro
(var object ())
(setq object (make-instance (make-type)
                            0
                            (lambda (message)
                              (set-composite-value (+ (if (= message inc)
                                                          1
                                                          (if (= message dec)
                                                              -1
                                                              0))
                                                      (composite-value object))))))
(print (funcall composite-value object))  ; ⇒ 0
(object inc)
(print (funcall composite-value object))  ; ⇒ 1
(object dec)
(object dec)
(object dec)
(print (funcall composite-value object))  ; ⇒ -2
```


## Flow control

`if`, `when`, and `unless` should act the same as in Common Lisp. And if you're feeling clever, you can use functions instead (untyped lambda calculus).

```lisp
(if (> x 10)  ; condition
  10  ; then
  x)  ; else
```

```lisp
(when (or (> x 10) (> y 10))  ; condition
  (setq x 0)  ; then
  (setq y 0)  ; also then
  …)
```

`(when condition body…)` acts the same as code below.

```lisp
(if condition
  (
   body…)
  ())
```

`unless` is the opposite of `when`. It acts similar to the code below.

```lisp
(when (not condition)
  body…)
```

`while` acts like C's `while` keyword. It always returns nil.

```lisp
(var x 0)
(while (< x 10)
  (setq x (print (+ x 1)))
  (print "\n"))
```


Compilation can be terminated with an error using `error`. A compile error occurs as soon as this form is encountered. The argument is the user-defined error message.

```lisp
(error "Compile error!")
```


## Metaprogramming

Duck-lisp supports `quote` and the symbol data type. This is enough to implement the metacircular evaluator.

```lisp
(print (quote (+ 4 17)))  ; ⇒ (+→0 4 17)
```

Common Lisp-like macros are also supported, with the major difference that functions cannot be hygienically used in macros (thus why duck-lisp is a lisp-2).

```lisp
(defmacro to (variable form)
  (var function (car form))
  (var args (cdr form))
  (var subform (list function variable))
  (set-cdr (cdr subform) args)
  (list (quote setq) variable subform))
```

```lisp
(defun nthcdr (n list)
  (var i 0)
  (while (< i n)
	(to list (cdr))
	(to i (1+)))
  list)
```

Macros, like functions, can be variadic.

```lisp
(defmacro to (variable function &rest args)
  (var subform (list function variable))
  (set-cdr (cdr subform) args)
  (list (quote setq) variable subform))
```

```lisp
(defun nthcdr (n list)
  (var i 0)
  (while (< i n)
	(to list cdr)
	(to i 1+))
  list)
```

or alternatively,

```lisp
(defun nthcdr (n list)
  (var i 0)
  (while (< i n)
	(to list cdr)
	(to i + 1))
  list)
```

The most significant limitation is that there are separate runtime and compile-time environments, which means that macros cannot call functions in the runtime environment, and functions in the runtime environment cannot call functions in the compile-time environment. The `comptime` keyword is provided to run code at compile-time. Another limitation is that macro definitions may not be nested or appear in a `comptime` form.

```lisp
(comptime
 (defun list* (&rest args)
   (if (null? (cdr args))
       (car args)
       (cons (car args) (apply self (cdr args))))))

;; `to' calls the compile time function `list*'
(defmacro to (variable form)
  (list (quote setq) variable (list* (car form) variable (cdr form))))
```

Macros are compile-time functions declared in the runtime environment, so it is possible to call macros like functions at compile-time.

```lisp
(defmacro and (&rest args)
  (if args
	  (list (quote if) (car args)
            ;; `self' is always called as a function, and never as a macro.
			(apply self (cdr args))
			false)
	  true))
```

Calling a macro using normal function call syntax at compile time still results in it being treated as a macro.

```lisp
(comptime
 (var x 4)
 (to x (+ 5))
 (print x))  ; ⇒ 9
```

`funcall` can be used to explicitly force calling the macro as a function. This only works in compile-time code.

```lisp
(comptime
 (print (funcall to (quote x) (quote (+ 5))))  ; ⇒ (setq x (+ x 5))
 ())
```

`comptime` can be used to calculate constants at compile time, but it is unable to pass closures to the runtime environment.

```lisp
(print (comptime (+ 3 4)))  ; ⇒ 7
```

Sometimes is convenient to return multiple unscoped forms from a macro. The most common reason to do this is to create new bindings in the caller's scope. Wrapping the bindings in a list would bundle them together in one form, but would also create a new scope. The bindings would not be visible to code that occur after the macro call. Instead, they should be wrapped in the `noscope` keyword.

```lisp
(defun declare-variables (&rest names)
  (cons (quote noscope)
        (mapcar (lambda (name) (list (quote var) name ())) names)))

(
 (declare-variables a b c)

 (setq b 3)
 …)
```

This expands to

```lisp
(
 (noscope
  (var a ())
  (var b ())
  (var b ()))

 (setq b 3)
 …)
```

There are three helper functions for macros that are defined only at compile time. `gensym` creates a unique symbol. The returned symbols are nearly unreadable, so it is supplemented by `intern`, which accepts a string and returns a symbol with the name it was passed. `read` parses strings into duck-lisp AST.

```lisp
(defmacro quote-gensym ()
  (list (quote quote) (gensym)))
(print (quote-gensym))  ; ⇒ 3000000000000000→15
```

```lisp
(defmacro quote-named-gensym (name)
  (list (quote quote) (intern (concatenate name (gensym)))))
(print (quote-named-gensym "Fred-"))  ; ⇒ Fred-2000000000000000→15
```

```lisp
(defmacro dotimes (variable top &rest body)
  ;; Create a unique symbol with a readable name.
  (var top-var (intern (concatenate "top-var:" (gensym))))
  (list
   (list (quote var) variable 0)
   (list (quote var) top-var top)
   (list (quote while) (list (quote <) variable top-var)
         (list* (quote noscope) body)
         (list (quote to) variable (quote (1+))))
   ;; `top-var' will print as `top-var:6D30000000000000→316'.
   (println top-var)
   top-var))
```

```lisp
(__defmacro read! (string)
            ;; Parse the string.
            (__var ast (read string true))
            (__var error (__cdr ast))
            (__setq ast (__car ast))
            ;; Error handling
            (__if error
                  (__list (__quote __quote)
                          (__quote error)
                          error)
                  ast))
(read! "
(()
 __declare print (I)
 print \"Hello, world!\n\")
")  ; ⇒ Hello, world!
```

`gensym`, `intern`, and `read` are C functions, not keywords, so they have all the advantages of functions.
