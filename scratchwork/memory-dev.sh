#!/bin/sh

cd build
make
cd ..

for I in {1..10}
do
	valgrind --leak-check=yes build/memory-dev &> memory-dev-fuzz/memory-dev-fuzz$I.log
done
