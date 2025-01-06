#pragma once

#include <optional>

#include "types.h"
#include "alloc.h"
// #include "error.h"

template<typename T>
struct list {
    allocator m_allocator;
    T* items;
    usize size, capacity;

    static auto create(allocator allocator, usize capacity) -> list<T> {
        list<T> n;
        n.init(allocator, capacity);
        return n;
    }

    void init(allocator allocator, usize capacity) {
        this->m_allocator = allocator;
        this->size = 0;
        this->capacity = capacity;
        this->items = (T*)allocator.alloc(capacity);
    }

    //auto at(usize i) const -> const T& {
    //    if (i > this->size) panic("index overflow"); // TODO if debug
    //    return this->start_ptr[i];
    //}

    void push(T item) {
        if (this->size == this->capacity) {
            auto new_capacity = this->size*2;
            this->items = (T*)this->m_allocator.realloc(this->items, new_capacity);
            // TODO: check error
            this->capacity = new_capacity;
        }

        this->items[this->size] = item;
        this->size += 1;
    }

    func pop() -> std::optional<T> {
        if (this->size == 0) {
            return std::nullopt;
        } else {
            this->size -= 1;
            return std::optional<T>(this->items[this->size]);
        }
    }

    void resize(usize new_size) {
        if (new_size > this->capacity) {
            this->items = this->m_allocator.realloc(this->items, new_size);
            // TODO: check error
            this->capacity = new_size;
        }
        this->size = new_size;
    }

    void reserve(usize new_size) {
        if (new_size > this->capacity) {
            this->items = this->m_allocator.realloc(this->items, new_size);
            // TODO: check error
            this->capacity = new_size;
        }
    }

    void deinit() {
        this->m_allocator.free(this->items);
    }
};


//template<typename T>
//struct dict {
//    list<str_view>  keys;
//    list<T>         vals;
//
//    inline
//    func find(str_view key, bool* is_not_found) -> T {
//        for (u32 i = 0; i != keys.size; ++i) {
//            auto el = keys.items[i];
//            if (el.cmp(key)) {
//                return vals.items[i];
//            }
//        }
//
//        *is_not_found = true;
//        return;
//    }
//
//    inline
//    func insert(str_view key, T val) {
//        keys.push(key);
//        vals.push(val);
//    }
//};
