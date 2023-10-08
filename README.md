# duck-lisp

A simple scripting language for Hidey-Chess and similar projects.

Duck-lisp is your typical hobby lisp with one or two twists. I started this project before I knew what lisp was, choosing the syntax solely for parsability, so variable declaration has a bit of a JavaScript-like feel. Macros are also a little weird since the language is split into separate runtime and compile-time environments. Also, parentheses are optional.

## Features

* Optional free-form parenthesis inference that is mostly backwards compatible with S-expressions
* Optional compile-time arity checks (part of parenthesis inference)
* First class functions and lexical scope
* Common Lisp-like macros
* UTF-8 compatible
* C FFI
* Split compiler and VM
* Tested on x64 (Linux) and ARM (Linux)
* A simplified VM has been used on an ATmega328P, and the full VM runs on an LPC1769.

### Planned features

* Independent of the standard library
* Independent of OS

### Quirks

* Functions created by `defun` and `defmacro` are lexically scoped.
* Built-in keywords can be overridden using `var`, `defun` and `defmacro`.
* Variables are declared as they are in C-like languages. There is no `let`.
* Parenthesis inference does not work with lisp auto-formatters.
* Not quite a lisp-2.
* Recursion is performed using they keyword `self`.

### Misfeatures

* Error reporting is horrible. It will likely stay this way.
* There are no debug features other than a disassembler.
* Macros are unhygienic due to the inability of closures to be passed from the compilation VM to the runtime VM.
* The C FFI isn't great. Use at your own risk.

## Examples

### Building

You can ignore compile flags, but in case you want them, here are all of them:

To enable parenthesis inference and compile-time arity checks, configure the project with `cmake .. -DUSE_PARENTHESIS_INFERENCE=ON` instead of `cmake ..`.  
To build with shared libraries, set `-DBUILD_SHARED_LIBS=ON` as with the option above.  
To use DuckLib's memory allocator instead of the system's, set `-DUSE_DUCKLIB_MALLOC=ON`.  
It is intended to be possible to use duck-lisp without the standard library if necessary. Duck-lisp is not quite in a state where it will work without the standard library, but if you want to try compiling without support the option is `USE_STDLIB=OFF`.  
Advanced option: `NO_OPTIMIZE_JUMPS=ON`  
Advanced option: `NO_OPTIMIZE_PUSHPOPS=ON`  
If you need maximum performance out of the compiler, then `USE_DATALOGGING=ON` might be helpful. `duckLisp-dev` is setup to print the data collected when this flag is enabled.

```bash
git clone --recurse-submodules https://github.com/oitzujoey/duck-lisp
cd duck-lisp/scratchwork
mkdir build && cd build
cmake ..
cmake --build .
```

Examples and other junk can be found in the scratchwork directory. The large collection of scripts is mostly out of date. Only a few run without modification.

### Running

```bash
# Run duck-lisp language tests.
./duckLisp-test ../../tests
```

```bash
# Run the duck-lisp program "factorial.dl".
./duckLisp-dev ../scripts/factorial.dl
```

Note: Multiplication is defined in the VM, but this program was written pre-multiplication, so it reimplements it.

```bash
# Run a script with arguments.
./duckLisp-dev "(include \"../scripts/underout.dl\") (main 52)"
```

```bash
# Start the REPL.
./duckLisp-dev
```


## Usage

"language.md" contains a brief description of the language.  
"language-reference.md" contains a description of all duck-lisp keywords.  
"api.md" contains the C API documentation. (incomplete)  

"duckLisp.h" contains everything needed for normal usage of the compiler.  
"parser.h" contains declarations for the reader.  
"emitters.h" contains declarations for emitters for all special forms.  
"generators.h" contains declarations for generators for all special forms.  
"duckVM.h" contains everything needed usage of the VM.  

"duckLisp.c" contains the main compiler functions.  
"parser.c" contains the reader.  
"parenthesis-inferrer.c" contains the parenthesis inferrer and arity checker.  
"emitters.c" contains the emitters (functions that produce "high-level assembly").  
"generators.c" contains the generators (functions that convert AST forms to "high-level assembly" by calling emitters).  
"assembler.c" contains the assembler for the "high-level assembly" generated by the compiler.  
"compiler-debug.c" contains a single 7500 line function that disassembles bytecode.  
"duckVM.c" contains the VM.  

Typical usage of the language only requires including "duckLisp.h" and "duckVM.h". Adding new user-defined generators that generate bytecode requires "emitters.h". Some user-defined generators may want to call existing generators which can be called by including "generators.h". Reader function declarations can be found in "parser.h".

Examples on how to extend the language can be found in `scratchwork/duckLisp-dev.c`.

## Is it any good?

Yes.
