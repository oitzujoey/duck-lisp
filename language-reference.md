# Language reference

## (__nop)

Insert a nop instruction. For testing only.

## (__funcall function::(Callback Closure Composite) args*)

Call a callback, function, or closure `function`, passing arguments `args`.

## (__apply function::(Callback Closure Composite) args::Any* last-arg::List)

Call a callback, function or closure `function`, passing arguments `args`. Pass the contents of `last-arg` as arguments as well.

## (__var name::Identifier value::Any)

Define a lexically-scoped local variable with name `name` and initialize it to `value`.

## (__global name::Identifier value::Any)

Define a global variable with name `name` and initialize it to `value`.

## (__setq variable::Identifier value::Any)

Set a variable `name` and assign the object `value` to it.

## (__not value::(Boolean Integer Float List Vector String))

Perform the logical operation NOT on `value`.

## (__* left::(Boolean Integer Float) right::(Boolean Integer Float))

Multiply `left` and `right`.

## (__/ left::(Boolean Integer Float) right::(Boolean Integer Float))

Divide `left` and `right`.

## (__+ left::(Boolean Integer Float) right::(Boolean Integer Float))

Add `left` and `right`.

## (__- left::(Boolean Integer Float) right::(Boolean Integer Float))

Subtract `left` and `right`.

## (__while condition::Any body::Any*)

Evaluate body forms while `condition` evaluates to `true`.

## (__if condition::Any then::Any else::Any)

If `condition` evaluates to `true`, evaluate `then`, otherwise, evaluate `else`.

## (__when condition::Any then::Any)

If `condition` evaluates to `true`, evaluate `then`.

## (__unless condition::Any else::Any)

If `condition` evaluates to `false`, evaluate `else`.

## (__= left::Any right::Any)

Return `true` if `left` and `right` are sufficiently equal. Otherwise return `false`.

## (__< left::(Boolean Integer Float) right::(Boolean Integer Float))

If `left` is less than `right`, return `true`, otherwise return `false.

## (__> left::(Boolean Integer Float) right::(Boolean Integer Float))

If `left` is greater than `right`, return `true`, otherwise return `false.

## (__defun name::Identifier (parameters::Identifier*) body::Any*)

Define a function and bind it to `name`. Return the closure.

## (__lambda (parameters::Identifier*) body::Any*)

Return a function.

## (__defmacro name::Identifier (parameters::Identifier*) body::Any*)

Define a macro.

## (__noscope body::Any*)

Evaluate body forms. Return the result of the last form.

## (__comptime body::Any*)

Evaluate body forms in the compile-time environment. Return the result of the last form.

## (__quote form::Any)

Convert the AST representation of `form` to a similar structure that is represented as one or more objects.

Booleans, integers, floats, and strings remain the same.
Identifiers are converted to symbols.  
Expressions are converted to lists.

## (__list args::Any*)

`args` are placed in a list.

## (__vector args::Any*)

`args` are placed in a vector.

## (__make-vector length::Integer fill::Any)

Create a vector of length `length` and fill each element with a copy of `fill`.

## (__get-vector-element vector::Vector index::Integer)

Return an element of `vector` indexed by `index`.

## (__set-vector-element vector::Vector index::Integer value::Any)

Set an element of `vector` indexed by `index` to `value`.

## (__cons left::Any right::Any)

Create a cons cell filled with `left` and `right`.

## (__car sequence::(List Vector String))

Return the first element of `sequence`.

## (__cdr sequence::(List Vector String))

Return `sequence` except the first element.

## (__set-car sequence::(List Vector String) value::Any)

Set the first element of `sequence` to `value`.

## (__set-cdr sequence::(List Vector String) value::Any)

If `sequence` is a list, then set the cdr to `value`. If `sequence` is a vector or string and `value` is the null value of that type, truncate the sequence right after the current element.

## (__null? value::Any)

Check if `value` is a null value.

## (__type-of value::Any)

Return the type of `value`. If `value` is a composite, return the value of the type slot.

## (__make-type)

Return a new unique user-defined type value.

## (__make-instance type::Type value::Any function::Any)

Create a composite of type `type` with `value` in the value slot and `function` in the function slot.

## (__composite-value composite::Composite)

Return the contents of the value slot of `composite`.

## (__composite-function composite::Composite)

Return the contents of the function slot of `composite`.

## (__set-composite-value composite::Composite value::Any)

Set the contents of the value slot in `composite` to `value.

## (__set-composite-function composite::Composite function::Any)

Set the contents of the function slot in `composite` to `value.

## (__make-string sequence::(List Vector))

Create a string from a sequence of integers.

## (__concatenate left::String right::String)

Append string `right` to the end of string `left`.

## (__substring string::String start-index::Integer end-index::Integer)

Return the subset of `string` starting from the index `start-index` and ending before the index `end-index`.

## (__length sequence::(List Vector String))

Return the length of the sequence.

## (__symbol-string symbol::Symbol)

Return the name of `symbol` as a string.

## (__symbol-id symbol::Symbol)

Return the unique ID of `symbol` as a string.

## (__error message::String)

Throw a compilation error using `message` as the error message.
