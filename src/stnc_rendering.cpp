#pragma once
#include "util/memory_management.h"
#include <cstdio>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui-docking/imgui.h"

#include "stnc.cpp"
#include "dmath.h"
#include "dx_backend.h"
#include <cmath>

struct stnc_rendering {
	bool active_conntection;
	v2 start_point;

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

inline func imgui_draw_bezier(memory_arena arena, bezier cubic) -> void {
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

		ImDrawList *list = ImGui::GetWindowDrawList();
		// list->AddPolyline((ImVec2*)points_array, cubic.points.count, IM_COL32(255,0, 0, 255), ImDrawFlags_None, 2.0f);
		list->AddBezierCubic(im_vec2(cubic.p0), im_vec2(cubic.p1), im_vec2(cubic.p2), im_vec2(cubic.p3), IM_COL32(192, 192, 192, 255), 2.0f);
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

func imgui_draw_canvas(dx_context *ctx, stnc_rendering *stnc_rndr)
{
	bool enabled = true;
	v2 canvas_position = {};
	if(ImGui::Begin("Node editor", &enabled, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
		// ImGui::SetCursorPos(ImVec2(0, 0));
        static ImVector<ImVec2> points;
        static ImVec2 scrolling(0.0f, 0.0f);
        static bool opt_enable_context_menu = true;
        static bool adding_line = false;
		ImGuiIO& io = ImGui::GetIO();

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
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
        if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

        // Draw border and background color
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

        // This will catch our interactions
        ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool is_hovered = ImGui::IsItemHovered(); // Hovered
        const bool is_active = ImGui::IsItemActive();   // Held
        const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y); // Lock scrolled origin
        const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

        // Pan (we use a zero mouse threshold when there's no context menu)
        // You may decide to make that threshold dynamic based on whether the mouse is hovering something etc.
        const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
        if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
        {
            scrolling.x += io.MouseDelta.x;
            scrolling.y += io.MouseDelta.y;
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
            for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x; x += GRID_STEP)
                draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
            for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y; y += GRID_STEP)
                draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(200, 200, 200, 40));
        }

		draw_list->PopClipRect();

		//NOTE(DH): draw bezier lines
		
		//NOTE(DH) END

		let render_node = [](dx_context* ctx, stnc_rendering *stnc_rndr, ImVec2 canvas_p0, ImVec2 canvas_p1, ImGuiIO io, v2 *child_moving) {
			// printf("Pos: %f, %f\n", child_moving->x, child_moving->y);
			ImVec2 node_size = ImVec2(300, 200);
			f32 circle_radius = 10.0f;
			stnc_rndr->start_point = V2(node_size.x - 20, node_size.y / 3);

			char tmp[128];
			ImGui::SetNextWindowPos(canvas_p0 + im_vec2(*child_moving) + scrolling);
			ImGui::PushClipRect(canvas_p0 + ImVec2(1,1), canvas_p1 - ImVec2(1,1), false);
			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 0, 0, 255));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			snprintf(tmp, sizeof(tmp), "BTN_%u", (u32)child_moving);
			ImGui::InvisibleButton(tmp, node_size, ImGuiButtonFlags_MouseButtonLeft);
			snprintf(tmp, sizeof(tmp), "CNV_%u", (u32)child_moving);
			ImGui::BeginChild(tmp, node_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);
			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			if (ImGui::IsItemActive())
			{
				child_moving->x += io.MouseDelta.x;
				child_moving->y += io.MouseDelta.y;
			}

			if (ImGui::IsItemActivated())
			{
				printf("SOL\n");
			}

			ImGui::SetCursorPos(im_vec2(stnc_rndr->start_point) - ImVec2(circle_radius, circle_radius));
			snprintf(tmp, sizeof(tmp), "Out_Pin_%u", (u32)child_moving);
			ImGui::InvisibleButton(tmp, ImVec2(circle_radius * 2, circle_radius * 2), ImGuiButtonFlags_MouseButtonLeft);
			if (ImGui::IsItemHovered()) {
				
			}

			if(ImGui::IsItemActive()) {
				stnc_rndr->active_conntection = true;
			} else {
				stnc_rndr->active_conntection = false;
				// TODO(DH): Create search for spawning new node from list
			}
			
			v2 node_position = V2(canvas_p0.x + scrolling.x + child_moving->x, canvas_p0.y + scrolling.y + child_moving->y);
			draw_list->AddCircleFilled(im_vec2(node_position + stnc_rndr->start_point), circle_radius, IM_COL32(128, 128, 128, 255), 32);
			
			ImGui::EndChild();
			
			if(stnc_rndr->active_conntection) {
				bezier loperamid = create_bezier(&ctx->mem_arena, node_position + stnc_rndr->start_point, V2(ImGui::GetMousePos().x, ImGui::GetMousePos().y));
				imgui_draw_bezier(ctx->mem_arena, loperamid);
			}

			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
			ImGui::PopClipRect();
		};

		let mod = ctx->mem_arena.get_ptr(stnc_rndr->mod);
		mod->defs.iter([=](definition *def) -> void {
			if(def->tag == definition::node) {
				def->data.node.nodes.iter([=](node *nd) -> void {
					render_node(ctx, stnc_rndr, canvas_p0, canvas_p1, io, (v2*)nd->pos);
				});
			}
		});

		
    }
	ImGui::End();

	return canvas_position;
}