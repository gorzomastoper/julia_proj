#pragma once
#include "types.h"
#include <algorithm>

template <typename A, typename B>
using fn1 = B(*)(A);
template <typename A, typename B, typename C>
using fn2 = C(*)(A, B);

template <typename A>
struct slab_array {
    u32 capacity;
    u32 first_free_slot_idx;
    u32 max_used_slot_idx; // TODO
    void* data;

    struct ptr {u32 idx;};

    struct slot_t {
        union {
            A val;
            u32 next_free_slot_idx;
        } data;
        bool in_use;
    };

    static func create(u32 slot_count) -> slab_array<A> {
        // static_assert(sizeof(A) >= sizeof(u32));
        return slab_array<A> {.capacity = slot_count, .first_free_slot_idx = 0, .max_used_slot_idx = 0, .data = calloc(sizeof(slot_t), slot_count)};
    }

    func dealloc() -> void {
        free(this->data);
    }

    func resize(u32 new_capacity) -> void {
        let old_capacity = this->capacity;
        let new_data = realloc(this->data, sizeof(slot_t) * new_capacity);
        memset((slot_t*)new_data + old_capacity, 0, new_capacity - old_capacity);
        this->data = new_data;
        this->capacity = new_capacity;
    }

    func push(A val) -> ptr {
        let data = (slot_t*)this->data;
        if (this->first_free_slot_idx == this->capacity) {
            this->resize(this->capacity * 2);
            this->push(val);
        } else {
            let slot_idx = this->first_free_slot_idx;
            if (data[this->first_free_slot_idx].data.next_free_slot_idx == 0) {
                this->first_free_slot_idx += 1;
            } else {
                this->first_free_slot_idx = data[this->first_free_slot_idx].data.next_free_slot_idx;
            }
            data[slot_idx] = slot_t {.data = {.val = val}, .in_use = true};
            this->max_used_slot_idx = std::max(this->max_used_slot_idx, slot_idx);
            return ptr {slot_idx};
        }
    }

    func remove(ptr _idx) {
        let idx = _idx.idx;
        let data = (slot_t*)this->data;
        data[idx] = slot_t {.data = {.next_free_slot_idx = this->first_free_slot_idx}, .in_use = false};
        this->first_free_slot_idx = idx;
    }

    template <typename B>
    func map(fn1<A, B> f) -> slab_array<B> {
        let result = slab_array<B>::create(this->capacity);
        for (var i = 0; i < this->max_used_slot_idx; i++) {
            let el = ((slot_t*)this->data)[i];
            if (el.in_use) {
                result.push(f(el.data.val));
            }
        }
        return result;
    }

    template <typename F>
    func iter(F f) -> void {
        for (var i = 0; i < this->max_used_slot_idx; i++) {
            let el = ((slot_t*)this->data)[i];
            if (el.in_use) {
                f(el.data.val);
            }
        }
        return;
    }

    template <typename ACC>
    func fold(ACC acc, fn2<A, ACC, ACC> f) -> ACC {
        for (var i = 0; i < this->max_used_slot_idx; i++) {
            let el = ((slot_t*)this->data)[i];
            if (el.in_use) {
                acc = f(el.data.val, acc);
            }
        }
        return acc;
    }

    template <typename ACC>
    func fold_ptr(ACC acc, fn2<A*, ACC, ACC> f) -> ACC {
        for (var i = 0; i < this->max_used_slot_idx; i++) {
            let el = ((slot_t*)this->data)[i];
            if (el.in_use) {
                acc = f(&el.data.val, acc);
            }
        }
        return acc;
    }

    template <typename ACC>
    func iter_with_acc(ACC acc, fn2<A*, ACC, void> f) -> void {
        for (var i = 0; i < this->max_used_slot_idx; i++) {
            let el = ((slot_t*)this->data)[i];
            if (el.in_use) {
                f(&el.data.val, acc);
            }
        }
    }

    func get(ptr i) -> A {
        let data = (slot_t*)this->data;
        return data[i.idx].data.val;
    }

    func get_ptr(ptr i) -> A* {
        let data = (slot_t*)this->data;
        return data[i.idx].data.val;
    }
};