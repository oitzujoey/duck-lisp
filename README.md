# duck-lisp

A simple scripting language for Hidey-Chess and similar projects.

Duck-lisp is very minimal. Only s-expressions and a few keywords will be provided. You are expected to create the rest of the language.

## Features

* Lexical scope
* Common Lisp-like macros
* UTF-8 compatible
* User created keywords
* C FFI
* Split compiler and VM
* Tested on x64 (Linux) and ARM (Linux)
* A simplified VM has been used on an ATmega328P.

### Planned features

* Independent of the standard library
* Independent of OS
* Independent of processor architecture

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
# Fuzz the memory allocator. Perform 10 tests.
./memory-dev 10
```

```bash
# Populate a trie from an array and print it.
./trie-dev
```

```bash
# Benchmark some sorts.
./sort-test
```

```bash
# Run the duck-lisp program "factorial.dl".
./duckLisp-dev ../scripts/factorial.dl
```

Note: Multiplication is defined in the VM, but this program was written pre-multiplication, so it reimplements it.

```bash
# Run a script with arguments.
./duckLisp-dev "(include ../scripts/underout.dl) (main 52)"
```

```bash
# Start the REPL.
./duckLisp-dev
```


## Usage

Extending the language is done by adding generators and callbacks to the compiler and VM. Generators are functions that convert the AST to bytecode during compilation. Callbacks are functions that are called at runtime to perform I/O and tasks that would be slow or difficult in the VM.

`duckLisp.c`: This contains the compiler.  
`duckVM.c`: This contains the VM.

Examples on how to extend the language can be found in `duckLisp-dev.c`.

## Is it any good?

Um…
