#include <initializer_list>
#include <memory.h>

#include "src/util/types.h"
#include "src/util/buffer.h"




enum ui_draw_op : u8 {
    udo_rect = 0,
    // udo_text,
    udo_margin,
    udo_padding,
    // udo_border_radius
    udo_position_relative,
    // display
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
        // struct {u16 childs_count;}                  udo_block;
        struct {std::initializer_list<ui_draw_command> childs;}                  udo_block;
    } params;
};

struct ui_context {
    u8 childs_count;
    buffer ui_cmd_buf;
};

consteval func mk_ui_context(buffer ui_cmd_buf) -> ui_context {
    return {.childs_count = 0, .ui_cmd_buf = ui_cmd_buf};
}

constexpr func ui_rect(u16 width, u16 height, rgba color) -> ui_draw_command {
    return {
        .op           = udo_rect,
        .params       = {.udo_rect = {.width = width, .height = height, .color = color}}
    };
}

constexpr func ui_block(std::initializer_list<ui_draw_command> childs) -> ui_draw_command {
    return {
        .op     = udo_block,
        .params = {.udo_block = {.childs = childs}}
    };
}

#define ui_cmd_size_for(_cmd) offsetof(ui_draw_command, params) + sizeof(_cmd)
#define ui_sizeof_for_tag(ty, tag) offsetof(ty, params) + sizeof ([]() -> ty {return ty {};}().params.tag)

constexpr func build_ui_cmd_buf(buffer ui_cmd_buf, ui_draw_command ui_el) -> buffer {
    switch (ui_el.op) {
        case udo_block: {
            let ch_count = ui_el.params.udo_block.childs.size();
            ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, 8, (u8*)&ui_el.op);
            ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, 8, (u8*)&ch_count);

            for (let ch : ui_el.params.udo_block.childs) {
                ui_cmd_buf = build_ui_cmd_buf(ui_cmd_buf, ch);
            }
        }; break;
        case udo_rect: {
            ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, ui_cmd_size_for(ui_el.params.udo_rect), (u8*)&ui_el);
        }; break;
    };
    return ui_cmd_buf;
}

// #define code(expr) ([=]() -> ui_draw_command {expr})()

func test1(buffer ui_cmd_buf, u32 yy, bool is_blabla) -> buffer {
    // let ui_cmd_buf = buffer_create(default_allocator, 256);

    const let xx = 0;

    let menu1 =
        ui_block({
            ui_rect(xx, 0, {.g = 0xff}),
            code({
                if (is_blabla) return ui_rect(xx, 0, {.g = 0xff});
                else           return ui_rect(xx, yy, {.g = 0xff});
            }),
        });
    ui_cmd_buf = build_ui_cmd_buf(ui_cmd_buf, menu1);

    return ui_cmd_buf;
}

func test2(buffer ui_cmd_buf, u32 yy, bool is_blabla) -> buffer {
    // let ui_cmd_buf = buffer_create(default_allocator, 256);

    let op = udo_block;
    let ch_count = 2;

    ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, 8, (u8*)&op);
    ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, 8, (u8*)&ch_count);

    const let xx = 0;
    let r = ui_rect(xx, 0, {.g = 0xff});
    ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, ui_cmd_size_for(r.params.udo_rect), (u8*)&r);
    if (is_blabla) {
        let r1 = ui_rect(xx, 0, {.g = 0xff});
        ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, ui_cmd_size_for(r1.params.udo_rect), (u8*)&r1);
    } else {
        let r2 = ui_rect(xx, yy, {.g = 0xff});
        ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, ui_cmd_size_for(r2.params.udo_rect), (u8*)&r2);
    }

    return ui_cmd_buf;
}

func test3(buffer ui_cmd_buf, u32 yy, bool is_blabla) -> buffer {
    // let ui_cmd_buf = buffer_create(default_allocator, 256);

    let op = udo_block;
    
    const let xx = 0;

    ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, 8, (u8*)&op);

    let xa = [=](buffer ui_cmd_buf) -> buffer {
        let r = ui_rect(xx, 0, {.g = 0xff});
        ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, ui_cmd_size_for(r.params.udo_rect), (u8*)&r);
        return ui_cmd_buf;
    };

    // const let xx = 0;
    ui_cmd_buf = (
        (ui_cmd_buf =
            xa(ui_cmd_buf)),
        (ui_cmd_buf =
            [=](buffer ui_cmd_buf) -> buffer {
                if (is_blabla) {
                    return (ui_cmd_buf =
                        [=](buffer ui_cmd_buf) -> buffer {
                            let r1 = ui_rect(xx, 0, {.g = 0xff});
                            ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, ui_cmd_size_for(r1.params.udo_rect), (u8*)&r1);
                            return ui_cmd_buf;
                        }(ui_cmd_buf)), ui_cmd_buf;
                } else {
                    return (ui_cmd_buf =
                        [=](buffer ui_cmd_buf) -> buffer {
                            let r2 = ui_rect(xx, yy, {.g = 0xff});
                            ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, ui_cmd_size_for(r2.params.udo_rect), (u8*)&r2);
                            return ui_cmd_buf;
                        }(ui_cmd_buf)), ui_cmd_buf;
                }
            }(ui_cmd_buf)),
        ui_cmd_buf
    );
    
    let end = udo_block_end;
    ui_cmd_buf = buffer_write_bytes(ui_cmd_buf, 8, (u8*)&end);

    return ui_cmd_buf;
}

// func test4() {
//     ui_block() {
//         ui_button();
//         ui_text("hello");
//         ui_button();
//     }
// }

#include <stdio.h>

int main() {
    let ui_cmd_buf = buffer_create(default_allocator, 256);
    for(var i = 0; i != 200000000; ++i) {
        ui_cmd_buf.size = 0;
        // ui_cmd_buf = test1(ui_cmd_buf, rand(), rand() % 1);
        // ui_cmd_buf = test2(ui_cmd_buf, rand(), rand() % 1);
        ui_cmd_buf = test3(ui_cmd_buf, rand(), rand() % 1);
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
