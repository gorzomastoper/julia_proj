#include <algorithm>
#include <cstdio>
#include <stdlib.h>

#include "util/types.h"
#include "util/list.h"
#include "util/alloc.h"

enum world_element : u32 {
    we_none,
    we_grass,
    we_dirt,
    we_water,
    we_lava,
    we_stone,
    we_max
};
// const u32 we_max = (u32)world_element::max;
// #define we world_element

enum map_el_bits {
    is_processing_bit   = 1 << 31,
    is_in_ng_list_bit   = 1 << 30,
    neighbor_set_bits   = ~0u >> 2,
    neighbor_count_bits = (~0u >> (32 - 8)) << 16,
};

func get_2d(world_element* map, u32 line_size, i32 x, i32 y) -> world_element {return map[(x < 0 ? 0 : x) + (y < 0 ? 0 : y) * line_size];}

func map_set_logand_2d(u32* map, u32 line_size, i32 x, i32 y, u32 v) {
    if (map[(x < 0 ? 0 : x) + (y < 0 ? 0 : y) * line_size] & is_processing_bit) {
        map[(x < 0 ? 0 : x) + (y < 0 ? 0 : y) * line_size] &= v | is_processing_bit;
    }
}

func map_set_logor_2d(u32* map, u32 mx, u32 my, i32 x, i32 y, u32 v) {
    if (x < 0 || x > mx || y < 0 || y > my) {
        return;
    }

    if (map[x + y * mx] & is_processing_bit) {
        u32 ng_count = (map[x + y * mx] & neighbor_count_bits) >> 16;

        map[x + y * mx] |= v | is_processing_bit | ((ng_count + 1) << 16);
    }
}

func set_prob(u32 prob_map[we_max][we_max], world_element p, world_element q, u32 prob) {
    prob_map[p][q] = prob;
    prob_map[q][p] = prob;
}

struct neighbor_info {
    bool processed;
    u32 inv_entropy;
    u32 x;
    u32 y;
};

inline func mb_add_to_ng_list(list<neighbor_info> ng_list, list<u32> free_ng_ls, u32* map, u32 mx, u32 my, i32 x, i32 y) -> tuple<list<neighbor_info>, list<u32>> {
    if (x < 0 || x > mx || y < 0 || y > my) {
        return {ng_list, free_ng_ls};
    }

    // u32 x = (_x < 0 ? 0 : _x);
    // u32 y = (_y < 0 ? 0 : _y);
    let m = &map[x + y * mx];

    if ((*m & is_processing_bit) && !(*m & is_in_ng_list_bit)) {
        // printf("add_to_ng_list: x, y = %u, %u\n", x, y);
        
        u32 ng_count = (*m & neighbor_count_bits) >> 16;
        u32 inv_entropy = __builtin_popcountl(*m & neighbor_set_bits);
        if (let free_ng_idx = free_ng_ls.pop()) {
            ng_list.items[*free_ng_idx] = {false, inv_entropy, (u32)x, (u32)y};
        } else {
            ng_list.push({false, inv_entropy, (u32)x, (u32)y});
        }
        *m |= is_in_ng_list_bit;
    }

    return {ng_list, free_ng_ls};
}

inline func update_neighbors(u32* map, u32 mx, u32 my, list<neighbor_info> ng_list, list<u32> free_ng_ls, i32 x, i32 y) -> tuple<list<neighbor_info>, list<u32>> {
    world_element c = (world_element)map[x + y * mx];
    
    map_set_logor_2d(map, mx, my, x - 1, y    , 1 << c);
    map_set_logor_2d(map, mx, my, x + 1, y    , 1 << c);
    map_set_logor_2d(map, mx, my, x - 1, y - 1, 1 << c);
    map_set_logor_2d(map, mx, my, x    , y - 1, 1 << c);
    map_set_logor_2d(map, mx, my, x + 1, y - 1, 1 << c);
    map_set_logor_2d(map, mx, my, x - 1, y + 1, 1 << c);
    map_set_logor_2d(map, mx, my, x    , y + 1, 1 << c);
    map_set_logor_2d(map, mx, my, x + 1, y + 1, 1 << c);

    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x - 1, y);
    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x + 1, y);
    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x - 1, y + 1);
    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x    , y + 1);
    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x + 1, y + 1);
    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x - 1, y - 1);
    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x    , y - 1);
    std::tie(ng_list, free_ng_ls) = mb_add_to_ng_list(ng_list, free_ng_ls, map, mx, my, x + 1, y - 1);

    return {ng_list, free_ng_ls};
}

func generate_one(u32* map, u32 mx, u32 my, list<neighbor_info> ng_list, list<u32> free_ng_ls, i32 best_ng_idx) -> optional<tuple<list<neighbor_info>, list<u32>, u32>> {
    u32 prob_map[we_max][we_max] = {0};

    set_prob(prob_map, we_grass, we_grass, 100);
    set_prob(prob_map, we_dirt , we_dirt , 10);
    set_prob(prob_map, we_water, we_water, 10);
    set_prob(prob_map, we_lava , we_lava , 10);
    set_prob(prob_map, we_stone, we_stone, 10);

    set_prob(prob_map, we_grass, we_dirt, 1);
    // prob_map[we_grass][we_dirt] = 1;
    // prob_map[we_dirt][we_grass] = 1;

    set_prob(prob_map, we_dirt, we_water, 1);
    // set_prob(prob_map, we_grass, we_stone, 1);
    set_prob(prob_map, we_dirt , we_stone, 1);
    // set_prob(prob_map, we_grass, we_dirt , 1);
    // set_prob(prob_map, we_grass, we_stone, 1);
    set_prob(prob_map, we_stone, we_lava , 1); 
    // set_prob(prob_map, we_dirt , we_water, 1);

    // set_prob(prob_map, we_grass, we_grass, 100);
    // set_prob(prob_map, we_dirt , we_dirt , 100);
    // set_prob(prob_map, we_water, we_water, 100);
    // set_prob(prob_map, we_lava , we_lava , 100);
    // set_prob(prob_map, we_stone, we_stone, 100);

    // prob_map[we_grass][we_water] = 3;
    // prob_map[we_grass][we_stone] = 5;
    // // prob_map[we_stone][we_lava] = 10;
    // prob_map[we_stone][we_grass] = 10;
    // prob_map[we_water][we_water] = 50;
    // prob_map[we_water][we_grass] = 10;

    // // set_prob(prob_map, we_grass, we_stone, 50);
    // // set_prob(prob_map, we_grass, we_dirt, 30);
    // // set_prob(prob_map, we_dirt, we_water, 10);
    // // set_prob(prob_map, we_stone, we_lava , 100); 
    // // set_prob(prob_map, we_dirt , we_stone, 100);
    // // set_prob(prob_map, we_grass, we_stone, 20);
    // // set_prob(prob_map, we_grass, we_dirt , 1);
    // // set_prob(prob_map, we_grass, we_stone, 1);
    // // set_prob(prob_map, we_dirt , we_water, 1);

    if (ng_list.size == 0) {
        return std::nullopt;
    }

    var raw_els_count = 0;
    i32 prev_best_ng_idx = -1;
    for (var i = ng_list.size - 1; i != 0; --i) {
        let ng = ng_list.items[i];
        if (ng.processed == false) {
            let m = &map[ng.x + ng.y * mx];
            u32 ng_count = (*m & neighbor_count_bits) >> 16;
            ng_list.items[i].inv_entropy = __builtin_popcountl(*m & neighbor_set_bits);
            // printf("ng_count = %u, iv_entropy = %u\n", ng_count, __builtin_popcountl(*m & neighbor_set_bits));
            // best_ng_idx = i;
            if (best_ng_idx == -1) {
                best_ng_idx = i;
            } else if (ng_list.items[i].inv_entropy > ng_list.items[best_ng_idx].inv_entropy) {
                prev_best_ng_idx = best_ng_idx;
                best_ng_idx = i;
            }
            raw_els_count += 1;
            // break;
        } else if (best_ng_idx == -1) {
            ng_list.size = ng_list.size - 1;
        }
    }

    if (raw_els_count == 0) {
        return std::nullopt;
    }

    let best_neighbor = ng_list.items[best_ng_idx];
    let x = best_neighbor.x;
    let y = best_neighbor.y;

    if (map[x + y * mx] & is_processing_bit) {
    // if (true) {
        let sum = 0;
        i32 c_prob[we_max] = {100, 100, 100, 100, 100, 100};

        u32 m = map[x + y * mx] & neighbor_set_bits;

        for (var p = 0; p != we_max; ++p) {
            if (m & 1) {
                for (var q = 0; q != we_max; ++q) {
                    c_prob[q] = min(c_prob[q], prob_map[p][q]);
                    // c_prob[q] = c_prob[q] = 0 ? prob_map[p][q] : c_prob[q];
                }
            }
            m = m >> 1;
        }

        for (var p = 0; p != we_max; ++p) {
            sum += c_prob[p];
        }

        if (sum > 0) {
            i32 rn = rand() % sum;
            // printf("sum = %u, rn = %u\n", sum, rn);

            for (var i = 0; i != we_max; ++i) {
                if (c_prob[i] == 0) {
                    continue;
                } else {
                    if (rn - c_prob[i] < 0) {
                        map[x + y * mx] = i;
                        ng_list.items[best_ng_idx].processed = true;
                        free_ng_ls.push(best_ng_idx);
                        std::tie(ng_list, free_ng_ls) = update_neighbors(map, mx, my, ng_list, free_ng_ls, x, y);
                        break;
                    } else {
                        rn = rn - c_prob[i];
                    }
                }
            }
        } else {
            map[x + y * mx] = we_none;
            ng_list.items[best_ng_idx].processed = true;
            free_ng_ls.push(best_ng_idx);
        }
    }


    return optional<tuple<list<neighbor_info>, list<u32>, u32>>({ng_list, free_ng_ls, prev_best_ng_idx});
}

func init_map(u32* map, u32 mx, u32 my) -> tuple<list<neighbor_info>, list<u32>, u32> {
    for (var y = 0; y != my; ++y) {
        for (var x = 0; x != mx; ++x) {
            map[x + y * mx] = 1 << 31;
        }
    }

    let x = mx / 2;
    let y = my / 2;

    map[x + y * mx] = we_grass;

    list<neighbor_info> ng_list    = list<neighbor_info>::create(default_allocator, 256);
    list<u32>           free_ng_ls = list<u32>::create(default_allocator, 128);
    u32 best_ng_idx                = 0;
    std::tie(ng_list, free_ng_ls)  = update_neighbors(map, mx, my, ng_list, free_ng_ls, x, y);

    return {ng_list, free_ng_ls, best_ng_idx};
}

func mk_map(u32* map, u32 mx, u32 my) {
    for (var y = 0; y != my; ++y) {
        for (var x = 0; x != mx; ++x) {
            map[x + y * mx] = 1 << 31;
        }
    }

    let x = mx / 2;
    let y = my / 2;

    map[x + y * mx] = we_grass;

    list<neighbor_info> ng_list    = list<neighbor_info>::create(default_allocator, 256);
    list<u32>           free_ng_ls = list<u32>::create(default_allocator, 128);
    u32 best_ng_idx                = 0;
    std::tie(ng_list, free_ng_ls)  = update_neighbors(map, mx, my, ng_list, free_ng_ls, x, y);

    while (let res = generate_one(map, mx, my, ng_list, free_ng_ls, best_ng_idx)) {
        std::tie(ng_list, free_ng_ls, best_ng_idx) = *res;
    }
}


const u32 we_colors[we_max] = {
    0x000fff, // none
    0x5b9945, // grass
    0x332702, // dirt
    0x0b479c, // water
    0xcfa40c, // lava
    0x595854, // stone
};

func map_to_img(u32* map, u32* img_buf, u32 mx, u32 my) {
    for (var y = 0; y != my; ++y) {
        for (var x = 0; x != mx; ++x) {
            if ((map[x + y * mx] & is_processing_bit) == 0) {
                img_buf[x + y * mx] = we_colors[map[x + y * mx]];
            }
            //  else if (map[x + y * mx] & is_in_ng_list_bit) {
            //     img_buf[x + y * mx] = we_colors[__builtin_popcountl(map[x + y * mx]) + 1];
            // } 
            // else {
            //     img_buf[x + y * mx] = map[x + y * mx] >> 8;
            // }
            else if (map[x + y * mx] & is_in_ng_list_bit) {
                u32 ng_count = (map[x + y * mx] & neighbor_count_bits) >> 16;
                u32 entr = __builtin_popcountl(map[x + y * mx] & neighbor_set_bits);
                img_buf[x + y * mx] = entr << 16;
            }
        }
    }
}

// world_element solve(world_element aj[we_max], u8** k) {
//     u8 r[we_max];

//     for (var i = 0; i != we_max; ++i) {
//         let el = aj[i];
//         switch (el) {
//             case we_none: break;
//             case we_grass: {app(r, k[el])}; break;
//         }
//     }
// }