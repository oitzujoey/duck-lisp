# C API

## Acronyms

DL: Duck-lisp
AST: Abstract syntax tree
HLA: High-level assembly (a certain data structure that exists only in duck-lisp)
VM: Virtual machine, specifically DL's bytecode interpreter.

## Model

Duck-lisp is a library that contains a bytecode compiler and a bytecode interpreter. If you downloaded the repository, it is likely that the first thing you tried was to run the executable called "duckLisp-dev". That executable is a wrapper around duck-lisp. When you pass it a file name or type a command into it, it takes that program text and compiles it, resulting in bytecode. It then takes that bytecode and passes it to the bytecode VM which executes it.

Duck-lisp is designed with extensibility as one of its primary goals. It is easy to use for simple use cases, but there are many hooks into the language in case you need to do something more complicated. A large portion of this document is dedicated to that extensibility. Unfortunately, writing extensions commonly requires very good knowledge of internal data structures.

### Compiler model

1. Initialize the compiler.
2. Load user-defined parser actions into the compiler. This step is optional.
3. Load user-defined generators into the compiler. This step is optional.
4. Load user-defined C functions into the compiler. This step is optional.
5. Compile text to bytecode.
6. Check for and handle compile errors.
7. Destroy the compiler.

The compilation step is easy. Pass source code to the `duckLisp_loadString` function and bytecode pops out. If an error occured, then print the error messages.

Now for the complicated part. There are three ways to extend the compiler.

Parser actions are functions that are run when parsing duck-lisp code. If the first parenthesized expression matches the parser action's name, then the function is run. The arguments are `duckLisp_t` (the compiler's context) and `duckLisp_ast_compoundExpression_t` (the current AST node that matches the action's name). It might be possible to safely modify the passed AST, but I don't make any guarantee that it won't break something. Modify the `duckLisp_t` argument as you see fit.

Generators are run during compilation of the AST to HLA. In fact, generators _are_ the compiler. Your generators are injected into the compiler alongside the native generators and are treated exactly the same as other generators. If you need to carry data around with the main compiler context, you can store it in the `userData` field of `duckLisp_t`. Your generator is run when the AST node's function name matches the name of the generator. Generators are passed the `duckLisp_t` compiler context, the `duckLisp_compileState_t` scope data structure, the HLA array, and the current `duckLisp_ast_compoundExpression_t` AST node. You may modify the AST node (and all other arguments), but you may leak memory or cause a double free if you aren't careful.

User-defined C functions that you intend to call from duck-lisp code must be loaded into the compiler before compilation takes place. This is so that the compiler knows the C functions being called are defined, and so that compile-time duck-lisp code is able to call these functions during compilation. C functions are discussed in depth in the "VM model" section. Of the three extension mechanisms in the compiler, C functions are by far the easiest to use.

### VM model

1. Initialize the VM.
2. Load user-defined C functions into the compiler. This step is optional.
3. Execute previously compiled bytecode.
4. Check for and handle runtime errors.
5. Destroy the VM.

The VM API is modeled after Lua's, but is likely more rough since it hasn't gone through as many design iterations. Duck-lisp's VM API has been through about three iterations, and remnants of the past APIs are still in use. Internally, the VM contains a structure called `duckVM_object_t`. Unless you are attempting to do something weird, I do not recommend ever touching this data structure. Instead of operating on DL objects directly, you must manipulate them while they are on the stack using the provided set of stack operations. While this model is not as pleasant to use, it prevents the VM from garbage collecting objects that you hold references to. And that is bad because it will happen randomly several weeks after you write this code, and then you will have to go back and fix it by keeping all your objects on the stack (where they should have been in the first place) so that the garbage collector knows they are in use.

Like Lua, objects live either on the stack or on the heap. Also like Lua, these objects are garbage collected, and in two different ways. Objects on the stack are garbage collected by executing a "pop" bytecode instruction. This is essentially a message from the compiler through the bytecode that this object on the stack is no longer needed and should be discarded. Objects on the heap are garbage collected using a stop-the-world tracing garbage collector. This garbage collector (from here on referred to as "the garbage collector" or "the collector") pauses the VM, iterates over all objects on the stack, and follows the trail of pointers from there to every object used by the VM. It marks every one of these objects (that is not on the stack) as "live", and then places all objects that are not live in a list to be recycled. The garbage collector doesn't care about how magnificent the data structure you created in your C callback is. If it's not on the stack it **will** be collected.

The best way to think about the VM is like Forth. You can push stuff on the stack. You can pop stuff off. You can copy stuff off the stack and into the heap. You can copy stuff out of the heap and onto the stack. In most callbacks, the interesting operations happen outside of the VM. You parse some arguments on the stack and then pop them off. You do an interesting operation. You push some objects back on the stack and build them into a single data structure.




Generators accept arguments from bytecode and push return values on the stack. Example: `add` accepts two pointers, fetches the objects at those locations, adds them, and pushes the result on the stack.

## Basic usage

First follow the instructions in the readme for compiling and running duck-lisp.

Figure out what platform the compiler and VM will run on. Duck-lisp was intended to run pretty much anywhere, even on platforms without the C standard library, so much of the standard library has been reimplemented, for better or for worse. This means that you will have to adjust some architecture-dependent settings to run duck-lisp optimally. Open "DuckLib/core.h". Change the `typedef`s `dl_bool_t`, `dl_size_t`, `dl_ptrdiff_t`, `dl_uint8_t`, and `dl_uint64_t` to match the size and types of their equivalents in the C standard library. Also adjust `DL_ALIGNMENT` to match the alignment of the architecture. It is set to 8 bytes by default, which should be fine (though not necessarily optimal) for most desktop architectures. Now choose whether to use C's `malloc` or DuckLib's `dl_malloc`, and whether to use standard library functions or DuckLib's replacements. As of right now, there is little reason to use `dl_malloc` since it is so slow. DuckLib's allocator is disabled by default but can be enabled by passing `-DUSE_DUCKLIB_MALLOC=ON` to CMake during configuration. I also suggest enabling the standard library to maximize speed. DuckLib uses the standard library by default but this setting may be disabled by passing `-DUSE_STDLIB=OFF` to CMake. In this example I will be using the parenthesis inferrer, so I suggest enabling parenthesis inference by passing `-DUSE_PARENTHESIS_INFERENCE=ON` to CMake during configuration.

Create a new file. Start the file by including some standard library headers and the compiler and VM headers.

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* The compiler */
#include "DuckLib/core.h"
#include "duckLisp.h"
/* The VM */
#include "duckVM.h"
```

Next define a helper function for error handling. All this function does is fetch all error messages from the compiler or VM and print them.

```c
// Print compilation errors.
dl_error_t serialize_errors(char *message,
                            dl_memoryAllocation_t *ma,
                            dl_array_t *errors,
                            dl_uint8_t *source,
                            size_t source_length){
	dl_error_t e = dl_error_ok;

	printf("%s\n", message);

	dl_uint8_t *errorString = NULL;
	dl_size_t errorString_length = 0;
	e = duckLisp_serialize_errors(ma,
	                              &errorString,
	                              &errorString_length,
	                              errors,
	                              source,
	                              source_length);
	if (e) {
		printf("Error serialization failed.\n");
		goto cleanup;
	}

	for (size_t i = 0; i < errorString_length; i++) {
		putchar(errorString[i]);
	}
	putchar('\n');

	errorString_length = 0;
	free(errorString);
	errorString = NULL;

	e = dl_error_invalidValue;

 cleanup: return e;
}
```

At the start of `main`, declare a variable to keep returned error codes in.

```c
	// Many functions return DuckLib error codes.
	dl_error_t e = dl_error_ok;
```

The language is split across two files. As noted above, the compiler is in one file and the virtual machine is in the other. This makes it easier to compile source code ahead of time and run it later, potentially on entirely different CPU architectures. We will follow that process here as well. First we will compile duck-lisp code to bytecode, then we will pass it to the VM and execute it.

If you insist on using the DuckLib allocator, then you have to set it up. There actually is a good reason to use it in some circumstances. Duck-lisp sometimes leaks memory on an error. When this happens the only way to clean it up without exiting `main` is to run `dl_memory_quit` on the allocator instance. This requires bringing down the compiler and VM as well, but in some cases those can be killed and restarted without a problem.

```c
	// You only need this if using the DuckLib allocator!

	// Allocate a big hunk of memory to use as the heap.
	const size_t duckLisp_memory_size = 10000000;
	void *duckLisp_memory = malloc(duckLisp_memory_size);
	if (duckLisp_memory == NULL) {
		printf("malloc failed.\n");
		// You can use your own error handling mechanism here, but this is my
		// code, so we're using `dl_error_t`. ;-)
		e = dl_error_invalidValue;
		goto cleanup;
	}
	dl_memoryAllocation_t duckLispMemoryAllocation;
	// `dl_memoryFit_best` means to find a chunk of memory that most closely
	// fits the specified size.
	e = dl_memory_init(&duckLispMemoryAllocation,
	                   duckLisp_memory,
	                   duckLisp_memory_size,
	                   dl_memoryFit_best);
	if (e) {
		printf("Failed to initialize memory allocator\n");
		goto cleanup;
	}
```

Now we can initialize the compiler.

```c
	duckLisp_t duckLisp;

	// Pass `NULL` as the second argument if not using DuckLib allocator.
	// Limit the maximum number of objects allocated in the compile-time VM to
	// 100.
	// Limit the maximum number of objects allocated in the inference-time VM to
	// 100. This last parameter only exists if you built with inference enabled.
	e = duckLisp_init(&duckLisp, &duckLispMemoryAllocation, 100, 100);
	if (e) {
		e = serialize_errors("Failed to initialize duck-lisp.",
		                     duckLisp.memoryAllocation,
		                     &duckLisp.errors,
		                     NULL,
		                     0);
		goto cleanup;
	}
```

Run the compiler.

```c
	// Duck-lisp doesn't have any I/O right now, so just do some calculations
	// and return the result.
	dl_uint8_t source[] = "(* 3 + 7 5)";
	size_t source_length = sizeof(source)/sizeof(*source) - 1;

	uint8_t *bytecode = NULL;
	size_t bytecode_length = 0;

	dl_error_t loadError = duckLisp_loadString(&duckLisp,
	                                           // Parenthesis inference is
	                                           // enabled:
	                                           true,
	                                           &bytecode,
	                                           &bytecode_length,
	                                           source,
	                                           source_length,
	                                           DL_STR("No file"));
	if (loadError) {
		e = serialize_errors("Compilation failed.",
		                     duckLisp.memoryAllocation,
		                     &duckLisp.errors,
		                     source,
		                     source_length);
		goto cleanup;
	}
```

The macro `DL_STR` is used above to simplify passing a string literal to a function. The macro expands to the string literal and the string literal's length (minus the null terminator) separated by a comma. This consumes two parameter slots. "No file" is passed as the file name argument and the length, 7, is passed as the length.

Assuming compilation succeeds, you will have a bit of bytecode ready to be executed. If you so desire, you may now run `duckLisp_quit` to destroy the compiler. It is not needed during bytecode execution.

Create the virtual machine.

```c
	duckVM_t duckVM;

	// Limit the maximum number of objects allocated in the VM to 10.
	const size_t objectHeap_size = 10;

	// Pass `NULL` as the second argument if not using DuckLib allocator.
	e = duckVM_init(&duckVM, &duckLispMemoryAllocation, objectHeap_size);
	if (e) {
		printf("Failed to initialize duck-lisp\n");
		goto cleanup;
	}
```

And finally, run the bytecode.

```c
	duckVM_object_t returnValue;
	dl_error_t runtimeError = duckVM_execute(&duckVM,
	                                         &returnValue,
	                                         bytecode,
	                                         bytecode_length);
	if (runtimeError) {
		e = serialize_errors("VM execution failed.",
		                     duckVM.memoryAllocation,
		                     &duckVM.errors,
		                     NULL,
		                     0);
		goto cleanup;
	}

	if (returnValue.type != duckVM_object_type_integer) {
		printf("Returned object is not an integer.\n");
		e = dl_error_invalidValue;
		goto cleanup;
	}

	// Adjust format string for your platform. The result should be 36.
	printf("VM: %li\n", returnValue.value.integer);
```

Now clean everything up.

```c
 cleanup:
	(void) duckVM_quit(&duckVM);
	(void) duckLisp_quit(&duckLisp);

	// These two lines are only required if using the DuckLib allocator.
	(void) dl_memory_quit(&duckLispMemoryAllocation);
	(void) free(duckLisp_memory);

	return e != 0;
```

## Callbacks

The language is running, but there's no I/O other than the source code and the return value. Let's define a C function that can be called from duck-lisp.

A C callback pops arguments off the stack and pushes a single return value on the stack. Every function must return a value. When in doubt what to return, return nil.

To start with, let's write the simplest useful function possible.

```c
dl_error_t callback_helloWorld(duckVM_t *duckVM) {
	puts("Hello, world!");
	return duckVM_pushNil(duckVM);
}
```

Both the compiler and the VM need to be aware of this function. Place the code below before `duckLisp_loadString` is called. Calls to `hello-world` can be compiled once this is done. `hello-world` can also be called at compile-time. Speaking of compile time code, if all duck-lisp code is placed inside a `comptime` form, then there is no need to create a VM since the compiler will evaulate all the code at compile-time.

```c
	dl_uint8_t callbackName[] = "hello-world";
	dl_size_t callbackName_length = (sizeof(callbackName)/sizeof(*callbackName)
	                                 - 1);
	e = duckLisp_linkCFunction(&duckLisp,
	                           callback_helloWorld,
	                           DL_STR("hello-world"),
	                           // `hello-world` doesn't accept arguments.
	                           // These two arguments are not required if
	                           // parenthesis inference is disabled.
	                           DL_STR("()"));
	if (e) {
		printf("Failed to register C callback with compiler.\n");
		goto cleanup;
	}
```

Now pass the callback to the VM. Place this code before `duckVM_execute` is called. The VM doesn't know the name of the callback. All it knows about is an integer ID and the callback itself. The compiler *does* know the name of the callback, so we have to ask it for the callback's unique ID that it was assigned and pass that to the VM.

```c
	ptrdiff_t callback_id = duckLisp_symbol_nameToValue(&duckLisp,
	                                                    callbackName,
	                                                    callbackName_length);
	if (callback_id == -1) {
		printf("\"%s\" could not be found in the compiler's global environment.\n",
		       callbackName);
		e = dl_error_invalidValue;
		goto cleanup;
	}
	e = duckVM_linkCFunction(&duckVM, callback_id, callback_helloWorld);
	if (e) {
		printf("Failed to register C callback with VM.\n");
		goto cleanup;
	}
```

The final step is to change the duck-lisp source code string to this:

```c
	dl_uint8_t source[] = "(hello-world)";
```

Now let's try a variant that's a little more useful. We will pop a string off the stack, print it, then return the length of the string.,

And that's everything there is to know about duck-lisp!

Actually it's just too convoluted to write here.

I need to fix that.


## Conventions

An error is nearly always indicated with a return value of the type `dl_error_t`. If the return type of a function is `void`, then the function should always succeed.

Strings may or may not be null terminated. Don't ever depend on them being null terminated. A length value is always passed around with the string instead. It is acceptable to pass a null-terminated string to duck-lisp, but be sure the length you pass does not include the null terminator. The literal string "abc" has a length of three, not four.