# DuckLib memory allocator

There were originally two purposes for the DuckLib memory allocator. The first was that it could allow duck-lisp to run on platforms without `malloc`. The second was that writing an allocator sounded fun. It was. It was also painful.

Another purpose revealed itself after I finished the allocator. It could be used as a fancy arena allocator. The heap that the allocator uses is one massive block of memory. If duck-lisp ever leaked memory (which it did all the time) then calling the stdlib `free` on that block of memory would immediately free all memory that duck-lisp used. While this is very convenient, it may increase the likelihood of use-after-free vulnerabilities since none of the pointers are set to `NULL` after the memory is freed.

I recommend not using the DuckLib allocator if you can help it. While it works, it is not very robust, and it is very, very slow.
