# duck-lisp

A simple scripting language for Hidey-Chess and similar projects.

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
