#pragma once
#include "util/memory_management.h"
#include <cstdio>
#include <stdlib.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui-docking/imgui.h"
#include "imgui-docking/imgui_internal.h"

#include "stnc.cpp"
#include "dmath.h"
#include "dx_backend.h"
#include <cmath>

struct node_bezier_render_info {
	v2 *start_pos;
	v2 *end_pos;
	i32 pin_idx_from;
	i32 pin_idx_to;
};

struct stnc_rendering {
	bool active_conntection;
	bool active_node_moving;
	bool active_pie_menu;
	int pin_idx_1 = -1; // NOTE(DH): -1 means there is no connection
	int pin_idx_2 = -1; // NOTE(DH): -1 means there is no connection
	u16 current_selcted_node_idx;
	f32 zoom_factor;
	f32 pin_radius;
	v2 *bezier_start_point;
	v2 *bezier_end_point;

	v2 bezier_temp_start_pos;

	ImVec2 canvas_panning;
	ImVec2 canvas_origin_screen_space;
	ImVec2 node_size;
	ImVec2 node_input_pin_pos_start;
	ImVec2 node_out_pin_pos;
	ImVec2 cursor_pos;

	ImColor unactive_pin_color;
	ImColor active_pin_color;
	ImColor unactive_node_color;
	ImColor active_node_color;

	ImColor node_bg_color;
	ImColor node_title_bg_color;
	ImColor node_text_color;
	ImColor node_bezier_color;

	f32 node_roundness;

	// NOTE(DH): Pie menu
	ImVec2 pie_menu_centre;

	// NOTE(DH): Mini map
	ImVec2 mini_map_content_size;

	arena_array<node_bezier_render_info> bezier_pairs;
	arena_ptr<struct module> mod;
};

struct bezier {
	arena_array<v2> points;
	v2 p0;
	v2 p1;
	v2 p2;
	v2 p3;
};

auto im_vec2 = [](v2 vec) -> ImVec2 {
	return ImVec2(vec.x, vec.y);
};

auto vec2_im = [](ImVec2 vec) -> v2 {
	return V2(vec.x, vec.y);
};

inline func screen_sapce_to_grid_space(stnc_rendering *stnc_rndr, ImVec2 v) -> ImVec2 {
	return v - stnc_rndr->canvas_origin_screen_space - stnc_rndr->canvas_panning;
}

inline func grid_space_to_screen_space(stnc_rendering *stnc_rndr, ImVec2 v) -> ImVec2 {
	return v + stnc_rndr->canvas_origin_screen_space + stnc_rndr->canvas_panning;
}

inline func grid_space_to_editor_space(stnc_rendering *stnc_rndr, ImVec2 v) -> ImVec2 {
	return v + stnc_rndr->canvas_panning;
}

inline func editor_space_to_grid_space(stnc_rendering *stnc_rndr, ImVec2 v) -> ImVec2 {
	return v - stnc_rndr->canvas_panning;
}

inline func editor_space_to_screen_space(stnc_rendering *stnc_rndr, ImVec2 v) -> ImVec2 {
	return stnc_rndr->canvas_origin_screen_space + v;
}

// inline func screen_space_to_mini_map_space(stnc_rendering *stnc_rndr, ImVec2 v) -> ImVec2 {
// 	return (screen_sapce_to_grid_space(stnc_rndr, v) - );
// }

inline func init_stnc_rndr(memory_arena *arena, arena_ptr<struct module> _mod) -> stnc_rendering {
	stnc_rendering result = {};
	result.mod = _mod;
	result.current_selcted_node_idx = 0xFFFF;
	result.canvas_panning = {0.0f, 0.0f};
	result.zoom_factor = 1.0f;
	result.node_size = ImVec2(300, 200);
	result.pin_radius = 10.0f;
	result.unactive_pin_color = IM_COL32(128, 128, 128, 255);
	result.active_pin_color = IM_COL32(128, 164, 128, 255);
	result.unactive_node_color = IM_COL32(128, 128, 128, 255);
	result.active_node_color = IM_COL32(210, 210, 210, 255);
	result.active_node_moving = false;
	result.active_conntection = false;

	result.node_bg_color = IM_COL32(64, 64, 128, 255);
	result.node_title_bg_color = IM_COL32(128, 128, 128, 255);
	result.node_bezier_color = IM_COL32(192, 192, 192, 255);
	result.node_roundness = 10.0f;

	result.node_out_pin_pos = ImVec2(result.node_size.x, result.node_size.y / 3);
	result.node_input_pin_pos_start = ImVec2(0, result.node_size.y / 4);
	
	result.bezier_pairs = arena->alloc_array<node_bezier_render_info>(64); // TODO(DH): Remake this to proper memory allocation strat

	return result;
}

inline func cubic_bezier(v2 start, v2 control_point_1, v2 control_point_2, v2 end_point, f32 t) -> v2 {
	f32 u = 1.0f - t;
	f32 tt = t * t;
	f32 uu = u * u;
	f32 uuu = uu * u;
	f32 ttt = tt * t;

	v2 p = uuu * start;
	p += 3 * uu * t * control_point_1;
	p += 3 * u * tt * control_point_2;
	p += ttt * end_point;

	return p;
}

inline func de_casteljau(v2 p0, v2 p1, v2 p2, v2 p3, f32 t) -> v2 {
	v2 p01 = p0 * (1.0f - t) + p1 * t;
	v2 p12 = p1 * (1.0f - t) + p2 * t;
	v2 p23 = p2 * (1.0f - t) + p3 * t;
	v2 p012 = p01 * (1.0f - t) + p12 * t;
	v2 p123 = p12 * (1.0f - t) + p23 * t;
	return p012 * (1.0f - t) + p123 * t;
}

inline func calculate_control_point(v2 start, v2 end, f32 tension) -> v2 {
	v2 direction = end - start;
	return start + direction * tension;
}

// inline func calculate_num_segments(v2 start, v2 control_point_1, v2 control_point_2, v2 end_point, f32 tolerance, v2 prev, v2 next) -> u32 {
// 	// v2 mid = cubic_bezier(start, control_point_1, control_point_2, end_point, 0.5);
// 	v2 mid = de_casteljau(start, control_point_1, control_point_2, end_point, 0.5f);
// 	f32 error = fmax(Distance(mid, start), Distance(mid, end_point));

// 	if(error <= tolerance) return 1;

// 	u32 left_segments = calculate_num_segments(start, control_point_1, mid, mid, tolerance, prev, start);
// 	u32 right_segments = calculate_num_segments(mid, mid, control_point_2, end_point, tolerance, end_point, next);

// 	return left_segments + right_segments;
// }

inline func calculate_num_segments(v2 start, v2 control_point_1, v2 control_point_2, v2 end_point) -> u32 {
	f32 estimatel_length = (Length(control_point_1 - start) + Length(control_point_2 - control_point_1) + Length(end_point - control_point_2));

	return (u32)ceil(estimatel_length);
}

inline func update_bezier(memory_arena arena, bezier cubic, v2 canvas_position, v2 p0, v2 p3, f32 tension) -> bezier {
	f32 dist = Distance(p0 + canvas_position, p3);
	v2 control_point_1 = calculate_control_point(p0 + canvas_position, (p0 + canvas_position + V2(1, 0)), tension * dist);
	v2 control_point_2 = calculate_control_point((p3 + V2(1, 0)), p3, tension * dist);

	// u32 num_segments = calculate_num_segments(start_point, control_point_1, control_point_2, end_point, 0.02f, start_point, end_point);
	u32 num_segments = fmin((calculate_num_segments(p0, control_point_1, control_point_2, p3) / 8), cubic.points.count);

	auto points_array = arena.get_array(cubic.points);

	for(u32 i = 0; i < num_segments; ++i) {
		f32 t = (f32)i / f32(num_segments);
		points_array[i] = cubic_bezier(p0, control_point_1, control_point_2, p3, t);
	}

	cubic.points.count = num_segments + 1;
	cubic.p0 = p0;
	cubic.p1 = control_point_1;
	cubic.p2 = control_point_2;
	cubic.p3 = p3;
	
	if(num_segments <= 2) return cubic;

	points_array[cubic.points.count - 1] = p3;
	return cubic;
}

inline func create_bezier(memory_arena *arena, v2 start_point, v2 end_point, f32 tension) -> bezier {
	struct bezier result = {};

	f32 dist = Distance(start_point, end_point);
	v2 control_point_1 = calculate_control_point(start_point, (start_point + V2(1, 0)), tension * dist);
	v2 control_point_2 = calculate_control_point((end_point + V2(1, 0)), end_point, tension * dist);

	u32 num_segments = calculate_num_segments(start_point, control_point_1, control_point_2, end_point) * 0.03f;

	result.points = arena->alloc_array<v2>(num_segments);
	result.points.count = num_segments + 1;
	auto points_array = arena->get_array(result.points);

	for(u32 i = 0; i < num_segments; ++i) {
		f32 t = (f32)i / f32(num_segments);
		points_array[i] = cubic_bezier(start_point, control_point_1, control_point_2, end_point, t);
	}

	points_array[result.points.count - 1] = end_point;

	result.p0 = start_point;
	result.p1 = control_point_1;
	result.p2 = control_point_2;
	result.p3 = end_point;

	return result;
}

inline func create_bezier(memory_arena *arena, v2 start_point, v2 end_point) -> bezier {
	struct bezier result = {};

	f32 dist = Distance(start_point, end_point);
	v2 control_point_1 = calculate_control_point(start_point, (start_point + V2(1, 0)), 0.35f * dist);
	v2 control_point_2 = calculate_control_point((end_point + V2(1, 0)), end_point, 0.35f * dist);

	result.p0 = start_point;
	result.p1 = control_point_1;
	result.p2 = control_point_2;
	result.p3 = end_point;

	return result;
}

inline func imgui_draw_bezier(stnc_rendering *stnd_rndr, memory_arena arena, bezier cubic, ImDrawList *canvas_draw_list) -> void {
	auto points_array = arena.get_array(cubic.points);
	
	// if(cubic.points.count > 2) {
		// ImGui::Begin("Bezier Example");

		//NOTE(DH): Make an arrow
		// f32 arrow_size = 10.0f;
		// v2 dir = cubic.p3 - cubic.p2;
		// dir = normalize(dir);
		// v2 perpendicular = V2(-dir.y, dir.x);
		// v2 p1 = points_array[cubic.points.count-1];
		// v2 p2 = points_array[cubic.points.count-1] - (dir * arrow_size);
		// v2 p3 = p2 + perpendicular * arrow_size * 0.5f;
		// v2 p4 = p2 - perpendicular * arrow_size * 0.5f;
		// NOTE(DH): End

		// ImDrawList *list = ImGui::GetWindowDrawList();
		// list->AddPolyline((ImVec2*)points_array, cubic.points.count, IM_COL32(255,0, 0, 255), ImDrawFlags_None, 2.0f);
		canvas_draw_list->AddBezierCubic(im_vec2(cubic.p0), im_vec2(cubic.p1), im_vec2(cubic.p2), im_vec2(cubic.p3), stnd_rndr->node_bezier_color, 2.0f * stnd_rndr->zoom_factor);
		// list->AddTriangleFilled(im_vec2(p1), im_vec2(p3), im_vec2(p4), IM_COL32(255, 0, 0, 255));
		// list->AddCircle(im_vec2(cubic.p1), 4.0f, IM_COL32(255, 0, 0, 255), 32, 4.0f);
		// list->AddCircle(im_vec2(cubic.p2), 4.0f, IM_COL32(255, 255, 0, 255), 32, 4.0f);

		// list->AddCircle(im_vec2(p3), 4.0f, IM_COL32(255, 0, 0, 255), 32, 4.0f);
		// list->AddCircle(im_vec2(p1), 4.0f, IM_COL32(0, 255, 0, 255), 32, 4.0f);
		// list->AddCircle(im_vec2(p4), 4.0f, IM_COL32(255, 255, 0, 255), 32, 4.0f);

		// list->AddLine(ImVec2(points_array[0].x, points_array[0].y), ImVec2(points_array[125].x, points_array[125].y), IM_COL32(255,0, 0, 255));

		// ImGui::End();
	// }
}

inline func is_mouse_hovering_pin(ImVec2 pin_pos, f32 radius) -> bool {
	v2 mouse_pos = vec2_im(ImGui::GetMousePos());
	v2 pin_pos_v2 = vec2_im(pin_pos);
	f32 dist = Distance(mouse_pos, pin_pos_v2);

	if(dist <= radius) {
		return true;
	}

	return false;
}

inline func calculate_mouse_pie_angle(ImVec2 mouse_pos, ImVec2 centre) -> f32 {
	f32 dx = mouse_pos.x - centre.x;
	f32 dy = mouse_pos.y - centre.y;

	return atan2(dy, dx);
}

inline func draw_pie_menu(ImDrawList *draw_list, ImVec2 pie_menu_pos, f32 dist_threshold, char ** menu_items, u32 menu_items_count) -> u8 {
	u8 result = 0xFF;
	char buff[32];
	const f32 PI = 3.1415926f;

	f32 increment = 2.0 * PI / menu_items_count;
	f32 radius = 100.0f;

	bool is_we_out_threshold = Distance(vec2_im(ImGui::GetMousePos()), vec2_im(pie_menu_pos)) > dist_threshold;
	draw_list->AddCircleFilled(pie_menu_pos, radius * 1.3f, IM_COL32(0, 0, 64, 128));

	// NOTE(DH): Calculate where mouse is in the pie
	f32 mouse_angle_in_radians = calculate_mouse_pie_angle(ImGui::GetMousePos(), pie_menu_pos);
	if(mouse_angle_in_radians < 0) {
		mouse_angle_in_radians += 2.0f * PI;
	}

	result = is_we_out_threshold ? (mouse_angle_in_radians / increment) : result;

	if(result >= menu_items_count && is_we_out_threshold)
		result = 0;

	// NOTE(DH): Draw pie menu
	u32 item_idx = 0;
	for(float curr_angle = 0.0f; curr_angle < 2.0 * PI; curr_angle += increment)
	{
		ImColor text_color = item_idx == result ? IM_COL32(255, 255, 0, 255) : IM_COL32_WHITE;

		ImVec2 centre = ImVec2(radius * cos(curr_angle) + pie_menu_pos.x, radius * sin(curr_angle) + pie_menu_pos.y);

		draw_list->AddCircle(centre, 5.0f, IM_COL32_WHITE);
		draw_list->AddLine(centre, pie_menu_pos, IM_COL32_BLACK);

		ImVec2 text_pos = ImVec2(radius * cos(curr_angle + increment / 2.0f) + pie_menu_pos.x, radius * sin(curr_angle + increment / 2.0f) + pie_menu_pos.y);
		ImVec2 text_size = ImGui::CalcTextSize(menu_items[item_idx]);

		draw_list->AddText(text_pos - text_size / 2, text_color, menu_items[item_idx]);

		++item_idx;
	}
	draw_list->AddLine(pie_menu_pos, ImGui::GetMousePos(), IM_COL32(0, 210, 0, 255));

	return result;
}

// inline func draw_pie_menu(ImDrawList *draw_list, ImVec2 pie_menu_pos, f32 dist_threshold, char ** menu_items, u32 menu_items_count) -> u32 {
// 	u32 result = 0xFFFE;
// 	char buff[32];
// 	const f32 PI = 3.1415926f;

// 	f32 increment = 2.0 * PI / menu_items_count;
// 	f32 radius = 100.0f;
// 	f32 angle_threshhold = 180.0f / (f32)menu_items_count;

// 	draw_list->AddCircleFilled(pie_menu_pos, radius * 1.3f, IM_COL32(0, 0, 64, 128));

// 	u32 item_idx = 0;
// 	// for(float curr_angle = 4.0 * (PI / 2.0f); curr_angle > PI / 2.0f; curr_angle -= increment)
// 	for(float curr_angle = 0.0f; curr_angle < 2.0 * PI; curr_angle += increment)
// 	{
// 		ImColor text_color = IM_COL32_WHITE;

// 		ImVec2 centre = ImVec2(radius * cos(curr_angle) + pie_menu_pos.x, radius * sin(curr_angle) + pie_menu_pos.y);
// 		draw_list->AddCircle(centre, 5.0f, IM_COL32_WHITE);
// 		draw_list->AddLine(centre, pie_menu_pos, IM_COL32_BLACK);

// 		ImVec2 text_pos = ImVec2(radius * cos(curr_angle + increment / 2.0f) + pie_menu_pos.x, radius * sin(curr_angle + increment / 2.0f) + pie_menu_pos.y);
// 		ImVec2 text_size = ImGui::CalcTextSize(menu_items[item_idx]);

// 		v2 normalized_mouse_dir = normalize(vec2_im(ImGui::GetMousePos()) - vec2_im(pie_menu_pos));
// 		v2 normalized_text_dir = normalize(vec2_im(text_pos) - vec2_im(pie_menu_pos));
// 		f32 angle = ((1.0f - (Inner(normalized_mouse_dir, normalized_text_dir))) / 2.0f) * 180.0f;

// 		if(angle < angle_threshhold) {
// 			result = item_idx;
// 			text_color = IM_COL32(255, 255, 0, 255);
// 		}

// 		draw_list->AddText(text_pos - text_size / 2, text_color, menu_items[item_idx]);

// 		snprintf(buff, _countof(buff), "angle: %.2f", angle);
// 		draw_list->AddText(centre, IM_COL32_WHITE, buff);

// 		++item_idx;
// 	}

// 	draw_list->AddLine(pie_menu_pos, ImGui::GetMousePos(), IM_COL32(0, 210, 0, 255));

// 	return result;
// }

inline func draw_pin(ImColor color, f32 radius, dx_context *ctx, stnc_rendering *stnc_rndr, char* label, ImDrawList* draw_list, ImVec2 pin_pos, u32 node_idx, bool left_corner) {
	draw_list->AddCircleFilled(pin_pos, radius * stnc_rndr->zoom_factor, color, 32);

	f32 zoomed_pin_size = (stnc_rndr->pin_radius * stnc_rndr->zoom_factor);
	ImVec2 text_size = ImGui::CalcTextSize(label) * stnc_rndr->zoom_factor;
	ImVec2 text_pos = left_corner ? ImVec2(pin_pos.x + zoomed_pin_size * 2, pin_pos.y - zoomed_pin_size / 2) : ImVec2(pin_pos.x - text_size.x - zoomed_pin_size * 2, pin_pos.y - zoomed_pin_size / 2);
	draw_list->AddText(text_pos, IM_COL32(210, 210, 210, 255), label);
}

inline func draw_node_title(ImVec2 node_pos, stnc_rendering *stnc_rndr, ImDrawList* draw_list) -> void {
	char test_label[] = "TEST NODE LABEL";
	ImVec2 text_size = ImGui::CalcTextSize(test_label) * stnc_rndr->zoom_factor;
	f32 node_width = stnc_rndr->node_size.x * stnc_rndr->zoom_factor;
	ImVec2 text_pos = ImVec2((node_pos.x + (node_width / 2) - (text_size.x / 2)), (node_pos.y + (40.0f / 2) * stnc_rndr->zoom_factor) - (text_size.y / 2));
	draw_list->AddText(text_pos, IM_COL32(210, 210, 210, 255), test_label);
}

inline func draw_node(dx_context *ctx, stnc_rendering *stnc_rndr, ImDrawList* canvas_draw_list, ImGuiIO io, ImVec2 canvas_p0, ImVec2 canvas_p1, node *nd, u32 node_idx) {
	bool 	is_node_need_to_be_on_top 	= false;
	bool	is_node_selected 			= (stnc_rndr->current_selcted_node_idx == node_idx);
	ImVec2 	node_size 					= stnc_rndr->node_size * stnc_rndr->zoom_factor;
	f32 	circle_radius 				= stnc_rndr->pin_radius * stnc_rndr->zoom_factor;
	v2 		child_moving 				= V2(nd->pos[0] * stnc_rndr->zoom_factor, nd->pos[1] * stnc_rndr->zoom_factor);
	v2		bezier_start_point 			= V2(node_size.x, node_size.y / 3);
	
	ImVec2 node_pos = canvas_p0 + im_vec2(child_moving) + stnc_rndr->canvas_panning;
	ImGui::SetCursorPos(node_pos);
	
	if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(node_pos, node_pos + node_size)) {
		stnc_rndr->current_selcted_node_idx = node_idx;
		stnc_rndr->active_node_moving = true;
	}

	if(ImGui::IsMouseHoveringRect(node_pos, node_pos + node_size)) {
		if(ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) && !stnc_rndr->active_pie_menu) {
			stnc_rndr->active_pie_menu = true;
			stnc_rndr->pie_menu_centre = ImGui::GetMousePos();
		}
	}

	if(is_node_selected && stnc_rndr->active_node_moving && !stnc_rndr->active_conntection) {
		nd->pos[0] += io.MouseDelta.x * 1.0f / stnc_rndr->zoom_factor;
		nd->pos[1] += io.MouseDelta.y * 1.0f / stnc_rndr->zoom_factor;
		
		is_node_need_to_be_on_top = true;
	}
	canvas_draw_list->AddRectFilled(node_pos, node_pos + node_size, stnc_rndr->node_bg_color, 10.0f);
	
	ImColor col_white = IM_COL32(0, 128, 255, 255);
	ImColor col_black = IM_COL32(0, 0, 0, 255);
	int vtx_idx_0 = canvas_draw_list->VtxBuffer.Size;
	canvas_draw_list->AddRectFilled(node_pos, node_pos + ImVec2(node_size.x, 40 * stnc_rndr->zoom_factor), col_white, stnc_rndr->node_roundness, ImDrawFlags_RoundCornersTop);
	int vtx_idx_1 = canvas_draw_list->VtxBuffer.Size;
	ImGui::ShadeVertsLinearColorGradientKeepAlpha(canvas_draw_list, vtx_idx_0, vtx_idx_1, node_pos, node_pos + ImVec2(node_size.x, 40 * stnc_rndr->zoom_factor), col_white, col_black);

	draw_node_title(node_pos, stnc_rndr, canvas_draw_list);
	canvas_draw_list->AddRect(node_pos - ImVec2(1,1), node_pos + ImVec2(1,1) + node_size, is_node_selected ? stnc_rndr->active_node_color : stnc_rndr->unactive_node_color, 10.0f, 0, 2.0f);

	v2 node_position = V2(canvas_p0.x + stnc_rndr->canvas_panning.x + child_moving.x, canvas_p0.y + stnc_rndr->canvas_panning.y + child_moving.y);
	ImVec2 out_pin_pos_to_node = im_vec2(node_position + bezier_start_point);

	bool highlight_pin = (stnc_rndr->active_conntection && stnc_rndr->current_selcted_node_idx == node_idx);
	ImVec2 out_pin_pos = out_pin_pos_to_node;
	draw_pin(stnc_rndr->unactive_pin_color, stnc_rndr->pin_radius, ctx, stnc_rndr, "Output", canvas_draw_list, out_pin_pos, node_idx, false);

	if(is_mouse_hovering_pin(out_pin_pos_to_node, circle_radius * 1.5f)) {
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			stnc_rndr->active_conntection = true;
			stnc_rndr->current_selcted_node_idx = node_idx;
			stnc_rndr->bezier_temp_start_pos = vec2_im(out_pin_pos_to_node);
			stnc_rndr->bezier_start_point = (v2*)nd->pos;
			stnc_rndr->pin_idx_1 = 0;
		}

		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && stnc_rndr->pin_idx_1 != 0) {
			if(stnc_rndr->current_selcted_node_idx != node_idx) {
				stnc_rndr->pin_idx_2 = 0;
				stnc_rndr->bezier_end_point = (v2*)nd->pos;
			}
		}

		if(stnc_rndr->active_conntection && stnc_rndr->pin_idx_1 != 0) {
			stnc_rndr->cursor_pos = out_pin_pos_to_node;
			draw_pin(stnc_rndr->node_bezier_color, stnc_rndr->pin_radius / 2, ctx, stnc_rndr, "", canvas_draw_list, out_pin_pos_to_node, node_idx, true);
		}
	}

	ImVec2 input_pin_1 = im_vec2(node_position) + ImVec2(0, stnc_rndr->node_input_pin_pos_start.y * stnc_rndr->zoom_factor);
	highlight_pin = is_mouse_hovering_pin(input_pin_1, circle_radius * 1.5f);
	draw_pin(stnc_rndr->unactive_pin_color, stnc_rndr->pin_radius, ctx, stnc_rndr, "Base Input 1", canvas_draw_list, input_pin_1, node_idx, true);

	if(is_mouse_hovering_pin(input_pin_1, circle_radius * 1.5f)) {
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			stnc_rndr->active_conntection = true;
			stnc_rndr->current_selcted_node_idx = node_idx;
			stnc_rndr->pin_idx_1 = 1;
			stnc_rndr->bezier_temp_start_pos = vec2_im(input_pin_1);
			stnc_rndr->bezier_start_point = (v2*)nd->pos;
		}

		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && stnc_rndr->pin_idx_1 == 0) {
			if(stnc_rndr->current_selcted_node_idx != node_idx) {
				stnc_rndr->pin_idx_2 = 1;
				stnc_rndr->bezier_end_point = (v2*)nd->pos;
			}
		}

		if(stnc_rndr->active_conntection && stnc_rndr->pin_idx_1 == 0) {
			stnc_rndr->cursor_pos = input_pin_1;
			draw_pin(stnc_rndr->node_bezier_color, stnc_rndr->pin_radius / 2, ctx, stnc_rndr, "", canvas_draw_list, input_pin_1, node_idx, true);
		}
	}
	
	ImVec2 input_pin_2 = im_vec2(node_position) + ImVec2(0, (stnc_rndr->node_input_pin_pos_start.y + stnc_rndr->pin_radius * 3) * stnc_rndr->zoom_factor);
	highlight_pin = is_mouse_hovering_pin(input_pin_2, circle_radius * 1.5f);
	draw_pin(stnc_rndr->unactive_pin_color, stnc_rndr->pin_radius, ctx, stnc_rndr, "Base Input 2", canvas_draw_list, input_pin_2, node_idx, true);

	if(is_mouse_hovering_pin(input_pin_2, circle_radius * 1.5f)) {
		if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			stnc_rndr->active_conntection = true;
			stnc_rndr->current_selcted_node_idx = node_idx;
			stnc_rndr->pin_idx_1 = 2;
			stnc_rndr->bezier_temp_start_pos = vec2_im(input_pin_2);
			stnc_rndr->bezier_start_point = (v2*)nd->pos;
		}

		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && stnc_rndr->pin_idx_1 == 0) {
			if(stnc_rndr->current_selcted_node_idx != node_idx) {
				stnc_rndr->pin_idx_2 = 2;
				stnc_rndr->bezier_end_point = (v2*)nd->pos;
			}
		}

		if(stnc_rndr->active_conntection && stnc_rndr->pin_idx_1 == 0) {
			stnc_rndr->cursor_pos = input_pin_2;
			draw_pin(stnc_rndr->node_bezier_color, stnc_rndr->pin_radius / 2, ctx, stnc_rndr, "", canvas_draw_list, input_pin_2, node_idx, true);
		}
	}

	return is_node_need_to_be_on_top;
}

func imgui_draw_canvas(dx_context *ctx, stnc_rendering *stnc_rndr)
{
	bool enabled = true;
	v2 canvas_position = {};
	if(ImGui::Begin("Node editor", &enabled, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
		// ImGui::SetCursorPos(ImVec2(0, 0));
        static ImVector<ImVec2> points;
        static bool opt_enable_context_menu = true;
        static bool adding_line = false;
		ImGuiIO& io = ImGui::GetIO();
		f32 old_zoom = stnc_rndr->zoom_factor;
		stnc_rndr->zoom_factor += io.MouseWheel * 0.01f;
		stnc_rndr->zoom_factor = std::fmax(0.01f, stnc_rndr->zoom_factor);

		f32 angle = Inner(V2(0, -1), normalize(vec2_im(ImGui::GetMousePos())));
		ImGui::Text("Angle: %f", angle);
		ImGui::Text("Zoom: %f", stnc_rndr->zoom_factor);
		ImGui::Text("This is canvas position: %f, %f", ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
		

        // Typically you would use a BeginChild()/EndChild() pair to benefit from a clipping region + own scrolling.
        // Here we demonstrate that this can be replaced by simple offsetting + custom drawing + PushClipRect/PopClipRect() calls.
        // To use a child window instead we could use, e.g:
        //      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));      // Disable padding
        //      ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));  // Set a background color
        //      ImGui::BeginChild("canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove);
        //      ImGui::PopStyleColor();
        //      ImGui::PopStyleVar();
        //      [...]
        //      ImGui::EndChild();

        // Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
		stnc_rndr->canvas_origin_screen_space = ImGui::GetCursorScreenPos();
        ImVec2 canvas_p0 = stnc_rndr->canvas_origin_screen_space;      // ImDrawList API uses screen coordinates!
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
        if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

		// ImVec2 diff = {0.f, 0.f};
		// if(io.MouseWheel != 0) {
		// 	diff = (ImGui::GetMousePos() - (canvas_p0 + (canvas_sz / 2)));
		// 	ImVec2 pan_zoom_correction = (diff / old_zoom) - (diff / stnc_rndr->zoom_factor);
		// 	stnc_rndr->canvas_panning = io.MouseWheel > 0 ? stnc_rndr->canvas_panning - pan_zoom_correction : stnc_rndr->canvas_panning + pan_zoom_correction;
		// 	printf("\nPan correction: %f, %f", pan_zoom_correction.x, pan_zoom_correction.y);
		// }

        // Draw border and background color
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

        // This will catch our interactions
        ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool is_hovered = ImGui::IsItemHovered(); // Hovered
        const bool is_active = ImGui::IsItemActive();   // Held
        const ImVec2 origin = canvas_p0 + stnc_rndr->canvas_panning; // Lock scrolled origin
        const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

        // Pan (we use a zero mouse threshold when there's no context menu)
        // You may decide to make that threshold dynamic based on whether the mouse is hovering something etc.
        const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
        if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
        {
            stnc_rndr->canvas_panning.x += io.MouseDelta.x;
            stnc_rndr->canvas_panning.y += io.MouseDelta.y;
        }

        // Context menu (under default mouse threshold)
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        if (opt_enable_context_menu && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
            ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
        if (ImGui::BeginPopup("context"))
        {
            if (adding_line)
                points.resize(points.size() - 2);
            adding_line = false;
            if (ImGui::MenuItem("Remove one", NULL, false, points.Size > 0)) { points.resize(points.size() - 2); }
            if (ImGui::MenuItem("Remove all", NULL, false, points.Size > 0)) { points.clear(); }
            ImGui::EndPopup();
        }

        // Draw grid + all lines in the canvas
		draw_list->PushClipRect(canvas_p0, canvas_p1, true);
        {
            const float GRID_STEP = 128.0f;
            for (float x = fmodf(stnc_rndr->canvas_panning.x, GRID_STEP * stnc_rndr->zoom_factor); x < canvas_sz.x; x += GRID_STEP  * stnc_rndr->zoom_factor)
                draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
            for (float y = fmodf(stnc_rndr->canvas_panning.y, GRID_STEP * stnc_rndr->zoom_factor); y < canvas_sz.y; y += GRID_STEP * stnc_rndr->zoom_factor)
                draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(200, 200, 200, 40));
        }
		draw_list->PopClipRect();

		draw_list->PushClipRect(canvas_p0 + ImVec2(1,1), canvas_p1 - - ImVec2(1,1), true);

		//NOTE(DH): draw bezier lines
		node_bezier_render_info *bezier_pairs = ctx->mem_arena.get_array(stnc_rndr->bezier_pairs);
		for(u32 i = 0; i < stnc_rndr->bezier_pairs.count; ++i) {
			bool is_pos_start_out = (bezier_pairs[i].pin_idx_from == 0);
			v2 pin_pos_start = (is_pos_start_out ? *bezier_pairs[i].start_pos : *bezier_pairs[i].end_pos) + vec2_im(is_pos_start_out ? stnc_rndr->node_out_pin_pos : stnc_rndr->node_input_pin_pos_start + ImVec2(0, (stnc_rndr->pin_radius * 3) * (bezier_pairs[i].pin_idx_from - 1)));
			v2 pin_pos_end = (is_pos_start_out ? *bezier_pairs[i].end_pos : *bezier_pairs[i].start_pos) + vec2_im(is_pos_start_out ? stnc_rndr->node_input_pin_pos_start + ImVec2(0, (stnc_rndr->pin_radius * 3) * (bezier_pairs[i].pin_idx_to - 1)) : stnc_rndr->node_out_pin_pos);
			
			// NOTE(DH): We need to flip the order of positions, for proper bezier rendering
			if(!is_pos_start_out) {
				v2 tmp = pin_pos_end;
				pin_pos_end = pin_pos_start;
				pin_pos_start = tmp;
			}
			
			bezier new_bezier = create_bezier(&ctx->mem_arena, pin_pos_start * stnc_rndr->zoom_factor + vec2_im(origin), pin_pos_end * stnc_rndr->zoom_factor + vec2_im(origin));
			imgui_draw_bezier(stnc_rndr, ctx->mem_arena, new_bezier, draw_list);
		}
		//NOTE(DH) END

		bool swap = false;
		u32 swap_node_idx = 0;
		let mod = ctx->mem_arena.get_ptr(stnc_rndr->mod);
		mod->defs.iter_in_order([=, &swap, &swap_node_idx](definition *def, u32 def_idx, u32 ordered_def_idx) -> void {
			if(def->tag == definition::node) {

				// NOTE(DH): Swap elements only on next iteration for proper drawing!
				def->data.node.nodes.iter([=, &swap, &swap_node_idx](node *nd) -> void {
					if(draw_node(ctx, stnc_rndr, draw_list, io, canvas_p0, canvas_p1, nd, ordered_def_idx) == true) {
						swap = true;
						swap_node_idx = def_idx;
					}
				});

				def->data.node.links.iter([](link *lnk) -> void {
					//lnk->
				});
			}
		});
		draw_list->PopClipRect();

		if(swap_node_idx != 0 && swap) mod->defs.swap_to_last(swap_node_idx);

			// NOTE(DH): Clear all interaction things
		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			// stnc_rndr->current_selcted_node_idx = 0xFFFF;
			if(stnc_rndr->pin_idx_1 != -1 && stnc_rndr->pin_idx_2 != -1 && stnc_rndr->pin_idx_1 != stnc_rndr->pin_idx_2) {
				node_bezier_render_info *bezier_pairs = ctx->mem_arena.get_array(stnc_rndr->bezier_pairs);
				bool is_dst_output = stnc_rndr->pin_idx_2 == 0;
				bezier_pairs[stnc_rndr->bezier_pairs.count].start_pos = is_dst_output ? stnc_rndr->bezier_end_point : stnc_rndr->bezier_start_point;
				bezier_pairs[stnc_rndr->bezier_pairs.count].end_pos = is_dst_output ? stnc_rndr->bezier_start_point : stnc_rndr->bezier_end_point;
				bezier_pairs[stnc_rndr->bezier_pairs.count].pin_idx_from = stnc_rndr->pin_idx_1;
				bezier_pairs[stnc_rndr->bezier_pairs.count].pin_idx_to = stnc_rndr->pin_idx_2;
				++stnc_rndr->bezier_pairs.count;
				stnc_rndr->pin_idx_1 = -1;
				stnc_rndr->pin_idx_2 = -1;
			}
			stnc_rndr->active_node_moving = false;
			stnc_rndr->active_conntection = false;
			printf("\nReleased");
		}

		// NOTE(DH): Draw bezier {
		if(stnc_rndr->active_conntection) {
			v2 mouse_pos = V2(stnc_rndr->cursor_pos.x, stnc_rndr->cursor_pos.y);
			v2 pin_pos = stnc_rndr->bezier_temp_start_pos;
			bool input_or_output = stnc_rndr->pin_idx_1 > 0;
			bezier visualization_bezier = input_or_output ? create_bezier(&ctx->mem_arena, mouse_pos, pin_pos) : create_bezier(&ctx->mem_arena, pin_pos, mouse_pos);
			imgui_draw_bezier(stnc_rndr, ctx->mem_arena, visualization_bezier, draw_list);
			draw_pin(stnc_rndr->node_bezier_color, stnc_rndr->pin_radius / 2, ctx, stnc_rndr, "", draw_list, im_vec2(pin_pos), stnc_rndr->current_selcted_node_idx, false);
		}

		char *pie_menu_items[] {"Add", "Move", "Delete", "Show"};//, "Delete", "Show", "BLock"};

		if(stnc_rndr->active_pie_menu && !ImGui::IsKeyReleased(ImGuiKey_LeftCtrl,ImGuiInputFlags_LockUntilRelease)) {
			u32 idx = draw_pie_menu(draw_list, stnc_rndr->pie_menu_centre, 10.0f, pie_menu_items, _countof(pie_menu_items));
			printf("\n%u", idx);
		} else {
			stnc_rndr->active_pie_menu = false;
		}

		stnc_rndr->cursor_pos = ImGui::GetMousePos(); // NOTE(DH): Update cursor position
		// NOTE(DH): Draw bezier }

		// draw_list->AddCircle((canvas_p0 + (canvas_sz / 2)), 12, IM_COL32(0, 255, 0, 255));
		
    }
	ImGui::End();

	return canvas_position;
}