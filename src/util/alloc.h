#pragma once

#include "types.h"
#include <cstdlib>

struct allocator {
    void*(*alloc)(usize);
    void*(*realloc)(void*, usize);
    void(*free)(void*);
};

static constexpr allocator default_allocator = {
    .alloc   = [](usize size) -> void* {
        let ptr = malloc(size);
        // printf("alloc %p %llu\n", ptr, size);
        // system("pause");
        return ptr;
        // return malloc(size);
    },
    .realloc = [](void* ptr, usize size) -> void* {
        // printf("realloc %p %llu\n", ptr, size);
        // system("pause");
        let nptr = realloc(ptr, size);
        // printf("realloced %p %llu\n", nptr, size);
        // system("pause");
        return nptr;
        // return realloc(ptr, size);
    },
    .free    = [](void* ptr) -> void {return free(ptr);},
};

// static constexpr allocator mem_arena_with_free_list = {};