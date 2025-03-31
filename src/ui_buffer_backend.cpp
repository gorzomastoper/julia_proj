#pragma once

#include "ui.h"
#include "util/types.h"
#include "util/buffer.h"
#include "util/log.h"

struct image_buffer_b {
    rgba* data;
    u32 width;
    u32 height;
};

struct rectangle {
    u32 x;
    u32 y;
    u32 mx;
    u32 my;
};

func ui_buffer_backend_draw_rect(image_buffer_b img_buf, rectangle rc, rgba color) -> void {
    for (var y = rc.y; y < rc.my; ++y) {
        for (var x = rc.x; x < rc.mx; ++x) {
            img_buf.data[x + y * img_buf.width] = color;
        }
    }
}

// func ui_mun_to_int() {}


struct draw_ui_context {
    u32 x;
    u32 y;
    u32 mx;
    u32 my;
    u32 ui_buf_pos;
};

// struct draw_ui_result {
//     u32 width;
//     u32 height;
//     u32 ui_buf_pos;
// };

func ui_buffer_backend_draw_ui(draw_ui_context ctx, image_buffer_b img_buf, buffer ui_buf) -> u32 {
    if (ctx.ui_buf_pos == ui_buf.size) {
        return ctx.ui_buf_pos;
    } else if (ctx.ui_buf_pos >= ui_buf.size) {
        panic("ctx.ui_buf_pos >= ui_buf.size");
    }

    let data     = (u8*)ui_buf.data + ctx.ui_buf_pos;
    let draw_cmd = (ui_draw_command*)data;

    switch (draw_cmd->op) {
        case udo_invalid: { panic("udo_invalid"); };
        case udo_block: {
            let params = draw_cmd->params.udo_block;

            let width = params.width;
            let height = params.height;

            ui_buffer_backend_draw_rect(img_buf, {.x = ctx.x, .y = ctx.y, .mx = min(ctx.x + width, ctx.mx), .my = min(ctx.y + height, ctx.my)}, params.bg_color);

            ctx.mx = ctx.x + width - params.padding;
            ctx.my = ctx.y + height - params.padding;
            ctx.x = ctx.x + params.padding;
            ctx.y = ctx.y + params.padding;

            ctx.ui_buf_pos += ui_sizeof_for_tag(ui_draw_command, udo_block);

            // var cur_x = ctx.x;
            var cur_y = ctx.y;
            // while (true) {
            //     let data     = (u8*)ui_buf.data + ctx.ui_buf_pos;
            //     let draw_cmd = (ui_draw_command*)data;

            //     if (draw_cmd->op == udo_block_end) {
            //         ctx.ui_buf_pos += ui_sizeof_for_tag(ui_draw_command, udo_block_end);

            //         return ctx.ui_buf_pos;
            //     } else {
            //         ctx.y = cur_y;
            //         let res = ui_buffer_backend_draw_ui(ctx, img_buf, ui_buf);
            //         cur_y = cur_y + res.height;
            //         ctx.ui_buf_pos = res.ui_buf_pos;
            //     }
            // }
            return ui_buffer_backend_draw_ui(ctx, img_buf, ui_buf);
        }; break;
        case udo_block_end: { panic("unexpected: udo_block_end"); }; break;
        case udo_text: {
            ctx.ui_buf_pos += ui_sizeof_for_tag(ui_draw_command, udo_text);
            
            warn("udo_text is not implemented\n");
            return ui_buffer_backend_draw_ui(ctx, img_buf, ui_buf);
        }; break;
        case udo_rect: {
            let params = draw_cmd->params.udo_rect;
            ui_buffer_backend_draw_rect(img_buf, {.x = params.x, .y = params.y, .mx = min(params.mx, ctx.mx), .my = min(params.my, ctx.my)}, params.color);
            
            ctx.ui_buf_pos += ui_sizeof_for_tag(ui_draw_command, udo_rect);
            
            return ui_buffer_backend_draw_ui(ctx, img_buf, ui_buf);
        }; break;
        case udo_position_relative: {
            ctx.ui_buf_pos += ui_sizeof_for_tag(ui_draw_command, udo_position_relative);

            warn("udo_position_relative is not implemented\n");
            return ui_buffer_backend_draw_ui(ctx, img_buf, ui_buf);
        }; break;
    }

}