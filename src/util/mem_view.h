#pragma once

#include "types.h"
#include "mem.h"
#include "list.h"
#include "error.h"

static int
cstrlen(const char *str)
{
	const char *s;
	for (s = str; *s; ++s)
		;
	return (s - str);
}

template<typename T>
struct mem_view {
    T* start_ptr;
    usize size;

    auto at(usize i) const -> const T& {
        if (i > this->size) panic("index overflow"); // TODO if debug
        //return *(this->start_ptr + i);
        return this->start_ptr[i];
    }

    auto inline cmp(const mem_view<T> other) const -> bool {
        if (this->size != other.size) return false;
        return this->cmp_at(0, other);
    }

    auto inline cmp_at(usize pos, const mem_view<T> other) const -> bool {
        for (usize i = 0; i != other.size; i++) {
            if (this->start_ptr[pos + i] != other.start_ptr[i]) return false;
        }
        return true;
    }

    static constexpr auto from_cstr(const char* cstr) -> mem_view<u8> {
        return mem_view<u8> {(u8*)cstr, (usize)cstrlen(cstr)};
    }

    static auto from_list(list<T> l) -> mem_view<T> {
        return mem_view<T> {(T*)l.items, l.size};
    }
};