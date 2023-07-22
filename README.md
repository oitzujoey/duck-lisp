# duck-lisp

A simple scripting language for Hidey-Chess and similar projects.

Duck-lisp is very minimal. Only s-expressions and a few keywords will be provided. You are expected to create the rest of the language.

## Features

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

### Misfeatures

* Error reporting is horrible. It will likely stay this way.
* There are no debug features other than a disassembler.

## Examples

### Building

```bash
git clone https://github.com/oitzujoey/duck-lisp
cd duck-lisp/DuckLib
git submodule init
git submodule update
cd ../scratchwork
mkdir build && cd build
cmake ..
cmake --build .
```

To build with shared libraries, configure the project with `cmake .. -DBUILD_SHARED_LIBS=ON` instead of `cmake ..`.

Examples can be found in the scratchwork directory.

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

"duckLisp.c" contains the compiler.  
"duckVM.c" contains the VM.  

Examples on how to extend the language can be found in `duckLisp-dev.c`.

## Is it any good?

Sure…
