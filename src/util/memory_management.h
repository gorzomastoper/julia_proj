#pragma once
#include "types.h"
#include <cassert>

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

struct memory_arena
{
    usize size;
    u8 *base;
    usize used;
	
    u32 temp_count;
};

typedef void*(*default_alloc_lambda)(void*, memory_arena, usize);

struct temporary_memory
{
    memory_arena *arena;
    usize used;
};

inline usize
default_arena_alignment(void)
{
	usize result = 4; //NOTE(DH): Align to 4 bytes by default
	return(result);
}

inline memory_arena
initialize_arena(usize size, void *base)
{
	memory_arena result = {};
    result.size = size;
    result.base = (u8 *)base;
    result.used = 0;
    result.temp_count = 0;

	return result;
}

inline usize
get_alignment_offset(memory_arena *arena,  usize alignment)
{
	usize alignment_offset = 0;
	usize result_pointer = (usize)arena->base + arena->used;
	usize alignment_mask = alignment - 1;
	if(result_pointer & alignment_mask)
	{
		alignment_offset = alignment - (result_pointer & alignment_mask);
	}
	return(alignment_offset);
}

inline usize
get_arena_size_remaining(memory_arena *arena, usize alignment)
{
	usize Result = arena->size - (arena->used + get_alignment_offset(arena, alignment));
	return(Result);
}

inline usize
get_effective_size_for(memory_arena *arena, usize size_init, usize alignment)
{
	usize size = size_init;
	
	usize alignment_offset = get_alignment_offset(arena, alignment);
	size += alignment_offset;
	
	return size;
}

inline bool
arena_has_place_for(memory_arena *arena, usize size_init, usize alignment)
{
	usize size = get_effective_size_for(arena, size_init, alignment);
	bool result = ((arena->used + size) <= arena->size);
	return result;
}

template<typename T>
T* arena_mem_alloc(memory_arena *arena, usize size_init) 
{ 
	usize size = get_effective_size_for(arena, size_init, default_arena_alignment());
	
	assert((arena->used + size) <= arena->size);
	
	usize alignment_offset = get_alignment_offset(arena, default_arena_alignment());
	T *result = (T*)(arena->base + arena->used + alignment_offset);
	arena->used += size;
	
	assert(size >= size_init);
	return result;
}