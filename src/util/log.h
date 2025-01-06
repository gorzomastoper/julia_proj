#pragma once

#include <stdio.h>
#include <stdlib.h>

inline
void _panic [[ noreturn ]] (const char *msg, const char *file, int line, const char *fn) {
    fprintf(stderr, "panic at: %s:%d in %s\n\t%s\n", file, line, fn, msg);
    exit(1);
}

#define panic(e) _panic(e, __FILE__, __LINE__, __func__)

inline
void _warn (const char *msg, const char *file, int line, const char *fn) {
    fprintf(stderr, "panic at: %s:%d in %s\n\t%s\n", file, line, fn, msg);
    // system("pause");
}

#define warn(e) _warn(e, __FILE__, __LINE__, __func__)
