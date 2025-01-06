#include "src/util/types.h"
#include "src/util/alloc.h"
#include "src/util/buffer.h"

#include <cstdio>

enum class event_id {
    ev_one,
    ev_two,
    ev_mouse_hover,
    ev_draw,
    ev_three,
    ev_max
};


struct event_system {
    // template<typename R, typename... Args>
    // using ev_fn_ty = R(*)(event_system*, Args...);
    using void_fn_ty = void(*)(void);

    void_fn_ty subs[(usize)event_id::ev_max];
};

template<typename Fty, u32 Eid>
struct event {
    u32 ev_id = Eid;
    // func perform() {}
    // static constexpr auto p = 234;
};

// func mk_event() -> event<> {}

event<void(u32 x, i32), event_id::ev_one> ev_one;
event<u32()                    , event_id::ev_two> ev_two;
event<void()                   , event_id::ev_three> ev_three;

event<void(i32 x, i32 y), event_id::ev_mouse_hover> ev_mouse_hover;

template<typename a_ty, typename Rty, typename... Args, typename Fty, event_id Eid>
func subscribe(event_system* ev_sys, event<Rty(Args...), Eid> ev, Fty fn) -> void {
    (*ev_sys).subs[(usize)Eid] = (event_system::void_fn_ty)(Rty(*)(a_ty, Args...))fn;
}

template<typename a_ty, typename Rty, typename... Args, event_id Eid>
func perform(event_system* ev_sys, event<Rty(event_system*, Args...), Eid> ev, a_ty x, Args... args) -> Rty {
    return ((Rty(*)(a_ty, Args...))((*ev_sys).subs[(usize)Eid]))(x, args...);
}

// template<typename R>
// func perform2(event_system* ev_sys, event<R> ev) -> R* {
//     return ((R*)((*ev_sys).subs[ev.ev_id]));
// }


event_system ev_sys = {};

func init_subs() {
    subscribe<event_system*>(&ev_sys, ev_one, [](event_system* ev_sys, u32 x, i32 y) {
        printf("one\n");
        // perform<event_system*>(ev_sys, ev_two, ev_sys);
    });

    subscribe<event_system*>(&ev_sys, ev_two, [](event_system* ev_sys) -> u32 {
        printf("two\n");
        return 123;
    });
    
    subscribe<event_system*>(&ev_sys, ev_three, [](event_system* ev_sys){ printf("three\n"); });
}

func main() -> int {
    init_subs();

    printf("hello\n");

    // perform<event_system*>(&ev_sys, ev_three, &ev_sys);
    // perform<event_system*>(&ev_sys, ev_one, &ev_sys, 100u, 120);
    // let res = perform(&ev_sys, ev_two);

    // printf("res = %u\n", res);
}


template<typename Rty, typename... Args>
using fn_type = Rty(Args...);



template<typename state_ty>
struct ui_component {
    event_system evs;
    state_ty st;
};

template<typename state_ty>
func ui_element_create() -> ui_component<state_ty> {
    return {.evs = {}, .st = {}};
}


template<typename Rty, typename... Args, typename Fty, event_id Eid>
inline func ui_on(event<Rty(Args...), Eid> ev, Fty fn) /*-> fn_type<ui_component<state_ty>, ui_component<state_ty>>*/ {
    return [=]<typename state_ty>(ui_component<state_ty> el) -> ui_component<state_ty> {
        subscribe<ui_component<state_ty>>(&el.evs, ev, fn);
        return el;
    };
}


enum class block_layout {
    horizontal,
    vertical
};

struct ui_context {
    u32 max_width;
    u32 max_height;

    block_layout block_layout;
};

// enum block_events {};
// redraw_masked

struct image_buffer {
    rgba* render_img;
    u16* component_id_img;
    u32 width;
    u32 height;
};

struct ui_back_state {
    image_buffer render_buf;

    u32 components_count;
    u32 components_map[4096]; // component idx -> offset in buffer to the component data
    
    // allocator alc; // component data storage. TODO: Use free list allocator
    buffer components_data;
};

template<typename T, typename Fty>
inline func operator| (T obj, Fty fn) -> decltype(fn(obj)) {
    return fn(obj);
}

template<typename T, typename Fty>
inline func operator>> (T p, Fty q) -> decltype(fn(obj)) {
    return [](ui_context ui_ctx){
        ui_ctx = p(ui_ctx);
        ui_ctx = q(ui_ctx);

        let component_id = bst.components_count;

        bst.components_map[component_id] = bst.components_data.size;
        bst.components_data = buffer_write(bst.components_data, el);
        bst.components_count += 1;

        return ui_ctx;
    };
}

template<typename T>
func ev_draw() -> event<ui_back_state(T, ui_context, ui_back_state), event_id::ev_draw> { return {.ev_id = event_id::ev_draw}; };

struct block_params_ty {u32 width; u32 height; u32 margin; u32 padding; rgba bg_color; u32 border_radius;};

template<u32 ev_lvl>
func block(block_params_ty params, auto childs) {
    enum this_region_events {
        increment,
        decrement
    };

    event_handle_region;
    static let ev_blabla = register_event(event_handle_region, ev_blabla);
    static let ev_blabla2 = register_event(event_handle_region, ev_blabla2);

    using ev_blabla1 = event<void(i32 x, i32 y), ev_lvl + 0>;
    using ev_blabla2 = event<void(i32 x, i32 y), ev_lvl + 1>;
    using ev_blabla3 = event<void(i32 x, i32 y), ev_lvl + 2>;

    let el = ui_element_create<block_params_ty>()
        | ui_on(ev_update, [](block_params_ty ax, ui_context ui_ctx, event_handle_region r, ui_component<block_params_ty> el) -> block_params_ty, ui_context_res {
            emit(r, ev_blabla2, 1, 2);

            return 123;
        })
        | ui_on(ev_draw<asd>(), [](block_params_ty ax, ui_context ui_ctx, ui_back_state bst) -> ui_back_state {
            struct rectangle {
                u32 x;
                u32 y;
                u32 mx;
                u32 my;
            };

            let rc = rectangle {};

            for (var y = rc.y; y < rc.my; ++y)
            for (var x = rc.x; x < rc.mx; ++x) {
                bst.render_buf.render_img[x + y * bst.render_buf.width] = params.bg_color;
            };

            for (var y = rc.y; y < rc.my; ++y)
            for (var x = rc.x; x < rc.mx; ++x) {
                bst.render_buf.component_id_img[x + y * bst.render_buf.width] = (u16)component_id;
            };

            return ui_ctx, bst;
        });

    return el >> childs;
}

func casd() {
    block({}, [=](ui_context ui_ctx, ui_back_state bst) -> ui_back_state {});
}

// func my_el1(ui_context ui_ctx) {
//     struct my_state_ty {
//         u32 counter;
//     } st;

//     return ui_element_create<my_state_ty>()
//     | ui_on(ev_two, [](ui_component<my_state_ty> el) -> u32 {
//         return 123;
//     });
// }

// func asd(ui_context ui_ctx) {
//     my_el1() >> my_el1()
// }

// func asd(ui_context ui_ctx) {
//     ui_ctx = my_el1(ui_ctx);
//     ui_ctx = my_el1(ui_ctx);
// }





// func my_el2() {
//     return block(
//         my_el1() | ui_on() | ui_on(),
//         my_el1()
//     );
// }


// func asd() {
//     let t = text("hello");
//     let b = button("ok");
//     sub(ev_sys, ev_click, [](){
//         emit(ev_sys, t, ev_set_text, "world");
//     });
// }

// enum my_el1_events {
//     my_ev_1,
//     my_ev_2,
// };

// func my_el1() {
//     struct my_state {
//         u32 counter;
//     };

//     return ui_el::create<my_state>()
//     | on(draw, [](my_state* st){
//         text(st.counter);
//     })
//     | on(ev_mouse_clicked, [](my_state* st){
//         st->counter += 1;
//         emit(me, draw)
//     })
//     | on(my_ev_1, {}(my_state* st) {

//     });
// }

// func my_el2() {
//     my_el1()
//     | ev_mouse_hover([](my_state* st){
//         st->color = {.u32 = 0x00ffff};
//         emit(me, draw);
//     })
// }

// func my_el3() {
//     [](ui_ctx) {
//         block({.width = ui_ctx.max_width, .height = ui_ctx.max_height}, [=](){
//             my_el1()
//             | handle(ev_mouse_click, [](params){
//                 params.color = blue;
//                 return params;
//             });
//             my_el2();
//         })
//     }
// }

// operator| (ui_component)
// operator| (ui_component)

// template<typename Fty>
// struct event {};


// template<typename Fty, typename... Args>
// func ff(event<Fty> ev) -> R;

// func test() {
//     let ev1 = event<void(int)> {};
//     let x = ff(ev1);
//     return x;
// }