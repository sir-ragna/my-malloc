
# Write your own allocator

Someone sent me a link of a blogpost about writting your own allocator.
https://mliezun.github.io/2020/04/11/custom-malloc.html 

I didn't like the that he had to rely on using `malloc` once in the beginning.
I took it as a challenge to not use `malloc` but the `brk` system call.

Modern Linux relies mainly on `mmap` instead of `brk` for memory allocation.

Using `brk` directly and `malloc` or other libc functions that rely on 
malloc is a recipe for disaster. This is why you don't see any `printf` 
usage in this software but I'm also using the `write` systemcall for output.



