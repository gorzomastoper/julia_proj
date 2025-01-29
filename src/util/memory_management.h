#pragma once
#include "types.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

void* allocate_memory(void* base, size_t size);


// NOTE(DH): Used to locate place for actual data in arena
struct arena_ptr {
	u32 offset; // TODO(DH): Make this offset 8 bytes, because we (want?) to use more than 4 gb memory :)
};

struct arena_array {
	u32 capacity;
	u32 count;
	arena_ptr ptr;
};

template<typename T>
struct stored_elem {
	u32 reserved;
	T data;
};

struct memory_arena
{
    usize size;
    u8 *base;
    usize used;
    u32 temp_count;

private:
	template<typename T>
	inline func mem_alloc_aligned(memory_arena *arena) -> arena_ptr
	{ 
		usize size = get_effective_size_for(arena, sizeof(stored_elem<T>), default_arena_alignment());
		
		assert((arena->used + size) <= arena->size);
		
		usize alignment_offset = get_alignment_offset(arena, default_arena_alignment());

		arena_ptr result = {.offset = (u32)(arena->used + alignment_offset)}; // TODO(DH): Get rid of this ugly convertion, please!!!
		arena->used += size;
		
		assert(size >= sizeof(T));
		return result;
	}

	template<typename T>
	inline func mem_alloc_aligned(memory_arena *arena, usize count) -> arena_ptr
	{ 
		usize memory_size = (sizeof(T) * count) + sizeof(stored_elem<T>::reserved);
		usize size = get_effective_size_for(arena, memory_size, default_arena_alignment());
		
		assert((arena->used + size) <= arena->size);
		
		usize alignment_offset = get_alignment_offset(arena, default_arena_alignment());
		arena_ptr result = {.offset = (u32)(arena->used + alignment_offset)}; // TODO(DH): Get rid of this ugly convertion, please!!!
		arena->used += size;
		
		assert(size >= memory_size);
		return result;
	}

public:
	template<typename T>
	inline func load(arena_ptr ptr) -> T {
		stored_elem<T> *elem = (stored_elem<T>*)(this->base + ptr.offset);
		return elem->data;
	}

	template<typename T>
	inline func push_data() -> arena_ptr {
		arena_ptr ptr = mem_alloc_aligned<T>(this);
		return ptr;
	}

	template<typename T>
	inline func push_array(usize count) -> arena_array {
		return {.capacity = (u32)count, .count = 0, .ptr = mem_alloc_aligned<T>(this, count)};
	}

	template<typename T>
	inline func load_by_idx(arena_ptr ptr, u32 idx) -> T {
		stored_elem<T> *elem = (stored_elem<T>*)(this->base + ptr.offset);
		//assert(elem->reserved >= idx);
		return ((T*)&elem->data)[idx];
	}

	template<typename T>
	inline func load_ptr_by_idx(arena_ptr ptr, u32 idx) -> T* {
		stored_elem<T> *elem = (stored_elem<T>*)(this->base + ptr.offset);
		//assert(elem->reserved >= idx);
		return &((T*)&elem->data)[idx];
	}

	template<typename T>
	inline func get_ptr(arena_ptr ptr) -> T* {
		stored_elem<T> *elem = (stored_elem<T>*)(this->base + ptr.offset);
		return &elem->data;
	}

	// inline func free_temp(arena_ptr ptr) {
	// 	stored_elem<u8>* elem = (stored_elem<u8>*)(this->base + ptr.offset);
	// 	u32 free_offset = elem->reserved;
	// }
};

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
initialize_arena(usize size)
{
	memory_arena result = {};
    result.size = size;
    result.base = (u8 *)allocate_memory(0, size);
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