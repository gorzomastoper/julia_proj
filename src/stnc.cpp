#include "util/types.h"
#include "util/slab_array.h"
#include <cassert>

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

    union data_t {
        struct type_def_t  {def_idx_t typed_def_idx;} type_def;
        struct array_t     {int size;}                array;
        struct structure_t {u8 field_count}           structure;
        struct function_t  {u8 in_pins_count;}        function;
    } data;
};

struct pin_desc {
    type     ty;
    quantity q;
};

struct link {
    u16 src_node_is_bound : 1;
    u16 src_node_idx      : 15;
    u16 dst_node_is_out   : 1;
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
    } data;


    #define mk_node(_tag, _vals) (node {.tag = _tag, .data = {._tag = node::data_t::_tag##_t _vals}})
};

struct definition {
    enum tag_t {
        type = 0,
        node
    };

    tag_t tag;

    union data_t {
        struct type_t {slab_array<def_idx_t> nodes; slab_array<link> links;}               type;
        struct node_t {def_idx_t ty; slab_array<def_idx_t> nodes; slab_array<link> links;} node;
    } data;
};

struct module {
    char                   name[256];
    slab_array<definition> defs;
};
