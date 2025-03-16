#pragma once
#include "util/types.h"
#include "util/slab_array.h"

template<typename a, template<typename> typename ptr_t>
struct list {
    a                     data;
    ptr_t<list<a, ptr_t>> next;
};

template<typename a>
using slab_array_ptr = slab_array<a>::ptr;

template<typename a>
using slab_list = list<a, slab_array_ptr>;

enum quantity {
    zero = 0,
    one,
    many
};

struct definition;
using def_idx_t = slab_array<definition>::ptr;

struct type {
    enum tag_t {
        i32 = 0,
        f32,
        vec2f,
        vec3f,
        vec4f,
        array,
        structure,
        function,
    };

    tag_t tag;

    ::f32 pos[2];

    union data_t {
        struct f32_t       {}                         f32;
        struct type_def_t  {def_idx_t typed_def_idx;} type_def;
        struct array_t     {int size;}                array;
        struct structure_t {u8 field_count;}          structure;
        struct function_t  {u8 in_pins_count;}        function;
    } data;
};

struct pin_desc {
    type     ty;
    quantity q;
};

struct link {
    u16 src_node_is_bound : 1; // NOTE(DH): Input is argument
    u16 src_node_idx      : 15;
    u16 dst_node_is_out   : 1; // NOTE(DH): 1 == return
    u16 dst_node_idx      : 15; // 0, if dst_node_is_out == 1
    u8 dst_pin_idx;             // 0, if dst_node_is_out == 1
};

struct node {
    enum tag_t {
        node_def = 0,

        i32_lit,
        f32_lit,

        structure_lit,
        array_lit,

        f32_add,
    };

    tag_t tag;

    f32 pos[2];

    union data_t {
        struct node_def_t      {def_idx_t node_def_idx;}                                            node_def;
        struct i32_lit_t       {u32 val;}                                                           i32_lit;
        struct f32_lit_t       {f32 val;}                                                           f32_lit;
        struct structure_lit_t {def_idx_t ty;}                                                      structure_lit;
        struct array_lit_t     {def_idx_t ty;}                                                      array_lit;
        struct lambda_t        {def_idx_t ty; slab_array<def_idx_t> nodes; slab_array<link> links;} lambda;
        struct f32_add_t       {}                                                                   f32_add;
    } data;


    #define mk_node(_tag, _vals) (node {.tag = node::_tag, .data = {._tag = node::data_t::_tag##_t _vals}})
};

struct definition {
    enum tag_t {
        type = 0,
        node
    };

    tag_t tag;

    union data_t {
        struct type_t {slab_array<::type> nodes; slab_array<link> links;}               type;
        struct node_t {u8 in_pins_count; slab_list<::type> in_pin_types; slab_array<::node> nodes; slab_array<link> links;} node;
    } data;
};

struct module {
    char                   name[256];
    slab_array<definition> defs;
};


func example1() -> module {
    let defs = slab_array<definition>::create(128);

    let fn_ty1_els = slab_array<type>::create(4);
    let fn_ty1_links = slab_array<link>::create(4);
    let fn_ty1_idx = fn_ty1_els.push(type {.tag = type::function, .data = {.function = {2}}});
    let a_idx = fn_ty1_els.push(type {.tag = type::f32, .data = {}});
    let b_idx = fn_ty1_els.push(type {.tag = type::f32, .data = {}});
    let c_idx = fn_ty1_els.push(type {.tag = type::f32, .data = {}});
    fn_ty1_links.push(link {0, (u16)a_idx.idx, 0, (u16)fn_ty1_idx.idx, 0});
    fn_ty1_links.push(link {0, (u16)b_idx.idx, 0, (u16)fn_ty1_idx.idx, 1});
    fn_ty1_links.push(link {0, (u16)c_idx.idx, 0, (u16)fn_ty1_idx.idx, 2});
    let my_node1_ty = definition {.tag = definition::type, .data = {.type = {.nodes = fn_ty1_els, .links = fn_ty1_links}}};
    let my_fn1_ty_idx = defs.push(my_node1_ty);

    let my_node1_els = slab_array<node>::create(4);
    let my_node1_links = slab_array<link>::create(4);
    let my_node1_idx = my_node1_els.push(node {.tag = node::f32_add, .data = {}}).idx;
    my_node1_links.push(link {1, 0, 0, (u16)my_node1_idx, 0});
    my_node1_links.push(link {1, 1, 0, (u16)my_node1_idx, 1});
    my_node1_links.push(link {0, (u16)my_node1_idx, 1, 0, 0});
    let my_node1 = definition {.tag = definition::node, .data = {.node = {.ty = my_fn1_ty_idx, .nodes = my_node1_els, .links = my_node1_links}}};
    let my_node1_def_idx = defs.push(my_node1);

	let my_node2_els = slab_array<node>::create(4);
    let my_node2_links = slab_array<link>::create(4);
    let my_node2_idx = my_node2_els.push(node {.tag = node::f32_add, .data = {}}).idx;
    my_node2_links.push(link {1, 0, 0, (u16)my_node2_idx, 0});
    my_node2_links.push(link {1, 1, 0, (u16)my_node2_idx, 1});
    my_node2_links.push(link {0, (u16)my_node2_idx, 1, 0, 0});
    let my_node2 = definition {.tag = definition::node, .data = {.node = {.ty = my_fn1_ty_idx, .nodes = my_node2_els, .links = my_node2_links}}};
    let my_node2_def_idx = defs.push(my_node2);

	let my_node3_els = slab_array<node>::create(4);
    let my_node3_links = slab_array<link>::create(4);
    let my_node3_idx = my_node3_els.push(node {.tag = node::f32_add, .data = {}}).idx;
    my_node3_links.push(link {1, 0, 0, (u16)my_node3_idx, 0});
    my_node3_links.push(link {1, 1, 0, (u16)my_node3_idx, 1});
    my_node3_links.push(link {0, (u16)my_node3_idx, 1, 0, 0});
    let my_node3 = definition {.tag = definition::node, .data = {.node = {.ty = my_fn1_ty_idx, .nodes = my_node3_els, .links = my_node3_links}}};
    let my_node3_def_idx = defs.push(my_node3);

    let my_mod = module {"my module1", defs};

    return my_mod;
}
