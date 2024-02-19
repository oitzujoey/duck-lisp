# Duck-lisp internals

## Acronyms and terms

VM — Virtual Machine, specifically the one that executes duck-lisp bytecode.  
AST — Abstract Syntax Tree  
HLA — High-Level Assembly, an intermediate representation.  
Assembly — HLA  
Inference, Parenthesis Inference — The module and algorithm that infers the structure of the code using type declarations.  
Form — A lisp form.  
Expression — A form that has the "expression" AST type. If it is wrapped in parentheses, then it is an expression.  

## Goals

* The language must be useful.
* The language must be embeddable.
* The compiler and VM must be able to run on nearly any computer architecture or operating system.
* The compiler and VM must remain independent of the standard library.
* The VM must be able to run on a different machine than the compiler.
* The bytecode binary size should be minimized.
* Extensibility should be maximized when practical.
* The C API must not be mind-bogglingly difficult to use.
* The language should include a demonstration of free-form parenthesis inference.

## Architecture

### Process view

Like many of the batch compilers that came before, duck-lisp has multiple stages of compilation.

1. Parse source text into an AST. The parser parses everything into the AST. Being a lisp, the AST produced by the
   parser is very similar to the parenthesized notation in the source code.
2. Run parenthesis inference. (optional) The inferrer counts arguments to functions to determine where function calls should occur and modifies the AST to match.
3. Compile AST into HLA. Macros are expanded during this stage.
4. Peephole optimization: Remove redundant stack operations. (optional) Simple push-pop combinations are removed. Pop instructions are combined.
5. Assemble HLA into bytecode. This mainly consists of serializing instruction arguments and choosing a suitable instruction size for the provided data.
6. Peephole optimization: Minimize the size of branch instructions. (optional) 8-bit or 16-bit relative addresses are used instead of 32-bit when possible.




### Module view

The compiler has several systems that have strict separation from the rest of the compiler.

    Compiler
      Parser
      Inferrer
      Generators
      Emitters
      Assembler
    VM

Some of this separation is natural, the parser and assembler are naturally distinct from the rest of the compiler, while others such as generators, emitters, and the inferrer are divided to improve extensibility or portability.

The Inferrer is separate from the parser and generators because it is designed to be relatively easy to remove and insert onto the backend of a foreign parser. How practical it is to transplant the inferrer remains to be seen.

Generators are compilers that are generally dedicated to a single keyword. They accept the compiler state and AST and return HLA. Emitters are compilers that are dedicated to a single class of bytecode instruction. The emitted instruction class and associated arguments are used by the assembler to generate the final bytecode. These two divisions provide two benefits. The first is that the mappings of generators to keywords and emitters to instruction classes is a reasonably nice abstraction. The second is that it makes the compiler more modular, providing users with the possibility to add their own generators that extend the language without modifying the compiler itself.

## Walkthrough of compilation process

Parenthesis inference is enabled for this exercise.

We start with this expression.

```lisp
(()
 __not 5)
```

First the parser is run (`duckLisp_read`). That results in this AST:

```lisp
(() __not 5)
```

Next, inference is run (`duckLisp_inferParentheses`). `__not` is declared in the compiler to have the type `(I)`, so one form is consumed as an argument.

```lisp
(() (__not 5))
```

Now the main compile stage begins (`duckLisp_compileAST`, which calls `duckLisp_compile_compoundExpression`). The outermost expression and the form in the function position are analyzed first. Since an expression is in the function position, it is determined that the outermost expression creates a new scope and `duckLisp_compile_expression` is called, which calls `duckLisp_generator_expression` because the form in the function position is still an expression. A new scope is created and `duckLisp_generator_noscope` is called. The forms inside the outermost expression are now compiled from left to right and `duckLisp_compile_compoundExpression` is called on each form. 

```lisp
()
```

The first form (still an expression) is passed to `duckLisp_compile_expression`, which again calls `duckLisp_generator_expression`. A new scope is pushed. `duckLisp_generator_noscope` is called. The expression has a length of zero, so `duckLisp_emit_nil` is called, which pushes the `duckLisp_instructionClass_nil` instruction class into the assembly array.

`duckLisp_generator_noscope` calls `duckLisp_emit_pop` to balance the stack. `duckLisp_instructionClass_pop` is pushed into the assembly array with an argument of `1`.

`duckLisp_generator_expression` pops the current scope.

```lisp
(__not 5)
```

The second form is also an expression, so it is passed to `duckLisp_compile_expression`, which calls `duckLisp_generator_expression`. The form in the function position is an identifier, so the name is looked up in the current environment. It is determined to be a generator (keyword) so the generator for `__not` (`duckLisp_generator_not`) is fetched and run. `duckLisp_generator_not` calls `duckLisp_compile_compoundExpression` on its single argument.

```lisp
5
```

`duckLisp_compile_compoundExpression` calls `duckLisp_emit_pushInteger` which pushes the instruction `duckLisp_instructionClass_pushInteger` and its argument `5` into the assembly array.

`duckLisp_generator_not` calls `duckLisp_emit_not` which pushes into the assembly array the instruction `duckLisp_instructionClass_not` and its argument, which is the index on the runtime stack where `5` was pushed to. In this case the index is `1`, which is the top of the stack.

`duckLisp_generator_expression` pops the current scope.

`duckLisp_compileAST` sometimes performs a final balance of the stack using "move" and "pop" instructions. In this case the stack is two deep. `5` and the result of `__not` would be on the runtime stack. `5` needs to be discarded but the result needs to be preserved. `duckLisp_emit_move` is called to move the result to the position that `5` holds. `duckLisp_instructionClass_move` is pushed into the assembly with arguments of `1` and `2`. `duckLisp_emit_pop` is called to remove the original result object from the stack. `duckLisp_emit_exit` is then called which pushes `duckLisp_instructionClass_halt` into the assembly array.

The main compilation stage is complete.

At this point there is no printable representation of the HLA form of the program, but if there were it would look like this:

```
nil
pop 1
push-integer 5
not 1
move 1 2
pop 1
halt
```

`duckLisp_compileAST` now calls `duckLisp_assemble`.

The first set of peephole optimizations is run. `nil` is followed by an immediate `pop 1`, meaning that this sequence does nothing. Delete it.

```
push-integer 5
not 1
move 1 2
pop 1
halt
```

This is a straightforward translation of the HLA to bytecode. The instruction type used for each instruction class depends upon the size of the arguments. In this case, all values fit into 8 bits, so the 8-bit version of each instruction is used.

```
integer.8 5
not.8 1
move.8 1 2
pop.8 1
halt
```

The second set of peephole optimizations is run which is to minimize the size of branch instructions. Since there are no branch instructions in this example, the above code is final. The compilation is finished.
