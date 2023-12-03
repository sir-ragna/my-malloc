#include <string.h>
// /* Not going to rely on imports for these */
// #define NULL 0
// #define stdin 0
// #define stdout 1
// #define stderr 2

// COMPILATION:
//     nasm -f elf64 brk.asm
//     cc allocator.c brk.o -Wall -Werror -o allocator.o

/* we can't use the brk glibc function as it returns 0, or -1 to indicate
 * success. We need to use the actual pure syscall.
 * defined in 'brk.asm' */
void* brk_syscall(void *);

/* to avoid that glibc starts to allocate memory with brk itself 
 * using glibc functions should be avoided. In order to troubleshoot
 * and test my own allocator, I'm going to implement some of my own 
 * version to print some strings */

/* strlen variant */
// int strlen(char *str) {
//     int result = 0;
//     while (*str != '\0') {
//         str++;
//         result++;
//     }
//     return result;
// }

/* defined in 'write.asm'. Uses the write syscall */
int write_out(int stream, char* str, int len);

/* write to stdout */
int wstdout(char *str) {
    int len = strlen(str);
    return write_out(1, str, len);
}

/* write to stderr */
int wstderr(char *str) {
    int len = strlen(str);
    return write_out(2, str, len);
}

char tohex[] = {'0', '1', '2', '3', '4', '5', '6', '7', 
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
char ptrToStrResult[512] = { 0 };
/* stores the result in ptrToStrResult 
 * This is to avoid dynamic memory allocation */
void ptrToStr(void* ptr) {
    ptrToStrResult[0] = '0';
    ptrToStrResult[1] = 'x'; // start with 0x

    unsigned long long bit64int = (unsigned long long)ptr;
    for (int i = 2; i < 18; i++) {
        /* take the last nibble and do a lookup */
        ptrToStrResult[i] = tohex[(unsigned char)((0xF000000000000000 & bit64int) >> 60)];
        bit64int <<= 4; // shift a nible
    }
    ptrToStrResult[19] = '\0';
}

/* end of glibc-like functions */

/* Memory Marker
 * functions as a single linked list */
typedef struct MMarker {
    void* next_marker;
    char  occupied;
} MMarker;

void* heap_start = NULL;
void* heap_end = NULL; // also called the break

void 
init_allocator() {
    /* get the current break */
    heap_start = brk_syscall(NULL);
    heap_end = heap_start;

    /* debugging info */
    wstdout("initializing allocator at: ");
    ptrToStr(heap_start);
    wstdout(ptrToStrResult);
    wstdout("\n");
}

/* dead simple malloc implementation */
void* 
ds_malloc(int requested) {
    if (heap_start == NULL)
        init_allocator();
    
    if (requested == 0) {
        wstderr("zero bytes requested, returning NULL\n");
        return NULL;
    }

    void* prev_end = heap_end;
    MMarker mmarker = {0};
    if (heap_start == heap_end) {
        /* There is no marker yet
         *     Allocate requested + marker
         *     Set marker
         **/
        // printf("current break %p, requested %p\n", heap_end, (void*)(((char*)heap_end) + sizeof(MMarker) + requested));
        heap_end = brk_syscall((void*)(((char*)heap_end) + sizeof(MMarker) + requested));

        /* Was the allocation a success? */
        if (prev_end == heap_end) {
            wstderr("error: brk call failed!\n");
            return NULL;
        }

        /* set memory marker */
        mmarker.next_marker = NULL;
        mmarker.occupied = 1;
        memcpy(prev_end, &mmarker, sizeof(MMarker));

        /* return the start of the new block */
        return prev_end + sizeof(MMarker);
    }

    return NULL;
}

int
main() {
    char* text = "abcdefghijklmnopqrstuvwxyz";
    char* buffer = (char *)ds_malloc(512); // get me 512 bytes of memory
    if (buffer == NULL) {
        wstderr("allocation failed!");
        return 1;
    }
    memcpy(buffer, text, strlen(text));
    wstdout(buffer);
    wstdout("\n");
    return 0;
}