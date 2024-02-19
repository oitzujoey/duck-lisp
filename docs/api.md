# C API

## Internal model

C callbacks accept arguments on the stack and push return values on the stack. Example: `print` prints the object on the top of the stack.
Generators accept arguments from bytecode and push return values on the stack. Example: `add` accepts two pointers, fetches the objects at those locations, adds them, and pushes the result on the stack.

## Basic usage

First follow the instructions in the readme for compiling and running duck-lisp.

Figure out what platform the compiler and VM will run on. Duck-lisp was intended to run pretty much anywhere, even on platforms without the C standard library, so much of the standard library has been reimplemented, for better or for worse. This means that you will have to adjust some architecture-dependent settings to run duck-lisp optimally. Open "DuckLib/core.h". Change the `typedef`s `dl_bool_t`, `dl_size_t`, `dl_ptrdiff_t`, `dl_uint8_t`, `dl_uint16_t`, `dl_uint32_t`, `dl_uint64_t`, `dl_int8_t`, `dl_int16_t`, and `dl_int32_t` to match the size and types of their equivalents in the C standard library. Also adjust `DL_ALIGNMENT` to match the alignment of the architecture. It is set to 8 bytes by default, which should be fine (though not optimal) for most desktop architectures. Now choose whether to use C's `malloc` or DuckLib's `dl_malloc`. As of right now, there is little reason to use `dl_malloc` since it's so slow. DuckLib's allocator is enabled by default, so if you want fast allocation you will have to disable it. Open "DuckLib/memory.h". Near the very bottom is the line `#if 1`.  Change the 1 to a 0. Open "DuckLib/memory.c". Search for `#if 1`. Replace the 1 with 0. Rebuild and you should notice large programs run much faster.

```c
/* The compiler */
#include "duckLisp.h"
/* The VM */
#include "duckVM.h"
```

At the start of `main`, declare a variable to keep returned error codes in.

```c
// Many functions DuckLib error codes.
dl_error_t e = dl_error_ok;
```

The language is split across two files. As noted above, the compiler is in one file and the virtual machine is in the other. This makes it easier to compile source code ahead of time and run it later, potentially on an entirely different CPU architecture. We will follow that process here as well. First we will compile duck-lisp source code to bytecode, then we will pass it to the VM and execute it.

If you insist on using the DuckLib allocator, then you have to set it up. There actually is a good reason to use it in some cases. Duck-lisp does sometimes leak memory on an error. When this happens, the only way to clean it up without exiting `main` is to run `dl_memory_quit` on the allocator instance. This requires bringing down the compiler and VM as well, but in some cases those can be killed and restarted without a problem.

```c
// You only need this if using the DuckLib allocator!

// Allocate a big hunk of memory to use as the heap.
const size_t duckLisp_memory_size = 10000000;
void *duckLisp_memory = malloc(duckLisp_memory_size);
if (duckLisp_memory == NULL) {
    printf("malloc failed.\n");
    // You can use your own error handling mechanism here, but this is my code, so we're using `dl_error_t`. ;-)
    e = dl_error_invalidValue;
    goto cleanup;
}
dl_memoryAllocation_t duckLispMemoryAllocation;
// `dl_memoryFit_best` means to find a chunck of memory that most closely fits the specified size.
e = dl_memory_init(&duckLispMemoryAllocation, duckLisp_memory, duckLisp_memory_size, dl_memoryFit_best);
if (e) {
    printf("Failed to initialize memory allocator\n");
    goto cleanup;
}
```

Now we can initialize the compiler.

```c
duckLisp_t duckLisp;

// Pass `NULL` as the second argument if not using DuckLib allocator.
// Limit the maximum number of objects allocated in the compile-time VM to 1000000.
e = duckLisp_init(&duckLisp, &duckLispMemoryAllocation, 1000000);
if (e) {
    printf("Failed to initialize duck-lisp\n");
    goto cleanup;
}
```

Run the compiler.

```c
// Duck-lisp doesn't have any I/O right now, so just do some fancy calculations and return the result.
char source[] = "(__* 3 (__+ 7 5))";

uint8_t *bytecode = NULL;
size_t bytecode_length = 0;

dl_error_t loadError = duckLisp_loadString(duckLisp, &bytecode, &bytecode_length, source, strlen(source));
if (loadError) {
    printf("Compilation failed.\n");
    // Print compilation errors.
    dl_array_t *errors = duckLisp.errors;
    while (errors.elements_length > 0) {
        duckLisp_error_t error;
        e = dl_array_popElement(errors, (void *) &error);
        if (e) goto cleanup;
        // Duck-lisp rarely uses null-terminated strings. Strings are nearly always stored with an associated length.
        for (ptrdiff_t j = 0; j < error.message_length; j++) {
            putchar(error.message[j]);
        }
        putchar('\n');
    }
    e = dl_error_invalidValue;
    goto cleanup;
}
```

Assuming compilation succeeded, you will have a bit of bytecode ready to be executed. If you so desire, you may now run `duckLisp_quit` to destroy the compiler. It is not needed during bytecode execution.

Create the virtual machine.

```c
duckVM_t duckVM;

// Limit the maximum number of objects allocated in the VM to 1000000.
const size_t objectHeap_size = 1000000;

// Pass `NULL` as the second argument if not using DuckLib allocator.
dl_error_t e = duckVM_init(&duckVM, &duckLispMemoryAllocation, objectHeap_size);
if (e) {
    printf("Failed to initialize duck-lisp\n");
    goto cleanup;
}
```

And finally, run the bytecode.

```c
duckVM_object_t returnValue;
dl_error_t runtimeError = duckVM_execute(&duckVM, &returnValue, bytecode, bytecode_length);
if (runtimeError) {
    printf("VM execution failed.\n");
    dl_array_t *errors = duckVM.errors;
    while (errors.elements_length > 0) {
        duckLisp_error_t error;
        e = dl_array_popElement(errors, (void *) &error);
        if (e) goto cleanup;
        for (ptrdiff_t j = 0; j < error.message_length; j++) {
            putchar(error.message[j]);
        }
        putchar('\n');
    }
    e = dl_error_invalidValue;
    goto cleanup;
}

if (returnValue.type != duckVM_object_type_integer) {
    printf("Returned object is not an integer.\n");
    e = dl_error_invalidValue;
    goto cleanup;
}

// Adjust format string for your platform. The result should be 36.
printf("VM: %li", returnValue.value.integer);
```

Now clean everything up.

```c
cleanup:

(void) duckVM_quit(&duckVM);
(void) duckLisp_quit(&duckLisp);

// These two lines are only required if using the DuckLib allocator.
(void) dl_memory_quit(&duckLispMemoryAllocation);
(void) free(duckLisp_memory);

return e;
```

## Callbacks

The language is running, but there's no I/O other than the source code and the return value. Let's define a C function that can be called from duck-lisp.

```c
dl_error_t callback_helloWorld(duckVM_t *duckVM) {
    dl_error_t e = dl_error_ok;

    puts("Hello, world!");

    e = duckVM_pushNil(duckVM);

    return e;
}
```

Both the compiler and the VM need to be aware of this function. Place this code before `duckLisp_loadString` is called. Once this is done, calls to `hello-world` can be compiled. It can also be called at compile-time. In fact, if all duck-lisp code is placed inside a `__comptime` form, there is no need to create a VM because the compiler will evaluate the code at compile-time.

```c
char *callbackName = "hello-world";
size_t callbackName_length = strlen(callbackName);
e = duckLisp_linkCFunction(&duckLisp, callback_helloWorld, callbackName_length);
if (e) {
    printf("Failed to register C callback with compiler.\n");
    goto cleanup;
}
```

Now pass the callback to the VM. Place this code before `duckVM_execute` is called. The VM doesn't know the name of the callback. All it knows about is an integer ID and the callback itself. The compiler *does* know the name of the callback, so we have to ask it for the callback's unique ID that it was assigned and pass that to the VM.

```c
ptrdiff_t callback_id = duckLisp_symbol_nameToValue(&duckLisp, callbackName, callbackName_length);
if (callback_id == -1) {
    // I admit it. I didn't parameterize the callback's name because DuckLib's strings are hard to work with.
    printf("\"hello-world\" could not be found in the compiler's global environment.\n");
    e = dl_error_invalidValue;
    goto cleanup;
}
e = duckVM_linkCFunction(&duckLisp, callback_id, callback_helloWorld);
if (e) {
    printf("Failed to register C callback with VM.\n");
    goto cleanup;
}
```

The final step is to change the duck-lisp source code string to this:

```c
char source[] = "(hello-world)";
```

And that's everything there is to know about duck-lisp!

Actually it's just too convoluted to write here.

I need to fix that.
