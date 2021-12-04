# duck-lisp

A simple scripting language for Hidey-Chess and similar projects.

Duck-lisp is very minimal. Only s-expressions and a few keywords will be provided. You are expected to create the rest of the language.

## Building

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

## Examples

Examples can be found in the scratchwork directory.

```bash
# Fuzz the memory allocator. Perform 10 tests.
./memory-dev 10
```

```bash
# Create a dictionary from an array and print it.
./trie-dev
```

```bash
# Run duck-lisp.
./duckLisp-dev.c
```

## Usage

Extending the language is done by adding generators and callbacks to the language. Generators are functions that convert the AST to bytecode during compilation. Callbacks are functions that are called at runtime to perform I/O and tasks that would be slower or difficult in the VM.

`duckLisp.c`: This contains the compiler.  
`duckVM.c`: This contains the VM.
