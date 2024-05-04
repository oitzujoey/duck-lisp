# FAQ

## Why use a bytecode virtual machine?

Bytecode VMs are known to be fast and portable. JIT is faster, and reasonably portable with LLVM, but then it wouldn't run on platforms like AVR microcontrollers. I also don't want to have to keep up to date with the frequent LLVM releases.

## Why did you call it "duck-lisp"?

Why, do you have something against ducks?

## How are macros implemented?

https://www.origamiparade.com/programming-projects/programming-languages/lisp/implementing-macros/

## Is it memory safe?

No. Operation outside of how I normally use it is likely to cause a segfault or data corruption.
