#include <initializer_list>
#include <memory.h>


#include <stdio.h>

#include "../../src/util/types.h"
#include "../../src/util/buffer.h"

enum ui_draw_op : u8 {
    udo_invalid,
    udo_block,
    udo_block_end,
    udo_text,
    udo_rect,
    udo_position_relative,
};

struct ui_draw_command {
    // types
    struct block_params_ty             {u32 width; u32 height; u32 margin; u32 padding; rgba bg_color; u32 border_radius;};
    struct rect_params_ty              {u32 x; u32 y; u32 mx; u32 my; rgba color;};
    struct text_params_ty              {const char* text;};
    struct position_relative_params_ty {u32 offset_x; u32 offset_y;};

    // layout
    ui_draw_op op;
    union {
        rect_params_ty  udo_rect;
        text_params_ty  udo_text;
        block_params_ty udo_block;
        empty_t         udo_block_end[0];
        position_relative_params_ty udo_position_relative;
    } params;
};

struct ui_context {
    u32 x;
    u32 y;
    u32 max_width;
    u32 max_height;
};

struct ui_element_result {
    buffer buf;
    u32 width;
    u32 height;
};

// struct ui_context {
//     u8 childs_count;
//     buffer ui_cmd_buf;
// };

// consteval func mk_ui_context(buffer ui_cmd_buf) -> ui_context {
//     return {.childs_count = 0, .ui_cmd_buf = ui_cmd_buf};
// }

// #define ui_cmd_size_for(_cmd) offsetof(ui_draw_command, params) + sizeof(_cmd)
#define ui_sizeof_for_tag(ty, tag) \
    inline_code({ \
        var size = offsetof(ty, params) + sizeof ([]() -> ty {return ty {};}().params.tag); \
        if (size % 8 != 0) { size = size + (8 - size % 8); } \
        return size; \
    })

#define ui_mk_basic_op(tag, pars) \
    [=](ui_context ui_ctx, buffer __ui_cmd_buf) -> ui_element_result {\
        let v = ui_draw_command {\
            .op           = udo_##tag,\
            .params       = {.udo_##tag = pars}\
        };\
        let size = ui_sizeof_for_tag(ui_draw_command, udo_##tag);\
        __ui_cmd_buf = buffer_write_bytes(__ui_cmd_buf, size, (u8*)&v);\
        return ui_element_result {.buf = __ui_cmd_buf, .width = 0, .height = 0};\
    }


#define ui_code(e) \
    [=](ui_context ui_ctx, buffer __ui_cmd_buf) -> ui_element_result { \
        ui_element_result __ui_el_res = {.buf = __ui_cmd_buf, .width = 0, .height = 0};\
        e \
        return {.buf = __ui_el_res.buf, .width = __ui_el_res.width, .height = __ui_el_res.height}; \
    }

#define bind(e) \
    __ui_el_res = ((ui_ctx.y = ui_ctx.y + __ui_el_res.height), (e)(ui_ctx, __ui_el_res.buf))

#define ui_nodiscard [[nodiscard("You probably need to insert \'bind(...)\' here. ")]]

inline func ui_rect(u32 width, u32 height, rgba color) {
    return [=](ui_context ui_ctx, buffer __ui_cmd_buf) -> ui_element_result {
        ui_element_result __ui_el_res = {.buf = __ui_cmd_buf, .width = 0, .height = 0};
        let pa = ui_draw_command::rect_params_ty {.x = ui_ctx.x, .y = ui_ctx.y, .mx = ui_ctx.x + width, .my = ui_ctx.y + height, .color = color};
        bind(ui_mk_basic_op(rect, pa));
        return {.buf = __ui_el_res.buf, .width = width, .height = height};
    };
}

ui_nodiscard
inline func ui_text(const char* text) {
    printf("ui_text: %s\n", text);
    return ui_mk_basic_op(text, {text});
}


template<typename T>
ui_nodiscard
inline func ui_block(ui_draw_command::block_params_ty params, T cont) {
    return [=](ui_context ui_ctx, buffer __ui_cmd_buf) -> ui_element_result {
        ui_element_result __ui_el_res = {.buf = __ui_cmd_buf, .width = 0, .height = 0};

        // __ui_el_res = ui_mk_basic_op(block, params)(ui_ctx, __ui_el_res.buf);

        __ui_el_res = ui_rect(params.width, params.height, params.bg_color)(ui_ctx, __ui_el_res.buf);
        __ui_el_res.width = 0;
        __ui_el_res.height = 0;

        ui_ctx.x = ui_ctx.x + params.padding;
        ui_ctx.y = ui_ctx.y + params.padding;
        ui_ctx.max_width = params.width - params.padding * 2;
        ui_ctx.max_height = params.height - params.padding * 2;
        
        // __ui_el_res.width = ;

        __ui_el_res = cont(ui_ctx, __ui_el_res.buf);

        // ui_ctx.y = ui_ctx.y + __ui_el_res.height;
        // __ui_el_res = ui_mk_basic_op(block_end, {})(ui_ctx, __ui_el_res.buf);

        return {.buf = __ui_el_res.buf, .width = params.width, .height = params.height};
    };
}




ui_nodiscard
inline func ui_button(const char* text) {
    return ui_block({.padding = 5, .bg_color = {.u32 = 0}},
        ui_code(
            printf("ui_button: %s\n", text);
            bind(ui_text(text));
        )
    );
}

inline
func ui_test1(ui_context ctx, buffer buf, u32 yy, bool is_blbbla) -> ui_element_result {
    let ew = ui_block({.width = 500, .height = 200, .padding = 10, .bg_color = {.u32 = 0x5b9945}}, ui_code(
        // bind(ui_text("Hello GUI"));
        // bind(ui_button("OK"));
        bind(ui_block({.width = 50, .height = 50, .bg_color = {.u32 = 0xff0000}}, ui_code()));
        // printf("ui_ctx.x = %u\n", ui_ctx.x);
        // printf("ui_ctx.y = %u\n", ui_ctx.y);
        // printf("ui_ctx.max_width = %u\n", ui_ctx.max_width);
        // printf("ui_ctx.max_height = %u\n", ui_ctx.max_height);
        bind(ui_block({.width = ui_ctx.max_width, .height = ui_ctx.max_height - 50, .bg_color = {.u32 = 0x0b479c}}, ui_code()));
    ));
    return ew(ctx, buf);
}

func ui_test7(ui_context ctx, buffer buf, u32 yy, bool is_blbbla) -> ui_element_result {
    let xx = (u32)0;
    let ew = ui_block({}, ui_code(
        bind(ui_rect(xx, xx, {.g = 0xff}));

        if (is_blbbla) bind(ui_rect(xx, 0, {.g = 0xff}));
        else           bind(ui_rect(xx, yy, {.g = 0xff}));
    ));
    return ew(ctx, buf);
}



int main() {
    let ui_cmd_buf = buffer_create(default_allocator, 256);
    let ctx = ui_context {.max_width = 200, .max_height = 100};
    ui_element_result __ui_el_res = {.buf = ui_cmd_buf, .width = 0, .height = 0};
    for(var i = 0; i != 200000000; ++i) {
        __ui_el_res.buf.size = 0;
        // ui_cmd_buf = test1(ui_cmd_buf, rand(), rand() % 1);
        // ui_cmd_buf = test2(ui_cmd_buf, rand(), rand() % 1);
        __ui_el_res = ui_test7(ctx, __ui_el_res.buf, rand(), rand() % 1);
    }
    printf("size = %u asfasf %u\n", __ui_el_res.buf.size, ((u8*)__ui_el_res.buf.data)[rand() % (__ui_el_res.buf.size - 1)]);

    // std::initializer_list<void(*)(void)> x = {[](){}, [](){}};
    let x = (std::initializer_list<void(*)(void)>){[](){}, [](){}}.size();
}


// time test1
// real    0m8.977s
// user    0m8.973s
// sys     0m0.000s

// time test2
// real    0m1.887s
// user    0m1.886s
// sys     0m0.000s
