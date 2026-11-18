/** @file syscalls.c
 *
 * heap for newlib, bounded so exhaustion shows up as allocation failure
 * instead of silently overwriting the stack
 *
 * Copyright (c) 2026 IACE
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

extern char end;
extern char _estack;
extern char _Min_Stack_Size;

void *_sbrk(ptrdiff_t incr) {
    static char *brk = &end;
    char *limit = &_estack - (size_t)&_Min_Stack_Size;
    if (brk + incr > limit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    char *prev = brk;
    brk += incr;
    return prev;
}
