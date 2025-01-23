#pragma once

#include "cstdint"
#include <tuple>
#include <optional>

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef float   f32;
typedef double  f64;

typedef i64 isize;
typedef u64 usize;

// typedef struct { char zero_pls[0]; } empty_t;
typedef char empty_t[0];
static empty_t empty;

template<typename T>
struct str {
    T   size;
    u8  data[];
};

typedef str<u8>     str8;
typedef str<u16>    str16;
typedef str<u32>    str32;
typedef str<u64>    str64;

#define func auto
#define var auto
#define let auto

#define global_variable static

template<typename... T>
using tuple = std::tuple<T...>;

template<typename T>
using optional = std::optional<T>;

union vec4f {
    struct {f32 x; f32 y; f32 z; f32 w;};
    f32 arr[4];
};

union rgba {
    u32                              u32;
    struct {u8 b; u8 g; u8 r; u8 a;};
    u8                               arr[4];
};

#define inline_code(expr) ([=]() {expr})()
#define inline_codex(ty, expr) ([=]() -> ty {expr})()


// template<typename T>
// func id(T e) -> T { return e; }

// template<typename T>
// func min(T x, T y) -> T {
//     return x < y ? x : y;
// }

// template<typename T>
// func max(T x, T y) -> T {
//     return x > y ? x : y;
// }
