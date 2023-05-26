# Language

Welcome to the weirdest lisp you'll ever use!

## Syntax

S-expressions to the extreme. Even comments follow the rule.  
Every function and keyword follows the form `(verb noun noun …)`.  
`verb` must be an identifier.

## Comments

```lisp
(comment This is a comment.)
(; This is also a comment, though it may mess with your parenthesis completion.)
```

## Data types

Integers, floats, booleans, strings, cons, symbols, closures, vectors

Floats are not yet supported by all keywords. There are no built-in string keywords.

The type of a value can be converted to an integer corresponding to its type using the `type-of` keyword.

```lisp
(type-of 5)  (; ⇒ 2)
```

## Arithmetic

`+`, `-`, `*`, `/`, `>`, `<`, `=`, `not` are the built-in arithmetic operators. `=` tests equality. It does not perform assignment.

Arithmetic operators are generators, not functions, so they have no value. However, if you do want to treat them as functions, there is a decent workaround.

```lisp
(; Functions names are defined after the function body is defined, so the `+'
   being called is the operator.)
(defun + (a b)
  (+ a b))

(; Now use it with a higher-order function)
(defun apply2 (f a b)
  (f a b))

(print (apply2 + 4 5))  (; ⇒ 9)
```

## Variables

Duck-lisp has both lexically and globally scoped variables. Callbacks from C are created as globals. Global variables persist between bytecode executions on the VM and can be accessed by name from C.

For the most part, it should be safe to assume that all identifiers reside within a single namespace.

Scopes are created as in C, but using parentheses instead of curly braces. Confusing? Yes.  
Lexical variables are created as in C but using the `var` keyword. Global variables are created using the `global` keyword. The value argument is required for both `var` and `global`.

```lisp
(var x 5)
(print x)  (; ⇒ 5)
(
 (print x)  (; ⇒ 5)
 (var x 6)
 (print x))  (; ⇒ 6)
(print x)  (; ⇒ 5)

(; `y' will still exist when newly-compiled bytecode is run on the VM.)
(global y)
```

### Functions

`defun` generates lexically scoped functions. Functions are first class. Variadic functions are created like in Common Lisp, using the `&rest` keyword. They can be called using `funcall` and `apply`, which also come from Common Lisp. Recursion is possible (using `self`), but mutual recursion between two functions requires a third function to do the setup.

```lisp
(; Basic usage)
(defun mod (a b)
  (- a (* (/ a b) b)))
(mod 47 12)  (; ⇒ 11)

(; Variadic functions)
(include "../scripts/library.dl")  (; Import `println')
(defun print-args (&rest args)
  (print (length args))
  (println args)
  args)
(print-args 1 2 3 4 5)  (; Prints 5(1 2 3 4 5))

(; Recursion)
(defun fact (n)
  (if (= n 1)
    1
    (* n (self (- n 1)))))
(fact 5)  (; ⇒ 120)

(; Mutual recursion)
(; `f2' must be declared first, and then set.)
(var f2 ())
(defun f1 () (f2))
(setq f2 (lambda () (f1)))
(f1)  (; Does not return)

(; Higher-order functions)
(defun apply1 (f n) (f n))
(apply1 fact 5)  (; ⇒ 120)
```

Anonymous functions are created with lambdas.

```lisp
(var f (lambda () 5))
(f)  (; ⇒ 5)
```

Expressions are never treated as functions. Attempting to call them will wrap another scope around them, which does nothing.

```lisp
((lambda () 5))  (; ⇒ (closure 2))
```

The above merely returns the function. It is not called. `funcall` is used to call an expression as a function.

```lisp
(funcall (lambda () 5))  (; ⇒ 5)
```

If you need to destructure a list to use the elements as arguments, use `apply`.

```lisp
(apply >= (list 2 2))  (; ⇒ true)
```

`funcall` and `apply` do not work on function-like keywords.

```lisp
(apply + (list 2 2))  (; compoundExpression: Could not find variable "+".)
```

### Assignment

Assignment is done with `setq`. It acts like `=` in C.

```lisp
(var x 0)  (; ⇒ 0)
(setq x 5)  (; ⇒ 5)
x  (; ⇒ 5)
```

Assignment can be used to modify captured variables in closures.

```lisp
(var x 0)
(defun f ()
  (setq x 5)))
x  (; ⇒ 0)
(f)
x  (; ⇒ 5)
```

## Sequences

### Strings

The only support for strings is `print`. The only sort of string is a string literal.

### Conses and lists

The most common sequence type is the cons cell. A cons cell is a pair of values.

```lisp
(cons 4 2)  (; ⇒ (4 . 2))
```

The dot notation above is how a cons cell is printed. Duck-lisp cannot parse that syntax.

To access the first value, use `car`.

```lisp
(car (cons 4 2))  (; ⇒ 4)
```

To access the second value, use `cdr`.

```lisp
(cdr (cons 4 2))  (; ⇒ 2)
```

They can also be nested.

```lisp
(cons (cons 1 2) (cons 3 4))  (; ⇒ ((1 . 2) . (3 . 4)))
```

`()` is the syntax for nil. It is used as an end-marker for lists, and as a general default/error value. They keyword `null?` can be used to check if a value is nil.

```lisp
()  (; ⇒ nil)
(cons 1 (cons 2 (cons 3 (cons 4 ()))))  (; ⇒ (1 2 3 4))
(null? (cons 1 2))  (; ⇒ false)
(null? ())  (; ⇒ true)
```

If nil is added to the end of a chain of conses, it becomes a list.  
`list` offers a shorthand for writing chains of conses terminated by a nil.

```lisp
(list 1 2 3 4)  (; ⇒ (1 2 3 4))
```

A list without a nil on the end is called a dotted list. In general, dotted lists are less useful than normal lists since it's not as simple to tell where the end is.

```lisp
(cons 1 (cons 2 (cons 3 (cons 4 5))))  (; ⇒ (1 2 3 4 . 5))
```

### Vectors

Vectors are designed for O(1) element access, as opposed to lists which have O(n) element access.

Most list operations work the same on vectors as on lists. Currently, the only major difference is that `set-cdr` works on vectors only if the "cdr" is being set to `()` or `[]`.

My guess is that the speed of `cdr`ing down a vector is similar to that of a list.

Both `()` and `[]` are null values, but vectors do not have a quoted read syntax, so the only way to create `[]` is to do `(vector)`.

Vectors are created using the `vector` keyword.

```lisp
(print (vector 1 2 3 4 5))  (; ⇒ [1 2 3 4 5])
```

To create an array of arbitrary size at runtime, use `make-vector`. The first argument is the length, and the second argument is the object to initialize each element with.

```lisp
(make-vector 5 3)  (; ⇒ [3 3 3 3 3])
(make-vector 4 (vector 1 2 3))  (; ⇒ [[1 2 3] [1 2 3] [1 2 3] [1 2 3]])
```

Instead of using `car` and `cdr` to access elements, `get-vector-element` can be used instead. The first argument is the vector, and the second element is the index.

```lisp
(get-vector-element (vector 5 4 3 2 1) 3)  (; ⇒ 2)
```

To set an element, use `set-vector-element`. Its usage is the same as `get-vector-element`, but its third argument is the value to set the element to.

```lisp
(var x (vector 1 2 3))  (; ⇒ [1 2 3])
(set-vector-element x 1 0)  (; ⇒ 0)
(print x)  (; ⇒ [1 0 3])
```

Like `car`, `cdr`, `set-car`, and `set-cdr`, these two operations can be used together on trees.

```lisp
(var x (make-vector 4 (vector 1 2 3)))  (; ⇒ [[1 2 3] [1 2 3] [1 2 3] [1 2 3]])
(get-vector-element x 3)  (; ⇒ [1 2 3])
(set-vector-element (get-vector-element x 3) 2 10)  (; ⇒ 10)
(print x)  (; ⇒ [[1 2 10] [1 2 10] [1 2 10] [1 2 10]])
```


## Flow control

`if`, `when`, and `unless` should act the same as in Common Lisp. And if you're feeling clever, you can use functions instead (untyped lambda calculus).

```lisp
(if (> x 10)  (; condition)
  10  (; then)
  x)  (; else)
```

```lisp
(when (or (> x 10) (> y 10))  (; condition)
  (setq x 0)  (; then)
  (setq y 0)  (; also then)
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

## Metaprogramming

Duck-lisp supports `quote` and the symbol data type. This is enough to implement the metacircular evaluator.

```lisp
(print (quote (+ 4 17)))  (; ⇒ (+→0 4 17))
```

Common Lisp-like macros are also supported.

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

The most significant limitation is that there are separate runtime and compile time environments, which means that macros cannot call functions in the runtime environment and functions in the runtime environment cannot call functions in the compile time environment. The `comptime` keyword is provided to run code at compile time.

```lisp
(comptime
 (defun list* (&rest args)
   (if (null? (cdr args))
       (car args)
       (cons (car args) (apply self (cdr args))))))

(; `to' calls the compile time function `list*')
(defmacro to (variable form)
  (list (quote setq) variable (list* (car form) variable (cdr form))))
```

Macros are really just compile time functions declared in the runtime environment, so it is possible to call macros like functions at compile time.

```lisp
(defmacro and (&rest args)
  (if args
	  (list (quote if) (car args)
            (; `self' is always called as a function)
			(apply self (cdr args))
			false)
	  true))
```

Calling a macro using normal function call syntax at compile time still results in it being treated as a macro.

```lisp
(comptime
 (var x 4)
 (to x (+ 5))
 (print x))  (; ⇒ 9)
```

`funcall` can be used to explicitly force calling the macro as a function.

```lisp
(comptime
 (var x 4)
 (funcall to x (+ 5))  (; Error: `+' requires two arguments)
 (print x))
```

`comptime` can be used to calculate constants at compile time, but it is unable to pass closures to the runtime environment.

```lisp
(print (comptime (+ 3 4)))  (; ⇒ 7)
```
