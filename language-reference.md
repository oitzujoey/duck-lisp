# Language reference

## Keywords

### (__nop)

Insert a nop instruction. Does not return anything. For testing only.

### (__funcall function::(Callback Closure Composite) args*)::Any

Call a callback, function, or closure `function`, passing arguments `args`.

### (__apply function::(Callback Closure Composite) args::Any* last-arg::List)::Any

Call a callback, function or closure `function`, passing arguments `args`. Pass the contents of `last-arg` as arguments as well.

### (__var name::Identifier value::Any)::Any

Define a lexically-scoped local variable with name `name` and initialize it to `value`.

### (__global name::Identifier value::Any)::Any

Define a global variable with name `name` and initialize it to `value`.

### (__setq variable::Identifier value::Any)::Any

Set a variable `name` and assign the object `value` to it.

### (__not value::(Boolean Integer Float List Vector String))::(Boolean Integer Float List Vector String)

Perform the logical operation NOT on `value`.

### (__* left::(Boolean Integer Float) right::(Boolean Integer Float))::(Boolean Integer Float)

Multiply `left` and `right`.

### (__/ left::(Boolean Integer Float) right::(Boolean Integer Float))::(Boolean Integer Float)

Divide `left` and `right`.

### (__+ left::(Boolean Integer Float) right::(Boolean Integer Float))::(Boolean Integer Float)

Add `left` and `right`.

### (__- left::(Boolean Integer Float) right::(Boolean Integer Float))::(Boolean Integer Float)

Subtract `left` and `right`.

### (__while condition::Any body::Any*)::Nil

Evaluate body forms while `condition` evaluates to `true`.

### (__if condition::Any then::Any else::Any)::Any

If `condition` evaluates to `true`, evaluate `then`, otherwise, evaluate `else`.

### (__when condition::Any then::Any)::Any

If `condition` evaluates to `true`, evaluate `then`.

### (__unless condition::Any else::Any)::Any

If `condition` evaluates to `false`, evaluate `else`.

### (__= left::Any right::Any)::Boolean

Return `true` if `left` and `right` are sufficiently equal. Otherwise return `false`.

### (__< left::(Boolean Integer Float) right::(Boolean Integer Float))::Boolean

If `left` is less than `right`, return `true`, otherwise return `false.

### (__> left::(Boolean Integer Float) right::(Boolean Integer Float))::Boolean

If `left` is greater than `right`, return `true`, otherwise return `false.

### (__defun name::Identifier (parameters::Identifier*) body::Any*)::Closure

Define a function and bind it to `name`. Return the closure.

### (__lambda (parameters::Identifier*) body::Any*)::Closure

Return a function.

### (__defmacro name::Identifier (parameters::Identifier*) body::Any*)::(Nil Closure)

Define a macro. Returns `()` if evaluated in the runtime environment, and returns the resulting closure in the compile-time environment.

### (__noscope body::Any*)::Any

Evaluate body forms. Return the result of the last form.

### (__comptime body::Any*)::(Boolean Integer Float Symbol List Vector String)

Evaluate body forms in the compile-time environment. Return the result of the last form.

### (__quote form::Any)::(Boolean Integer Float Symbol List String)

Convert the AST representation of `form` to a similar structure that is represented as one or more objects.

Booleans, integers, floats, and strings remain the same.  
Identifiers are converted to symbols.  
Expressions are converted to lists.

### (__list args::Any*)::List

`args` are placed in a list.

### (__vector args::Any*)::Vector

`args` are placed in a vector.

### (__make-vector length::Integer fill::Any)::Vector

Create a vector of length `length` and fill each element with a copy of `fill`.

### (__get-vector-element vector::Vector index::Integer)::Any

Return an element of `vector` indexed by `index`.

### (__set-vector-element vector::Vector index::Integer value::Any)::Any

Set an element of `vector` indexed by `index` to `value`.

### (__cons left::Any right::Any)::List

Create a cons cell filled with `left` and `right`.

### (__car sequence::(List Vector String))::Any

Return the first element of `sequence`.

### (__cdr sequence::(List Vector String))::Any

Return `sequence` except the first element.

### (__set-car sequence::(List Vector String) value::Any)::Any

Set the first element of `sequence` to `value`.

### (__set-cdr sequence::(List Vector String) value::Any)::Any

If `sequence` is a list, then set the cdr to `value`. If `sequence` is a vector or string and `value` is the null value of that type, truncate the sequence right after the current element.

### (__null? value::Any)::Boolean

Check if `value` is a null value.

### (__type-of value::Any)::Type

Return the type of `value`. If `value` is a composite, return the value of the type slot.

### (__make-type)::Type

Return a new unique user-defined type value.

### (__make-instance type::Type value::Any function::Any)::Composite

Create a composite of type `type` with `value` in the value slot and `function` in the function slot.

### (__composite-value composite::Composite)::Any

Return the contents of the value slot of `composite`.

### (__composite-function composite::Composite)::Any

Return the contents of the function slot of `composite`.

### (__set-composite-value composite::Composite value::Any)::Any

Set the contents of the value slot in `composite` to `value.

### (__set-composite-function composite::Composite function::Any)::Any

Set the contents of the function slot in `composite` to `value.

### (__make-string sequence::(List Vector))::String

Create a string from a sequence of integers.

### (__concatenate left::(String Symbol) right::(String Symbol))::String

Append string or symbol `right` to the end of string or symbol `left`.

### (__substring string::String start-index::Integer end-index::Integer)::String

Return the subset of `string` starting from the index `start-index` and ending before the index `end-index`.

### (__length sequence::(List Vector String))::Integer

Return the length of the sequence.

### (__symbol-string symbol::Symbol)::String

Return the name of `symbol` as a string.

### (__symbol-id symbol::Symbol)::Integer

Return the unique ID of `symbol` as an integer.

### (__error message::String)

Throw a compilation error using `message` as the error message. Does not return.


## Compile-time global functions

### (gensym)::Symbol

Create and return a new unique symbol.

### (intern string::String)::Symbol

Create and return a new symbol with the name provided by `string`.

### (read source::String enable-inference::Boolean)::(Cons (Boolean Integer Float Symbol List String) Integer)

Parse and return the AST for the provided source code string. If `enable-inference` is `true` and duck-lisp has been compiled with parenthesis inference, then inference will be enabled when running the parser. No declarations exist in this instance of the inferrer except for `__declare`, `__infer-and-get-next-argument`, `__declare-identifier`, and `__declaration-scope`. `read` returns a cons. The CAR is the AST. If an error occurred, the CAR is nil. The CDR is the error number. It is set from a C variable of `dl_error_t`, so a value of 0 is no error.
