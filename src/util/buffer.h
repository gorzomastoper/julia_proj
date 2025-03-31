#pragma once

#include "types.h"
#include "alloc.h"

struct buffer {
    u32       size;
    u32       capacity;
    void*     data;
    allocator alc;
};

inline func buffer_create(allocator alc, u32 capacity /* bytes */) -> buffer {
    capacity = capacity == 0 ? 64 : capacity;
    return {
        .size     = 0,
        .capacity = capacity,
        .data     = alc.alloc(capacity),
        .alc      = alc
    };
}

inline func buffer_resize(buffer buf, u32 size_needed) -> buffer {
    buf.capacity = inline_code(var sc = buf.capacity; while(sc < size_needed) {sc = sc * 2;}; return sc;);
    let data     = buf.alc.realloc(buf.data, buf.capacity);
    buf.data = data;
    return buf;
}

inline func buffer_write_bytes(buffer buf, u32 size, u8* data) -> buffer {
    if (buf.size + size > buf.capacity) { buf = buffer_resize(buf, buf.size + size); }
    memcpy(((u8*)buf.data)+buf.size, data, size);
    buf.size += size;
    return buf;
}

template <typename T>
inline func buffer_write(buffer buf, T val) -> buffer {
    let size = sizeof (T);
    if (buf.size + size > buf.capacity) { buf = buffer_resize(buf, buf.size + size); }
    *(T*)((u8*)buf.data + buf.size) = val;
    buf.size += size;
    return buf;
}


template<typename T, buffer buf>
struct buffer_offset_ptr {
    u32 offset;

    constexpr func get() -> T {
        return *(T*)((u8*)buf.data + this->offset);
    }
};
