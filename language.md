# Language

Welcome to the weirdest lisp you'll ever use!

## Syntax

S-expressions to the extreme. Even comments follow the rule.  
Every function and keyword follows the form `(verb noun noun …)`.  
`verb` must be an identifier.

## Comments

```lisp
(comment This is a comment.)
(; This is also a comment, though may mess with your parenthesis completion.)
```

## Data types

Integers, floats, booleans, strings, cons, symbols, closures

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

Variables are lexically scoped. The only exception at the moment are global callbacks that can be set from C.

For the most part, it should be safe to assume that all identifiers reside within a single namespace.

Scopes are created as in C, but using parentheses instead of curly braces. Confusing? Yes.  
Variables are created as in C, but the value argument is required.

```lisp
(var x 5)
(print x)  (; ⇒ 5)
(
 (print x)  (; ⇒ 5)
 (var x 6)
 (print x))  (; ⇒ 6)
(print x)  (; ⇒ 5)
```

### Functions

`defun` generates lexically scoped functions. Functions are first class. It is not currently possible to declare functions with a variable number of arguments. A workaround is to pass a list to one of the arguments. Recursion is possible (using `self`), but mutual recursion between two functions requires a third function to do the setup.

```lisp
(; Basic usage)
(defun mod (a b)
  (- a (* (/ a b) b)))
(mod 47 12)  (; ⇒ 11)

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

The only support for strings is `print`. The only sort of string is a string literal.

The only other sequence type is the cons cell. A cons cell is a pair of values.

```lisp
(cons 4 2)  (; ⇒ (4 . 2))
```

The dot notation above is how a cons cell is printed. duck-lisp cannot parse that syntax.

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

## Flow control

`if`, `when`, and `unless` should act the same as in Common Lisp. And if you're feeling clever, you can use functions instead.

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

Only support at the moment for metaprogramming is `quote` and the symbol data type.

```lisp
(print (quote (+ 4 17)))  (; ⇒ (+→0 4 17))
```

Still, it is possible to implement `eval` in duck-lisp.
