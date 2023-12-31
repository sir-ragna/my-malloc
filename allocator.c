#include <string.h>

// COMPILATION:
//     nasm -f elf64 brk.asm
//     cc allocator.c brk.o -Wall -Werror -o allocator.o

/* we can't use the brk glibc function as it returns 0, or -1 to indicate
 * success. We need to use the actual pure syscall.
 * defined in 'brk.asm' */
void* brk_syscall(void *);

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
char* ptrToStr(void* ptr) {
    ptrToStrResult[0] = '0';
    ptrToStrResult[1] = 'x'; // start with 0x

    unsigned long long bit64int = (unsigned long long)ptr;
    for (int i = 2; i < 18; i++) {
        /* take the last nibble and do a lookup */
        ptrToStrResult[i] = tohex[(unsigned char)((0xF000000000000000 & bit64int) >> 60)];
        bit64int <<= 4; // shift a nible
    }
    ptrToStrResult[19] = '\0';
    return ptrToStrResult;
}

/* Memory Marker
 * functions as a single linked list */
typedef struct MMarker {
    void* next_marker;
    char  occupied;
} MMarker;

void* heap_start = NULL;
void* heap_end = NULL; // also called the break

/* dead simple malloc implementation */
void* 
ds_malloc(unsigned long long requested) {
    if (heap_start == NULL) {
        /* get the current break */
        heap_start = brk_syscall(NULL);
        heap_end = heap_start;
    }
    
    if (requested == 0) {
        wstderr("zero bytes requested, returning NULL\n");
        return NULL;
    }

    if (heap_start == heap_end) {
        /* There is no marker yet
         *   allocate the first block
         *   allocated requested size + marker block
         **/
        void* prev_end = heap_end;
        heap_end = brk_syscall((void*)(((char*)heap_end) + sizeof(MMarker) + requested));

        /* Was the allocation a success? */
        if (prev_end == heap_end) {
            wstderr("error: brk call failed!\n");
            return NULL;
        }

        /* set the first memory marker */
        MMarker* mmarker = (MMarker*)heap_start;
        mmarker->next_marker = NULL;
        mmarker->occupied = 1;

        /* return the address after the first block */
        return (void*)((unsigned long long)heap_start + sizeof(MMarker));
    }

    /* look for an unoccupied block that is big enough */
    MMarker* mmarker = heap_start;
    while (mmarker->next_marker != NULL) {
        if (mmarker->occupied == 0) {
            /* unoccupied memory space, is it big enough? */
            if (((unsigned long long)mmarker->next_marker) - ((unsigned long long)mmarker) > requested + (2 * sizeof(MMarker))) {
                /* create a new marker at the end */
                MMarker* new_marker = (MMarker*)(((unsigned long long)mmarker) + sizeof(MMarker) + requested);
                /* set the new marker as free, while the old one as occupied */
                new_marker->occupied = 0;
                mmarker->occupied = 1;
                /* let the new marker point to the next marker
                 * while the old marker needs to point to the new marker */
                new_marker->next_marker = mmarker->next_marker;
                mmarker->next_marker = new_marker;

                /* return the address right behind the marker */
                return (void*)((unsigned long long)mmarker) + sizeof(MMarker);
            }
        }
        mmarker = mmarker->next_marker;
    }
    /* mmarker contains the last block. */
    if (mmarker->occupied == 0) {
        /* last block is not occupied, we can use it */
        mmarker->occupied = 1;
        /* resize the block to the exact size we need */
        heap_end = brk_syscall((void*)(((unsigned long long)mmarker) + sizeof(MMarker) + requested));
        return (void*)(((unsigned long long)mmarker) + sizeof(MMarker));
    }

    /* the last block is occupied, we need to allocated a new block at the end */
    void* new_end = brk_syscall((void*)(((unsigned long long)heap_end) + sizeof(MMarker) + requested));
    if (new_end == heap_end) {
        wstderr("failed to  ");
        wstderr(ptrToStr(mmarker));
        wstderr("\n"); 
    }
    /* set the marker at the previous heap_end */
    MMarker * new_mmarker = heap_end;
    new_mmarker->occupied = 1;
    new_mmarker->next_marker = NULL;
    /* let the previous mmarker point to the new block */
    mmarker->next_marker = new_mmarker;
    /* fix the heap_end */
    heap_end = new_end;

    return (void*) (((unsigned long long)new_mmarker) + sizeof(MMarker));
}

/* dead simple free */
void
ds_free(void* ptr) {
    if (ptr == NULL) {
        wstderr("cannot free NULL ptr\n");
        return;
    }
    /* mark the block as freed */
    MMarker* mmarker = (MMarker*)(((unsigned long long)ptr) - sizeof(MMarker));
    mmarker->occupied = 0;
    
    /* scan blocks for multiple unoccupied blocks */
    mmarker = (MMarker*)heap_start;
    while (mmarker->next_marker != NULL) {
        /* check if we have two markers next to each-other that are free */
        if (mmarker->occupied == 0 && ((MMarker*)mmarker->next_marker)->occupied == 0) {
            /* absorb the second marker into the first*/
            mmarker->next_marker = ((MMarker*)mmarker->next_marker)->next_marker;
        } else {
            mmarker = mmarker->next_marker;
        }
    }

    /* mmarker now contains the last block, if it is free shrink the heap
     * if it happens to also be the first block, remove the heap */
    if (mmarker->occupied == 0 && mmarker == heap_start) {
        //wstdout("removing heap completely\n");
        brk_syscall(heap_start);
        heap_start = NULL;
        heap_end = NULL;
    } else if (mmarker->occupied) {
        //wstdout("trimming heap\n");
        brk_syscall((void*) (((unsigned long long)mmarker) + sizeof(MMarker)));
    }
}

void
ds_print_heap_layout() {
    if (heap_start == heap_end) {
        wstdout("the heap is zero sized\n");
        return;
    }

    wstdout("the start of the heap: ");
    wstdout(ptrToStr(heap_start));
    wstdout("\n");

    MMarker *mmarker = heap_start;
    while (mmarker->next_marker != NULL) {
        wstdout("marker: ");
        wstdout(ptrToStr(mmarker));
        if (mmarker->occupied == 0) {
            wstdout(" FREE");
        } else {
            wstdout(" occupied");
        }
        wstdout("\n");
        mmarker = mmarker->next_marker;
    }
    wstdout("marker: ");
    wstdout(ptrToStr(mmarker));
    if (mmarker->occupied == 0) {
        wstdout(" FREE (last)");
    } else {
        wstdout(" occupied (last)");
    }
    wstdout("\n");
    wstdout("the end of the heap: ");
    wstdout(ptrToStr(heap_end));
    wstdout("\n");
}

int
main() {
    ds_print_heap_layout();
    char* text = "abcdefghijklmnopqrstuvwxyz";
    char* buffer = (char *)ds_malloc(512); // get me 512 bytes of memory
    if (buffer == NULL) {
        wstderr("allocation failed!");
        return 1;
    }
    memcpy(buffer, text, strlen(text));
    wstdout(buffer);
    wstdout("\n");
    ds_print_heap_layout();
    wstdout("----- lets allocates some pointers ---\n");
    void** ptrs = (void**)ds_malloc(1024 * sizeof(void*));
    wstdout("----- start allocating 1024 pointers ---\n");
    for (size_t i = 1; i < 1024; i++)
    {
        void * ptr = ds_malloc(i);
        ptrs[i - 1] = ptr;
    }
    ds_print_heap_layout();
    for (size_t i = 1; i < 1024; i++)
    {
        ds_free(ptrs[i - 1]);
    }
    ds_print_heap_layout();
    ds_free(ptrs);
    ds_free(buffer);
    ds_print_heap_layout();
    return 0;
}