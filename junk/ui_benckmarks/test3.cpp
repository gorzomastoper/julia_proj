#include <initializer_list>
#include <memory.h>

#include "../../src/util/types.h"
#include "../../src/util/buffer.h"


enum ui_draw_op : u8 {
    udo_rect = 0,
    // udo_text,
    udo_margin,
    udo_padding,
    // udo_border_radius
    udo_position_relative,
    udo_block,
    udo_block_end,
};

struct ui_draw_command {
    ui_draw_op op;
    union {
        struct {u16 width; u16 height; rgba color;} udo_rect;
        struct {u16 margin;}                        udo_margin;
        struct {u16 padding;}                       udo_padding;
        struct {u16 offset_x; u16 offset_y;}        udo_position_relative;
        empty_t                                     udo_block[0];
        empty_t                                     udo_block_end[0];
    } params;
};

struct ui_context {
    u8 childs_count;
    buffer ui_cmd_buf;
};

consteval func mk_ui_context(buffer ui_cmd_buf) -> ui_context {
    return {.childs_count = 0, .ui_cmd_buf = ui_cmd_buf};
}

// constexpr func rect(u16 width, u16 height, rgba color) -> ui_draw_command {
//     return {
//         .op           = udo_rect,
//         .params       = {.udo_rect = {width, height, color}}
//     };
// }

// constexpr func ui_block() -> ui_draw_command {
//     return {
//         .op     = udo_block,
//         .params = {}
//     };
// }

// #define ui_block() {}

#define ui_cmd_size_for(_cmd) offsetof(ui_draw_command, params) + sizeof(_cmd)
#define ui_sizeof_for_tag(ty, tag) offsetof(ty, params) + sizeof ([]() -> ty {return ty {};}().params.tag)


#define ui_mk_basic_op(tag, ...) \
    [=](buffer __ui_cmd_buf) -> buffer {\
        let v = ui_draw_command {\
            .op           = udo_##tag,\
            .params       = {.udo_##tag = {__VA_ARGS__}}\
        };\
        __ui_cmd_buf = buffer_write_bytes(__ui_cmd_buf, ui_sizeof_for_tag(ui_draw_command, udo_##tag), (u8*)&v);\
        return __ui_cmd_buf;\
    }

func rect(u16 width, u16 height, rgba color) {
    return [=](buffer buf) -> buffer {
        return ui_mk_basic_op(rect, width, height, color)(buf);
    };
}


#define ui_(ui_elem, ...) [=](buffer __ui_cmd_buf) -> buffer {return ui_##ui_elem(__ui_cmd_buf, __VA_ARGS__);}
// #define uiw_(e) (__ui_cmd_buf = e(__ui_cmd_buf))
#define ui(...) (__ui_cmd_buf = ui_(__VA_ARGS__)(__ui_cmd_buf))



template<typename T>
func ui_block(T cont) {
    return [=](buffer buf) -> buffer {
        buf = ui_mk_basic_op(block)(buf);
        buf = cont(buf);
        buf = ui_mk_basic_op(block_end)(buf);
        return buf;
    };
}

// #define ui_def(e) [=](buffer __ui_cmd_buf) { return e; }


#define ui_code(e) \
    [=](buffer __ui_cmd_buf) -> buffer { \
        e \
        return __ui_cmd_buf; \
    }

// template<typename T>
// func ui_button(buffer buf, const char* text) {
//     return ui_def(
//         ui_block(
//             ui(rect, 100, 100, {.u32 = 0x00ffff}),
//             ui_code({
//                 printf("button: %s", text);
//             })
//         )
//     );
// }

#define bind(e) (__ui_cmd_buf = (e)(__ui_cmd_buf))

func test7(buffer buf, u32 yy, bool is_blbbla) -> buffer {
    let xx = 0;
    let ew = ui_block(ui_code(
        bind(rect(xx, xx, {.g = 0xff}));

        if (is_blbbla) bind(rect(xx, 0, {.g = 0xff}));
        else           bind(rect(xx, yy, {.g = 0xff}));
    ));
    return ew(buf);
}

#include <stdio.h>

int main() {
    let ui_cmd_buf = buffer_create(default_allocator, 256);
    for(var i = 0; i != 200000000; ++i) {
        ui_cmd_buf.size = 0;
        // ui_cmd_buf = test1(ui_cmd_buf, rand(), rand() % 1);
        // ui_cmd_buf = test2(ui_cmd_buf, rand(), rand() % 1);
        ui_cmd_buf = test7(ui_cmd_buf, rand(), rand() % 1);
    }
    printf("size = %u asfasf %u\n", ui_cmd_buf.size, ((u32*)&ui_cmd_buf.data)[rand() % (ui_cmd_buf.size - 1)]);

    // std::initializer_list<void(*)(void)> x = {[](){}, [](){}};
    let x = (std::initializer_list<void(*)(void)>){[](){}, [](){}}.size();
}

func fas(...) {
    va_list args;
    for (let a : args) {

    }
}

// time test1
// real    0m8.977s
// user    0m8.973s
// sys     0m0.000s

// time test2
// real    0m1.887s
// user    0m1.886s
// sys     0m0.000s
