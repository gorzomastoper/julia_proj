#pragma once

#include "types.h"
#include "log.h"
#include <cstdlib>

struct free_list_node {
    u32 start;
    u32 end;
    free_list_node* next;
};

struct free_list_buffer {
    u32 capacity;
    u32 size;
    u8* data;

    free_list_node* free_list;

private:
    template <typename a>
    struct stored_elem {
        u32 size;
        a data;
    };

public:
    struct fl_ptr {
        u32 offset;
    };

    static inline func create(u32 capacity) -> free_list_buffer {
        capacity = capacity == 0 ? 64 : capacity;

        free_list_node* free_list = new free_list_node {.start = 0, .end = capacity, .next = nullptr};
        // free_list_node* free_list = (free_list_node*)malloc(sizeof(free_list_node));
        // *free_list = free_list_node {.start = 0, .end = capacity};

        return free_list_buffer {
            .capacity = capacity,
            .size = 0,
            .data = (u8*)malloc(capacity),
            .free_list = free_list
        };
    }

    inline func alloc_s(u32 size) -> fl_ptr {
        for (var fl = this->free_list, fl_prev = fl;;) {
            let fl_size = fl->end - fl->start;
            let block_size_needed = size + 8;
            // let block_size_needed = size + sizeof(stored_elem<empty_t>);

            if (fl_size >= block_size_needed) {
                let offset = fl->start;

                fl->start = fl->start + block_size_needed;

                if (fl->start == fl->end) {
                    fl_prev->next = fl->next;
                }

                *(stored_elem<empty_t>*)(this->data + offset) = stored_elem<empty_t> {.size = size};

                return debug_ptr_check_init(fl_ptr {.offset = offset});
            } else if (fl->next == nullptr) {
                let offset = fl->start;

                fl->end = fl->end + this->capacity;
                this->capacity = this->capacity * 2;
                this->data = (u8*)realloc(this->data, this->capacity);

                fl->start = fl->start + block_size_needed;

                if (fl->start == fl->end) {
                    fl_prev->next = fl->next;
                }

                *(stored_elem<empty_t>*)(this->data + offset) = stored_elem<empty_t> {.size = size};

                return debug_ptr_check_init(fl_ptr {.offset = offset});
            } else {
                fl_prev = fl;
                fl = fl->next;
                continue;
            }
        }
    }

    inline func free_s(u32 start, u32 size) -> void {
        let block_size = size + 8;
        for (free_list_node* fl = this->free_list, *fl_prev = nullptr;;) {
            if (fl->next != nullptr && fl->end == start && fl->next->start == start + block_size) {
                fl->end = fl->next->end;
                fl->next = fl->next->next;
                delete fl->next;
            } else if (fl->end == start) {
                fl->end = fl->end + block_size;
            } else if (start + block_size == fl->start) {
                fl->start = start;
            } else if (fl_prev && start < fl->start) {
                fl_prev->next = new free_list_node {.start = start, .end = start + block_size, .next = fl};
            } else if (start < fl->start) {
                this->free_list = new free_list_node {.start = start, .end = start + block_size, .next = fl};
                break;
            } else if (fl_prev && fl->next == nullptr) {
                fl_prev->next = new free_list_node {.start = start, .end = start + block_size, .next = fl};
            } else if (fl->next == nullptr) {
                this->free_list = new free_list_node {.start = start, .end = start + block_size, .next = fl};
            } else {
                fl_prev = fl;
                fl = fl->next;
            }
        }
    }

    // template <typename a>
    inline func free(fl_ptr offset_ptr) -> void {
        offset_ptr = debug_ptr_check_elim(offset_ptr);
        let fl_el = (stored_elem<empty_t>*)(this->data + offset_ptr.offset);
        this->free_s(offset_ptr.offset, fl_el->size);
    }

    template <typename a>
    inline func write(a val) -> fl_ptr {
        let fl_el = this->alloc_s(sizeof(val));
        let ptr = (stored_elem<a>*)(this->data + debug_ptr_check_elim(fl_el).offset);
        ptr->data = val;
        return fl_el;
    }

    template <typename a>
    inline func load(fl_ptr offset_ptr) -> a {
        let fl_el = (stored_elem<a>*)(this->data + debug_ptr_check_elim(offset_ptr).offset);
        return fl_el->data;
    }

    template <typename a>
    inline func get_ptr(fl_ptr offset_ptr) -> a* {
        let fl_el = (stored_elem<a>*)(this->data + debug_ptr_check_elim(offset_ptr).offset);
        return &fl_el->data;
    }

    template <typename a>
    inline func update(fl_ptr offset_ptr, auto upd_fn) {
        let fl_el = (stored_elem<a>*)(this->data + debug_ptr_check_elim(offset_ptr).offset);
        return upd_fn(&fl_el->data);
    }

private:
    constexpr func debug_ptr_check_init(fl_ptr ptr) -> fl_ptr {
        return ptr;
        // #if DEBUG
        // return fl_ptr {.offset = ptr.offset + 1};
        // #else
        // return ptr;
        // #endif
    }
    constexpr func debug_ptr_check_elim(fl_ptr ptr) -> fl_ptr {
        return ptr;
        // #if DEBUG
        // if (ptr.offset == 0) panic("ptr.offset == 0");
        // if (ptr.offset % 1 == 1) panic("ptr.offset % 1 == 1");
        // return fl_ptr {.offset = ptr.offset - 1};
        // #else
        // return ptr;
        // #endif
    }
};

template <typename a>
using fl_offset = free_list_buffer::fl_ptr;

using fl_offseta = free_list_buffer::fl_ptr;

// TODO: make offset 0 to be reserved for nullptr