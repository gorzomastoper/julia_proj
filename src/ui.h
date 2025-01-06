#pragma once

#include "util/types.h"
#include "util/buffer.h"
#include "util/free_list_alloc.h"
#include <cstdio>
#include <functional>


struct ui_point_2d {
    i32 x;
    i32 y;
};


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
    // struct block_params_ty             {u32 width; u32 height; u32 margin; u32 padding; rgba bg_color; u32 border_radius;};
    struct rect_params_ty              {u32 x; u32 y; u32 mx; u32 my; rgba color;};
    struct text_params_ty              {const char* text;};
    struct position_relative_params_ty {u32 offset_x; u32 offset_y;};

    // layout
    ui_draw_op op;
    union {
        rect_params_ty  udo_rect;
        text_params_ty  udo_text;
        // block_params_ty udo_block;
        empty_t         udo_block_end[0];
        position_relative_params_ty udo_position_relative;
    } params;
};

struct ui_context {
    u32 x;
    u32 y;
    u32 max_width;
    u32 max_height;

    ui_point_2d mouse_pos;

    bool is_mouse_hover;
    bool is_mouse_lbutton_down;
};

inline
func ui_context_compare(ui_context p, ui_context q) {
    return p.x == q.x && p.y == q.y && p.max_width == q.max_width && p.max_height == q.max_height;
}

// struct ui_element_result {
//     buffer buf;
//     u32 width;
//     u32 height;
// };

// // struct ui_context {
// //     u8 childs_count;
// //     buffer ui_cmd_buf;
// // };

// // consteval func mk_ui_context(buffer ui_cmd_buf) -> ui_context {
// //     return {.childs_count = 0, .ui_cmd_buf = ui_cmd_buf};
// // }

// // #define ui_cmd_size_for(_cmd) offsetof(ui_draw_command, params) + sizeof(_cmd)
// #define ui_sizeof_for_tag(ty, tag) \
//     inline_code({ \
//         var size = offsetof(ty, params) + sizeof ([]() -> ty {return ty {};}().params.tag); \
//         if (size % 8 != 0) { size = size + (8 - size % 8); } \
//         return size; \
//     })

// #define ui_mk_basic_op(tag, pars) \
//     [=](ui_context ui_ctx, buffer __ui_cmd_buf) -> ui_element_result {\
//         let v = ui_draw_command {\
//             .op           = udo_##tag,\
//             .params       = {.udo_##tag = pars}\
//         };\
//         let size = ui_sizeof_for_tag(ui_draw_command, udo_##tag);\
//         __ui_cmd_buf = buffer_write_bytes(__ui_cmd_buf, size, (u8*)&v);\
//         return ui_element_result {.buf = __ui_cmd_buf, .width = 0, .height = 0};\
//     }


// #define ui_code(e) \
//     [=](ui_context ui_ctx, buffer __ui_cmd_buf) -> ui_element_result { \
//         ui_element_result __ui_el_res = {.buf = __ui_cmd_buf, .width = 0, .height = 0};\
//         e \
//         return {.buf = __ui_el_res.buf, .width = __ui_el_res.width, .height = __ui_el_res.height}; \
//     }

// #define bind(e) \
//     __ui_el_res = ((ui_ctx.y = ui_ctx.y + __ui_el_res.height), (e)(ui_ctx, __ui_el_res.buf))

// #define ui_nodiscard [[nodiscard("You probably need to insert \'bind(...)\' here. ")]]

// inline func ui_rect(u32 width, u32 height, rgba color) {
//     return [=](ui_context ui_ctx, buffer __ui_cmd_buf) -> ui_element_result {
//         ui_element_result __ui_el_res = {.buf = __ui_cmd_buf, .width = 0, .height = 0};
//         let pa = ui_draw_command::rect_params_ty {.x = ui_ctx.x, .y = ui_ctx.y, .mx = ui_ctx.x + width, .my = ui_ctx.y + height, .color = color};
//         bind(ui_mk_basic_op(rect, pa));
//         return {.buf = __ui_el_res.buf, .width = width, .height = height};
//     };
// }

// ui_nodiscard
// inline func ui_text(const char* text) {
//     printf("ui_text: %s\n", text);
//     return ui_mk_basic_op(text, {text});
// }

// struct ui_back_state;
// struct ui_component_result;

// template<typename T>
// struct ui_componenta {
//     T(*fn)(ui_context, ui_back_state, T* params);
//     T params;
// };

// struct block_params_ty {u32 width; u32 height; u32 margin; u32 padding; rgba bg_color; u32 border_radius; free_list_buffer::fl_ptr childs; u32 has_childs;};


// func ui_block__(ui_context ui_ctx, ui_back_state ui_back, block_params_ty* _params) -> block_params_ty {
//     block_params_ty params = *_params;
//     ui_component_result __ui_el_res = {.ui_ctx = ui_ctx, .ui_back = ui_back, .width = 0, .height = 0};

//     ui_back = ui_draw_rect(params.width, params.height, params.bg_color)(ui_ctx, __ui_el_res.ui_back);

//     ui_ctx.is_mouse_hover = ui_ctx.mouse_pos.x >= ui_ctx.x && ui_ctx.mouse_pos.x <= ui_ctx.x + params.width &&
//                             ui_ctx.mouse_pos.y >= ui_ctx.y && ui_ctx.mouse_pos.y <= ui_ctx.y + params.height;

//     ui_ctx.is_mouse_lbutton_down = ui_ctx.is_mouse_lbutton_down && ui_ctx.is_mouse_hover;

//     ui_ctx.x = ui_ctx.x + params.padding;
//     ui_ctx.y = ui_ctx.y + params.padding;
//     ui_ctx.max_width = params.width - params.padding * 2;
//     ui_ctx.max_height = params.height - params.padding * 2;

//     if (params.has_childs != 0) {
//         // ui_component<empty_t> asd = (*(ui_stored_node<empty_t>*)((u8*)ui_back.components_data.data + params.childs.offset)).data;
//         let asd = ui_back.components_data.load<ui_component<block_params_ty>>(params.childs);
//         let ch = asd.fn(ui_ctx, ui_back, &asd.params);
//         ui_back.components_data.update<ui_component<block_params_ty>>(params.childs, [=](ui_component<block_params_ty>* el) { el->fn = asd.fn; el->params = ch; });
//         // params.childs = el_ptr;

//         return params;
//     } else {
//         printf("dupa");
//         // let asd = ui_back.components_data.load<ui_component<block_params_ty>>(params.childs);

//         return params;
//     }
// }

// // ui_nodiscard
// // inline func ui_block(block_params_ty params, free_list_buffer::fl_ptr ch) -> ui_component<block_params_ty> {
// //     params.childs = ch;

// //     return ui_component<block_params_ty> {
// //             .fn = ui_block__,
// //             .params = params
// //         };
    
// //     // return [=](ui_context ui_ctx, ui_back_state ui_back) mutable -> ui_component<block_params_ty> {
// //     //     let com = cont(ui_ctx, ui_back);
// //     //     let el_ptr = ui_back.components_data.write(com);
// //     //     ui_back.components_map[ui_back.components_count] = el_ptr;
// //     //     ui_back.components_count++;
// //     //     params.childs = el_ptr;

// //     //     return ui_component<block_params_ty> {
// //     //         .fn = ui_block__,
// //     //         .params = params
// //     //     };
// //     // };
// // }



// ui_nodiscard
// inline func ui_button(const char* text) {
//     // return ui_block({.padding = 5, .bg_color = {.u32 = 0}}, [](ui_context ui_ctx, ui_back_state ui_back, ui_draw_command::block_params_ty params) -> ui_back_state {
//     //         return ui_back;
//     //     }
//     //     // ui_code(
//     //     //     printf("ui_button: %s\n", text);
//     //     //     bind(ui_text(text));
//     //     // )
//     // );
// }

// var test_widget_pos = ui_point_2d {0};
// var test_widget_pressed_pos = ui_point_2d {0};
// bool test_widget_was_pressed = false;

// u32 ww = 50;

// func bindq(ui_context ui_ctx, ui_back_state ui_back, auto fn) -> free_list_buffer::fl_ptr {
//     let params = cont(ui_ctx, ui_back);
//     let el_ptr = ui_back.components_data.write(ui_component<decltype(params)> {
//         .fn = fn,
//         .params = params
//     });

//     return el_ptr;
// }

struct image_buffer {
    rgba* render_img;
    u32* component_id_img;
    u32 width;
    u32 height;
};

template <typename a> struct ui_component;

struct ui_back_state {
    // buffer buf;

    image_buffer             render_buf;

    free_list_buffer         components_data;

    u32                      components_count;
    free_list_buffer::fl_ptr components_map[4096]; // component idx -> offset in buffer to the component data

    template <typename a>
    inline func replace(ui_component<a>* el, auto new_el) -> ui_component<a>* {
        this->components_data.free(free_list_buffer::fl_ptr {.offset = (u32)((u8*)el - (u8*)this->components_data.data) - 8});
        // let el_offset = this->components_data.write(new_el);
        let el_offset = new_el(this);
        return this->components_data.get_ptr<ui_component<a>>(el_offset);
    }
};

func mk_ui_back_state(image_buffer render_buf) -> ui_back_state {
    return {
        // .buf = buffer_create(default_allocator, 1000),
        .render_buf = render_buf,
        .components_data = free_list_buffer::create(1000),
        .components_count = 0
    };
};

static u32 bbb = 0;

inline func ui_draw_rect(u32 width, u32 height, rgba color) {
    return [=](ui_context ui_ctx, ui_back_state* ui_back) {
        for (var y = ui_ctx.y; y < min(height, ui_back->render_buf.height); ++y)
        for (var x = ui_ctx.x; x < min(width, ui_back->render_buf.width); ++x) {
            ui_back->render_buf.render_img[x + y * ui_back->render_buf.width] = color;
            // ui_back->render_buf.render_img[x + y * ui_back->render_buf.width] = (rgba)(color.u32 + bbb);
        };

        bbb += 10;
    };
}

struct my_params {};


struct ui_block_params {
    u32 width; u32 height; u32 margin; u32 padding; rgba bg_color; u32 border_radius;
    free_list_buffer::fl_ptr childs;
};

template <typename a>
struct ui_component {
    using com_ty = ui_component<a>;
    using com_offset = fl_offset<ui_component<a>>;
    // using com_ptr = ui_component<a>*;

    using draw_fn_ty = void(*)(ui_back_state*, fl_offset<a> el_offset);
    using update_fn_ty = ui_component<a>*(*)(ui_back_state*, fl_offset<a>, a);
    using mouse_move_fn_ty = ui_component<a>*(*)(ui_back_state*, fl_offset<a>, ui_point_2d pos);
    // using mouse_press_fn_ty = com_offset(*)(ui_back_state*, com_offset, ui_point_2d pos);
    using mouse_press_fn_ty = void(*)(ui_back_state*, com_ty*, ui_point_2d pos);

    draw_fn_ty   draw_fn;
    update_fn_ty update_fn;
    mouse_move_fn_ty mouse_move;
    mouse_press_fn_ty mouse_press;
    // update_fn_ty mouse_release;

    struct {
        update_fn_ty update;
        mouse_move_fn_ty mouse_move;
        mouse_press_fn_ty mouse_press;
        // update_fn_ty mouse_release;
    } user_callbacks;

    free_list_buffer::fl_ptr parent;

    ui_context   ui_ctx;
    a            params;

    // event_region;

    func on_mouse_press(mouse_press_fn_ty mouse_press_cb) -> ui_component<a>* {
        this->user_callbacks.mouse_press = mouse_press_cb;
        return this;
    }
};

template<typename T, typename Fty>
inline func operator| (T obj, Fty fn) -> decltype(fn(obj)) {
    return fn(obj);
}

template <typename a>
inline func ui_on_update(typename ui_component<a>::update_fn_ty update_cb) {
    return [=](auto f) {
        return [=](ui_back_state* ui_back) mutable {
            let offset = f(ui_back);
            let el = ui_back->components_data.get_ptr<ui_component<a>>(offset);
            el->user_callbacks.update = update_cb;
            return offset;
        };
    };
}

template <typename a>
inline func ui_on_mouse_move(typename ui_component<a>::mouse_move_fn_ty mouse_move_cb) {
    return [=](auto f) {
        return [=](ui_back_state* ui_back) mutable {
            let offset = f(ui_back);
            let el = ui_back->components_data.get_ptr<ui_component<a>>(offset);
            el->user_callbacks.mouse_move = mouse_move_cb;
            return offset;
        };
    };
}

template <typename a>
inline func ui_on_mouse_press(typename ui_component<a>::mouse_press_fn_ty mouse_press_cb) {
    return [=](auto f) {
        return [=](ui_back_state* ui_back) mutable {
            let offset = f(ui_back);
            let el = ui_back->components_data.get_ptr<ui_component<a>>(offset);
            el->user_callbacks.mouse_press = mouse_press_cb;
            return offset;
        };
    };
}

template <typename a>
inline
func ui_on_draw(typename ui_component<a>::draw_fn_ty draw_fn) {
    return [=](ui_component<a> el) {
        el.draw_fn = draw_fn;
        return el;
    };
}

template <typename a>
func ui_store(ui_back_state* ui_back) -> std::function<fl_offset<a>(ui_component<a>)> {
    return [=](ui_component<a> el) -> fl_offset<a> {
        return (fl_offset<a>)ui_back->components_data.write(el);
    };
}

// inline func resize(u32 width, u32 height) {
//     return []<typename a>(ui_component<a> el) -> ui_component<a> {
//         el.params
//     };
// }


inline
func ui_block(ui_block_params params)  {
    return [=](ui_back_state* ui_back) mutable -> fl_offset<ui_block_params> {
        let el = ui_component<ui_block_params> {
            .draw_fn = [](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset) {
                let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);

                let params = el->params;
                ui_draw_rect(params.width, params.height, params.bg_color)(el->ui_ctx, ui_back);

                for (var y = el->ui_ctx.y; y < min(params.height, ui_back->render_buf.height); ++y)
                for (var x = el->ui_ctx.x; x < min(params.width, ui_back->render_buf.width); ++x) {
                    ui_back->render_buf.component_id_img[x + y * ui_back->render_buf.width] = el_offset.offset;
                };
            },
            .update_fn = [](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_block_params params) {
                let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                let prev_params = el->params;
                el->params = params;
                if (el->user_callbacks.update) el->user_callbacks.update(ui_back, el_offset, params);

                printf("updating\n");

                // if cur size < prev size then redraw parent(with mask(only affected area))
                if (params.width < prev_params.width || params.height < prev_params.height) {
                    let parent_el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el->parent);
                    parent_el->draw_fn(ui_back, el->parent);
                }

                el->draw_fn(ui_back, el_offset);
                
                return el;
            },
            .mouse_move = [](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_point_2d pos) {
                let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                if (el->user_callbacks.mouse_move) el->user_callbacks.mouse_move(ui_back, el_offset, pos);
                // el->update_fn(ui_back, el);
                return el;
            },
            .mouse_press = [](ui_back_state* ui_back, ui_component<ui_block_params>* el, ui_point_2d pos) {
                if (el->user_callbacks.mouse_press) {
                    el->user_callbacks.mouse_press(ui_back, el, pos);
                }
                // let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                // if (el->user_callbacks.mouse_press) {
                //     let el_offset_new = el->user_callbacks.mouse_press(ui_back, el_offset, pos);
                //     if (el_offset.offset != el_offset_new.offset) {
                //         let el_parent = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el->parent);
                //         el_parent->params.childs = el_offset_new;
                //         let el_new = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset_new);
                //         el_new->parent = el->parent;
                //         // el_new->ui_ctx = el->ui_ctx;
                //         el_new->update_fn(ui_back, el_offset_new, el_new->params);
                //         // el_new->draw_fn(ui_back, el_offset_new);
                //         return el_offset_new;
                //     }
                // }
                // return el_offset;
            },
            .params = params
        };

        return ui_back->components_data.write(el);
    };
}

inline
func ui_block(ui_block_params params, auto ch)  {
    return [=](ui_back_state* ui_back) mutable -> fl_offset<ui_block_params> {
        params.childs = ch(ui_back);
        printf("params.childs = %u\n", params.childs.offset);

        let el = ui_component<ui_block_params> {
            .draw_fn = [](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset) {
                let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);

                let params = el->params;
                ui_draw_rect(params.width, params.height, params.bg_color)(el->ui_ctx, ui_back);

                for (var y = el->ui_ctx.y; y < min(params.height, ui_back->render_buf.height); ++y)
                for (var x = el->ui_ctx.x; x < min(params.width, ui_back->render_buf.width); ++x) {
                    ui_back->render_buf.component_id_img[x + y * ui_back->render_buf.width] = el_offset.offset;
                };

                let ch = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el->params.childs);
                ch->draw_fn(ui_back, el->params.childs);
            },
            .update_fn = [](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_block_params params) {
                let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                let prev_params = el->params;
                el->params = params;
                if (el->user_callbacks.update) el->user_callbacks.update(ui_back, el_offset, params);

                printf("updating\n");
                params = el->params;

                let ui_ctx = el->ui_ctx;

                ui_ctx.is_mouse_hover = ui_ctx.mouse_pos.x >= ui_ctx.x && ui_ctx.mouse_pos.x <= ui_ctx.x + params.width &&
                                        ui_ctx.mouse_pos.y >= ui_ctx.y && ui_ctx.mouse_pos.y <= ui_ctx.y + params.height;

                ui_ctx.is_mouse_lbutton_down = ui_ctx.is_mouse_lbutton_down && ui_ctx.is_mouse_hover;

                ui_ctx.x = ui_ctx.x + params.padding;
                ui_ctx.y = ui_ctx.y + params.padding;
                ui_ctx.max_width = params.width - params.padding;
                ui_ctx.max_height = params.height - params.padding;

                let ch = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el->params.childs);
                if (ui_context_compare(ch->ui_ctx, ui_ctx) == false) {
                    ch->ui_ctx = ui_ctx;
                    ch->update_fn(ui_back, el->params.childs, ch->params);
                }

                // if cur size < prev size then redraw parent(with mask(only affected area))
                if (params.width < prev_params.width || params.height < prev_params.height) {
                    let parent_el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el->parent);
                    parent_el->draw_fn(ui_back, el->parent);
                }

                el->draw_fn(ui_back, el_offset);

                return el;
            },
            .mouse_move = [](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_point_2d pos) {
                let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                if (el->user_callbacks.mouse_move) el->user_callbacks.mouse_move(ui_back, el_offset, pos);
                // el->update_fn(ui_back, el);
                return el;
            },
            .mouse_press = [](ui_back_state* ui_back, ui_component<ui_block_params>* el, ui_point_2d pos) {
                if (el->user_callbacks.mouse_press) {
                    el->user_callbacks.mouse_press(ui_back, el, pos);
                }
            },
            // .mouse_press = [](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_point_2d pos) {
            //     let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
            //     if (el->user_callbacks.mouse_press) {
            //         let el_offset_new = el->user_callbacks.mouse_press(ui_back, el_offset, pos);
            //         if (el_offset.offset != el_offset_new.offset) {
            //             let el_parent = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el->parent);
            //             el_parent->params.childs = el_offset_new;
            //             let el_new = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset_new);
            //             el_new->parent = el->parent;
            //             // el_new->ui_ctx = el->ui_ctx;
            //             el_new->update_fn(ui_back, el_offset_new, el_new->params);
            //             // el_new->draw_fn(ui_back, el_offset_new);
            //             return el_offset_new;
            //         }
            //     }
            //     return el_offset;
            // },
            .params = params
        };

        let el_offset = ui_back->components_data.write(el);
        
        let ch_el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(params.childs);
        ch_el->parent = el_offset;

        return el_offset;
    };
}

template <typename a>
inline func on_mouse_press(typename ui_component<a>::mouse_press_fn_ty mouse_press_cb) {
    return [=](ui_component<a>* el) {
        el->user_callbacks.mouse_press = mouse_press_cb;
        return el;
    };
}

inline func pure(fl_offset<ui_block_params> el) {
    return [=](ui_context ui_ctx, ui_back_state* ui_back) {return el;};
}

inline
func my_component(my_params params) {
    return ui_block({},
    // return
        ui_block({.width = 0, .height = 0, .padding = 2, .bg_color = {.u32 = 0x292929}},
            ui_block({.width = 25, .height = 150, .bg_color = {.u32 = 0x5b9945}})
            | ui_on_update<ui_block_params>([](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_block_params params) {
                let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                el->params.width = el->ui_ctx.max_width;
                return el;
            })
            // | ui_on_mouse_press<ui_block_params>([](ui_back_state* ui_back, fl_offset<ui_block_params> el, ui_point_2d pos) {
            //     return el | set_bg_color(0x5b9945) | on_mouse_press([](el) {return el | recc();}) | handle(my_ev1, [](el, emiter_el, args...) {});
            // })
            // | ui_on_mouse_press<ui_block_params>([](ui_back_state* ui, ui_component<ui_block_params>* el, ui_point_2d pos) {
            //     // el | on_mouse_press<ui_block_params>([](ui_back_state* ui, ui_component<ui_block_params>* el, ui_point_2d pos) {});
            //     el->on_mouse_press([](ui_back_state* ui, ui_component<ui_block_params>* el, ui_point_2d pos) {});
            // })
            | ui_on_mouse_press<ui_block_params>([](ui_back_state* ui, ui_component<ui_block_params>* el, ui_point_2d pos) {
                // let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                // let params = el->params;
                // params.bg_color = {.u32 = 0x5b9945 + (u32)pos.x};
                // el->update_fn(ui_back, el_offset, params);
                //////// id([]{struct {u32 x; func upd() {x = 234;}} asd; return asd;});

                printf("pressed\b");

                // ui_back->components_data.free(el_offset);

                // el_offset = ui_block({})(el->ui_ctx, ui_back);

                ui->replace(el, ui_block({.width = 50, .height = 50, .bg_color = {.u32 = 0x292929}}));

                // return ui_block({.width = 50, .height = 50, .bg_color = {.u32 = 0x292929}})(el->ui_ctx, ui_back);
                // return ui_back.add(ui_block({.width = 50, .height = 50, .bg_color = {.u32 = 0x292929}}));
                // // return el | replace(ui_block({.width = 50, .height = 50, .bg_color = {.u32 = 0x292929}})(el->ui_ctx, ui_back));
                // return el | ui_back.replace(ui_block({.width = 50, .height = 50, .bg_color = {.u32 = 0x292929}})(el->ui_ctx, ui_back));
            })
            // | ui_on_mouse_press<ui_block_params>([](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_point_2d pos) {
            //     let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
            //     let params = el->params;
            //     params.bg_color = {.u32 = 0x5b9945 + (u32)pos.x};
            //     el->update_fn(ui_back, el_offset, params);
            //     return el;
            // })
        )
        | ui_on_update<ui_block_params>([](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_block_params params) {
            let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
            el->params.width = el->ui_ctx.max_width/2;
            el->params.height = el->ui_ctx.max_height/2;
            // el | param(width, el->ui_ctx.max_width/2)
            return el;
        })
        // | ui_on_mouse_move<ui_block_params>([](ui_back_state* ui_back, ui_component<ui_block_params>* el, ui_point_2d pos) {
        //     // el->params.width = (u32)el->ui_ctx.is_mouse_lbutton_down*50;
        //     // el->params.height = (u32)el->ui_ctx.is_mouse_lbutton_down*50;
        //     el->params.width = pos.x;
        //     el->params.height = pos.y;
        //     el->update_fn(ui_back, el);
        //     // el->params.width = el->ui_ctx.max_width/2;
        //     // el->params.height = el->ui_ctx.max_height/2;
        //     return el;
        // })
        | ui_on_mouse_press<ui_block_params>([](ui_back_state* ui_back, ui_component<ui_block_params>* el, ui_point_2d pos) {
            // let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
            let params = el->params;
            params.bg_color = {.u32 = 0x00};
            el->update_fn(ui_back, {.offset = (u32)((u8*)el - (u8*)ui_back->components_data.data) - 8}, params);
            el->user_callbacks.mouse_press = [](ui_back_state* ui_back, ui_component<ui_block_params>* el, ui_point_2d pos) {
                // let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
                let params = el->params;
                params.bg_color = {.u32 = 0x8a4515 + (u32)pos.x};
                el->update_fn(ui_back, {.offset = (u32)((u8*)el - (u8*)ui_back->components_data.data) - 8}, params);
                // return el_offset;
            };
            // return el_offset;
        })
    )
    | ui_on_update<ui_block_params>([](ui_back_state* ui_back, fl_offset<ui_block_params> el_offset, ui_block_params params) {
        let el = ui_back->components_data.get_ptr<ui_component<ui_block_params>>(el_offset);
        el->params.width = el->ui_ctx.max_width;
        el->params.height = el->ui_ctx.max_height;
        return el;
    });
}

// func my_component(my_params params) {
//     return [=](ui_context ui_ctx, ui_back_state* ui_back) -> fl_offset<ui_component<my_params>> {
//         ui_block({.width = ui_ctx.max_width/2, .height = ui_ctx.max_height/2, .padding = 20, .bg_color = {.u32 = 0x292929}},
//             [](ui_component<ui_block_params> old_el, ui_context ui_ctx, ui_back_state ui_back) {
//                 return ui_block({.width = ui_ctx.max_width, .height = 50, .bg_color = {.u32 = 0x5b9945}});
//             }
//         );
//     };
// }

inline
func ui_test0(ui_context ui_ctx, ui_back_state* ui_back) {
    return my_component({})(ui_back);
}


// inline
// func ui_test1(ui_context ui_ctx, ui_back_state back_state, u32 yy, bool is_blbbla) -> ui_component_result {
//     // struct my_state {
//     //     u32 yy;
//     //     bool is_blbbla;
//     // } st = {yy, is_blbbla};

//     // let el = ui_component<my_state> {
//     //     .fn = [](ui_context ui_ctx, ui_back_state ui_back, my_state* st) -> ui_component<my_state> {
//     //         let main_win = ui_block({.width = ui_ctx.max_width/2, .height = ui_ctx.max_height/2, .padding = 20, .bg_color = {.u32 = 0x292929}, .has_childs = 1},
//     //             [](ui_context ui_ctx, ui_back_state ui_back) -> ui_component<block_params_ty> {
//     //                 let res = ui_block({.width = ww++, .height = 50, .bg_color = {.u32 = 0x5b9945}}, [](ui_context ui_ctx, ui_back_state ui_back) -> ui_component<block_params_ty> { return {.ui_back = ui_back}; })(ui_ctx, ui_back);
//     //                 return res;
//     //             }
//     //         );

//     //         return main_win(ui_ctx, ui_back);
//     //     },
//     //     .params = st
//     // };

//     let main_win = ui_block({.width = ui_ctx.max_width/2, .height = ui_ctx.max_height/2, .padding = 20, .bg_color = {.u32 = 0x292929}, .has_childs = 1},
//         bindq([](ui_context ui_ctx, ui_back_state ui_back) -> empty_t {
//             printf("ui_ctx.max_width = %u", ui_ctx.max_width);
//             ui_block({.width = ui_ctx.max_width, .height = 50, .bg_color = {.u32 = 0x5b9945}}, {});
//             // return ui_block({.width = ui_ctx.max_width, .height = 50, .bg_color = {.u32 = 0x5b9945}}, [](ui_context ui_ctx, ui_back_state ui_back) -> ui_component<block_params_ty> { return {}; })(ui_ctx, ui_back);
//         })
//     );

//     let el = main_win(ui_ctx, back_state);
    
//     let com = el;
//     let el_ptr = back_state.components_data.write(com);
//     back_state.components_map[back_state.components_count] = el_ptr;
//     back_state.components_count++;
//     // params.childs = el_ptr;


//     // let res = el.fn(ui_ctx, back_state, &el.params);
//     // back_state = res.ui_back;

//     return {
//         .ui_back = back_state
//     };
// }

// inline
// func ui_test1(ui_context ui_ctx, ui_back_state back_state, u32 yy, bool is_blbbla) -> ui_component_result {
//     struct my_state {
//         u32 yy;
//         bool is_blbbla;
//     } st = {yy, is_blbbla};

//     let main_win = ui_block<my_state>({.width = ui_ctx.max_width/2, .height = ui_ctx.max_height/2, .padding = 20, .bg_color = {.u32 = 0x292929}}, [](ui_context ui_ctx, ui_back_state ui_back, my_state st) -> ui_back_state {
//         ui_back = ui_block<my_state>({.width = ui_ctx.max_width, .height = ui_ctx.max_height, .bg_color = {.u32 = 0x5b9945}}, [](ui_context ui_ctx, ui_back_state ui_back, my_state st) -> ui_back_state { return ui_back; })(ui_ctx, ui_back);
//         return ui_back;
//     });

//     back_state = main_win(ui_ctx, back_state);
//     // back_state.buf.size = 0;

//     // ui_component<ui_draw_command::block_params_ty> asd = *(ui_component<ui_draw_command::block_params_ty>*)back_state.components_data.data;

//     // let res = asd.fn(ui_ctx, back_state, asd.params);

//     return {
//         .ui_back = back_state
//     };
//     // return main_win.fn(ui_ctx, buf, main_win.params);
// }

// inline
// func ui_test1(ui_context ui_ctx, buffer buf, u32 yy, bool is_blbbla) -> ui_element_result {
//     let main_win = ui_block({.width = ui_ctx.max_width, .height = ui_ctx.max_height, .bg_color = {.u32 = 0x292929}}, ui_code(
//         bind(ui_block({.width = 500, .height = 200}, ui_code(
//             bind(ui_block({.width = ui_ctx.max_width, .height = ui_ctx.max_height, .padding = 10, .bg_color = {.u32 = 0x5b9945}}, ui_code(
//                 bind(ui_block({.width = ui_ctx.max_width, .height = ui_ctx.max_height - 50, .bg_color = {.u32 = 0xebae34}}, ui_code()));
//             )));
//         )));
//     ));

//     return main_win(ui_ctx, buf);
// }


// func ui_test7(ui_context ctx, buffer buf, u32 yy, bool is_blbbla) -> ui_element_result {
//     let xx = (u32)0;
//     let ew = ui_block({}, ui_code(
//         bind(ui_rect(xx, xx, {.g = 0xff}));

//         if (is_blbbla) bind(ui_rect(xx, 0, {.g = 0xff}));
//         else           bind(ui_rect(xx, yy, {.g = 0xff}));
//     ));
//     return ew(ctx, buf);
// }

