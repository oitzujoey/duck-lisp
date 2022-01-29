# duck-lisp

A simple scripting language for Hidey-Chess and similar projects.

Duck-lisp is very minimal. Only s-expressions and a few keywords will be provided. You are expected to create the rest of the language.

## Features

* Separate compiler and VM
* Lexical scoping
* User created keywords
* C FFI
* Low memory VM

### Planned features

* Independent of the standard library
* Independent of OS
* Independent of processor architecture

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
# Run the duck-lisp program "0.dl".
./duckLisp-dev ../0.dl
```

## Usage

Extending the language is done by adding generators and callbacks to the language. Generators are functions that convert the AST to bytecode during compilation. Callbacks are functions that are called at runtime to perform I/O and tasks that would be slower or difficult in the VM.

`duckLisp.c`: This contains the compiler.  
`duckVM.c`: This contains the VM.

Examples on how to extend the language can be found in `duckLisp-dev.c`.
