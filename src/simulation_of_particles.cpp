#include "simulation_of_particles.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <execution>
#include <numbers>
#include <random>
#include <immintrin.h>
#include <vector>

v2 calculate_circle_uv(f32 radius, f32 angle, f32 max_radius) {
	f32 v = radius / max_radius;
	f32 u = angle / (2.0f * std::numbers::pi);
	return V2(u,v);
}

// Generate simple circle
std::vector<vertex> generate_circle_vertices(f32 radius, f32 x, f32 y, u32 fragments)
{
	const float PI = 3.1415926f;
	std::vector<vertex> result;

	float increment = 2.0 * PI / fragments;

	result.push_back(vertex{V3(x, y, 0.0), V4(0.5, 0.5, 0, 0)});

	for(float curr_angle = 0.0f; curr_angle < 2.0 * PI; curr_angle += increment)
	{
		v2 uv = calculate_circle_uv(radius, curr_angle, radius * 4.0f);
		result.push_back(vertex{V3(radius * cos(curr_angle) + x, radius * sin(curr_angle) + y, 0.0), V4(uv.x, uv.y, 0, 0)});
	}

	return result;
}

std::vector<u32> generate_circle_indices(u32 vertices_count) {
	std::vector<u32> result;

	for(u32 i = 0; i < vertices_count-1; i+=1)
	{
		result.push_back(0);
		result.push_back((i + 2));
		result.push_back((i + 1));
	}

	result.push_back(0);
	result.push_back(1);
	result.push_back(vertices_count);

	return result;
}

static inline func spiky_pow_2_scaling_factor(f32 smoothing_radius) -> f32 {
	return 6.0f / (std::numbers::pi * pow(smoothing_radius, 4.0f));
}

static inline func spiky_pow_3_scaling_factor(f32 smoothing_radius) -> f32 {
	return 10.0f / (std::numbers::pi * pow(smoothing_radius, 5.0f));
}

static inline func spiky_pow_2_derivative_scaling_factor(f32 smoothing_radius) -> f32 {
	return 12.0f / (std::numbers::pi * pow(smoothing_radius, 4.0f));
}

static inline func spiky_pow_3_derivative_scaling_factor(f32 smoothing_radius) -> f32 {
	return 30.0f / (std::numbers::pi * pow(smoothing_radius, 5.0f));
}

static inline func smoothing_kernel(f32 dst, f32 radius) -> f32 {
	if(dst < radius) {
		f32 v = radius - dst;
		return v * v * spiky_pow_2_scaling_factor(radius);
	}
	return 0;
}

static inline func smoothing_kernel_near(f32 dst, f32 radius) -> f32 {
	if(dst >= radius) return 0;
	f32 volume = (std::numbers::pi * pow(radius, 6.0f)) / 8.0f;
	return(radius - dst) * (radius - dst) / volume;
}

static inline func viscosity_smoothing_kernel(f32 dst, f32 radius) -> f32 {
	if(dst >= radius) return 0;
	f32 volume = (std::numbers::pi * pow(radius, 4.0f)) / 6.0f;
	return(radius - dst) * (radius - dst) / volume;
}

static inline func smoothing_kernel_derivative(f32 dst, f32 radius) -> f32 {
	if(dst <= radius) {
		f32 v = radius - dst;
		return -v * spiky_pow_2_derivative_scaling_factor(radius);
	}
	return 0;
}

static inline func smoothing_kernel_derivative_near(f32 dst, f32 radius) -> f32 {
	if(dst <= radius) {
		f32 v = radius - dst;
		return -v * spiky_pow_3_derivative_scaling_factor(radius);
	}
	return 0;
}

static inline func get_random_dir() -> v2 {
	// NOTE(DH): Randomly create particles
	printf("Get random dir is called !\n");
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<f32> distrib(0.0f, 2.0f * std::numbers::pi);

	f32 angle = distrib(gen);

	f32 x = cos(angle);
	f32 y = sin(angle);

	return V2(x, y);
}

// NOTE(DH): Convert position to the coordinate of the cell it is within
static inline func position_to_cell_coord (v2 point, f32 radius) -> v2i {
	i32 cell_x = i32(point.x / radius);
	i32 cell_y = i32(point.y / radius);
	return V2i(cell_x, cell_y);
}

// NOTE(DH): Convert a cell coordinate into a single number
// Hash collisions (different cells -> same value) are unavoidable, but we want to
// at least try to minimize collisions for nearby cells. Mabe better ways is exist, but
// this is my approach, so it works for now :)
static inline func hash_cell(i32 cell_x, i32 cell_y) -> u32 {
	u32 a = (u32)cell_x * 15823;
	u32 b = (u32)cell_y * 9737333;
	return a + b;
}

// NOTE(DH): Wrap the hash value around the length of the array (so it can be used as an index)
static inline func get_key_from_hash(u32 hash, u32 array_count) -> u32 {
	return hash % array_count;
}

inline func particle_simulation::foreach_point_within_radius(f32 dt, v2 sample_point, u8* data, void(*lambda)(particle_simulation *sim, u32 particle_idx, f32 dt, f32 gravity, u8* data)) -> void {
	auto indices 		= arena.get_array(this->start_indices);
	auto spatial_lookup = arena.get_array(this->spatial_lookup);
	auto points			= arena.get_array(this->positions);

	u32 num_of_iters = 0;

	v2i centre = position_to_cell_coord(sample_point, this->info_for_cshader.smoothing_radius);
	f32 sqr_radius = this->info_for_cshader.smoothing_radius * this->info_for_cshader.smoothing_radius;

	auto cell_offsets = arena.get_array(this->cell_offsets);

	for(u32 i = 0; i < this->cell_offsets.count; ++i) {
		u32 key = get_key_from_hash(hash_cell(centre.x + cell_offsets[i].x, centre.y + cell_offsets[i].y), this->spatial_lookup.count);
		i32 cell_start_index = indices[key];

		for(i32 j = cell_start_index; j < this->spatial_lookup.count; ++j) {
			if(spatial_lookup[j].cell_key != key) break;

			u32 particle_index  = spatial_lookup[j].particle_index;
			f32 sqr_dst = Length(points[particle_index] - sample_point);

			// NOTE(DH): Test if the point is inside the radius
			if(sqr_dst <= sqr_radius) {
				lambda(this, particle_index, dt, this->info_for_cshader.gravity, data);
				++num_of_iters;
			}
		}
	}

	// printf("num of iters: %u\n", num_of_iters);
}

inline func particle_simulation::update_spatial_lookup(f32 radius) -> void {
	auto indices = arena.get_array(this->start_indices);
	auto lookup = arena.get_array(this->spatial_lookup);
	auto points	= arena.get_array(this->predicted_positions);

	for(u32 i = 0 ; i < this->positions.count; ++i) {
		v2i cell = position_to_cell_coord(points[i], radius);
		u32 cell_key = get_key_from_hash(hash_cell(cell.x, cell.y), this->positions.count);
		lookup[i] = {.particle_index = i, .cell_key = cell_key};
		indices[i] = INT_MAX;
	}

	auto sort_func = [](spatial_data a, spatial_data b) {
		return a.cell_key < b.cell_key;
	};

	std::sort(lookup, lookup + this->spatial_lookup.count, sort_func);

	for(u32 i = 0; i < this->positions.count; ++i) {
		u32 key = lookup[i].cell_key;
		u32 key_prev = (i == 0) ? UINT_MAX : lookup[i - 1].cell_key;
		if(key != key_prev) {
			indices[key] = i;
		}
	}
}

inline func particle_simulation::calculate_shared_pressure(f32 density_a, f32 density_b, f32 near_density_a, f32 near_density_b) -> v2 {
	v2 pressure_A = convert_density_to_pressure(density_a, near_density_a);
	v2 pressure_B = convert_density_to_pressure(density_b, near_density_b);
	v2 result = V2((pressure_A.x + pressure_B.x) / 2, (pressure_A.y + pressure_B.y) / 2);
	return result;
}

inline func particle_simulation::calculate_pressure_force(u32 particle_idx, f32 smoothing_radius) -> v2 {
	auto densities = arena.get_array(this->densities);

	v2 pressure_force = {};

	auto indices 		= arena.get_array(this->start_indices);
	auto spatial_lookup = arena.get_array(this->spatial_lookup);
	auto points			= arena.get_array(this->predicted_positions); // NOTE(DH): We need to use here predicted positions!!!

	v2i centre = position_to_cell_coord(points[particle_idx], smoothing_radius);
	f32 sqr_radius = smoothing_radius * smoothing_radius;

	auto cell_offsets = arena.get_array(this->cell_offsets);

	for(u32 i = 0; i < this->cell_offsets.count; ++i) {
		u32 key = get_key_from_hash(hash_cell(centre.x + cell_offsets[i].x, centre.y + cell_offsets[i].y), this->spatial_lookup.count);
		i32 cell_start_index = indices[key];

		for(i32 j = cell_start_index; j < this->spatial_lookup.count; ++j) {
			if(spatial_lookup[j].cell_key != key) break;

			u32 particle_index  = spatial_lookup[j].particle_index;

			if(particle_idx == particle_index) continue;

			v2 offset_to_neighbour = points[particle_index] - points[particle_idx];
			f32 sqr_dst = Inner(offset_to_neighbour, offset_to_neighbour);

			// NOTE(DH): Test if the point is inside the radius
			if(sqr_dst > sqr_radius) continue;

			f32 dst = sqrt(sqr_dst);
			v2 dir = (dst > 0.0f) ?  offset_to_neighbour / dst : get_random_dir();

			f32 density = densities[particle_idx].x;
			f32 near_density = densities[particle_idx].y;
			v2 shared_pressure = calculate_shared_pressure(density, densities[particle_idx].x, near_density, densities[particle_idx].y);
			pressure_force += shared_pressure.x * dir * smoothing_kernel_derivative(dst, smoothing_radius) / density;
			pressure_force += shared_pressure.y * dir * smoothing_kernel_derivative_near(dst, smoothing_radius) / near_density;
		}
	}

	return pressure_force;
}

inline func particle_simulation::calculate_density(v2 sample_point, f32 smoothing_radius) -> f32 {
	auto positions 		= arena.get_array(this->predicted_positions);
	auto indices 		= arena.get_array(this->start_indices);
	auto spatial_lookup = arena.get_array(this->spatial_lookup);

	f32 density = 0;

	auto cell_offsets = arena.get_array(this->cell_offsets);
	v2i centre = position_to_cell_coord(sample_point, smoothing_radius);
	f32 sqr_radius = Square(smoothing_radius);

	for(u32 i = 0; i < this->cell_offsets.count; ++i) {
		u32 key = get_key_from_hash(hash_cell(centre.x + cell_offsets[i].x, centre.y + cell_offsets[i].y), this->spatial_lookup.count);
		i32 cell_start_index = indices[key];

		for(i32 j = cell_start_index; j < this->spatial_lookup.count; ++j) {
			if(spatial_lookup[j].cell_key != key) break;

			u32 particle_index  = spatial_lookup[j].particle_index;

			v2 offset_to_neighbour = positions[particle_index] - sample_point;
			f32 sqr_dst = Inner(offset_to_neighbour, offset_to_neighbour);

			// NOTE(DH): Test if the point is inside the radius
			if(sqr_dst <= sqr_radius) {
				f32 dst = sqrt(sqr_dst);
				f32 influence = smoothing_kernel(dst, smoothing_radius);
				density += influence;
			}
		}
	}

	return density;
}

inline func particle_simulation::calculate_near_density(v2 sample_point, f32 smoothing_radius) -> f32 {
	auto positions 		= arena.get_array(this->predicted_positions);
	auto indices 		= arena.get_array(this->start_indices);
	auto spatial_lookup = arena.get_array(this->spatial_lookup);

	f32 density = 0;

	auto cell_offsets = arena.get_array(this->cell_offsets);
	v2i centre = position_to_cell_coord(sample_point, smoothing_radius);
	f32 sqr_radius = Square(smoothing_radius);

	for(u32 i = 0; i < this->cell_offsets.count; ++i) {
		u32 key = get_key_from_hash(hash_cell(centre.x + cell_offsets[i].x, centre.y + cell_offsets[i].y), this->spatial_lookup.count);
		i32 cell_start_index = indices[key];

		for(i32 j = cell_start_index; j < this->spatial_lookup.count; ++j) {
			if(spatial_lookup[j].cell_key != key) break;

			u32 particle_index  = spatial_lookup[j].particle_index;
			v2 offset_to_neighbour = positions[particle_index] - sample_point;
			f32 sqr_dst = Inner(offset_to_neighbour, offset_to_neighbour);

			// NOTE(DH): Test if the point is inside the radius
			if(sqr_dst <= sqr_radius) {
				f32 dst = sqrt(sqr_dst);
				f32 influence = smoothing_kernel_near(dst, smoothing_radius);
				density += influence;
			}
		}
	}

	return density;
}

inline func particle_simulation::calculate_viscosity(u32 particle_index) -> v2 {
	auto positions 		= arena.get_array(this->positions);
	auto indices 		= arena.get_array(this->start_indices);
	auto spatial_lookup = arena.get_array(this->spatial_lookup);
	auto velocities 	= arena.get_array(this->velocities);

	v2 viscosity_force = {};
	v2 position = positions[particle_index];

	f32 density = 0;

	auto cell_offsets = arena.get_array(this->cell_offsets);
	v2i centre = position_to_cell_coord(position, this->info_for_cshader.smoothing_radius);
	f32 sqr_radius = Square(this->info_for_cshader.smoothing_radius);

	for(u32 i = 0; i < this->cell_offsets.count; ++i) {
		u32 key = get_key_from_hash(hash_cell(centre.x + cell_offsets[i].x, centre.y + cell_offsets[i].y), this->spatial_lookup.count);
		i32 cell_start_index = indices[key];

		for(i32 j = cell_start_index; j < this->spatial_lookup.count; ++j) {
			if(spatial_lookup[j].cell_key != key) break;

			u32 other_index  = spatial_lookup[j].particle_index;
			v2 offset_to_neighbour = positions[other_index] - position;
			f32 sqr_dst = Inner(offset_to_neighbour, offset_to_neighbour);

			// NOTE(DH): Test if the point is inside the radius
			if(sqr_dst <= sqr_radius) {
				f32 dst = sqrt(sqr_dst);
				f32 influence = viscosity_smoothing_kernel(dst, this->info_for_cshader.smoothing_radius);
				viscosity_force += (velocities[other_index] - velocities[particle_index]) * influence;
			}
		}
	}

	return viscosity_force * this->info_for_cshader.viscosity_strength;
}

inline func particle_simulation::calculate_property(v2 sample_point, f32 smoothing_radius) -> f32 {
	auto positions = arena.get_array(this->positions);
	auto prprtes   = arena.get_array(this->particle_properties);
	auto densities = arena.get_array(this->densities);

	f32 property = 0;
	f32 mass = 1.0f;

	for(u32 i = 0 ; i < this->cell_offsets.count; ++i) {
		f32 dst = Length(positions[i] - sample_point);
		f32 influence = smoothing_kernel(smoothing_radius, dst);
		f32 density = densities[i].x;
		property += SafeRatio0(-prprtes[i] * mass * influence, density);
	}

	return property;
}

inline func particle_simulation::convert_density_to_pressure(f32 density, f32 near_density) -> v2 {
	f32 density_error = density - info_for_cshader.target_density;
	f32 pressure = density_error * info_for_cshader.pressure_multiplier;
	f32 near_pressure = near_density * this->info_for_cshader.near_pressure_multiplier;
	return V2(pressure, near_pressure);
}

func sign(f32 val) -> f32 {
	if(val > 0) return 1;
	else if(val < 0) return -1.0f;
	return 0;
}

func particle_simulation::resolve_collisions(v2* position, v2* velocity, f32 particle_size) -> void {
	v2 half_bound_size = this->info_for_cshader.bounds_size * 0.5f - V2(particle_size, particle_size);

	if(abs(position->x) > half_bound_size.x) {
		position->x = half_bound_size.x * sign(position->x);
		velocity->x *= -1 * info_for_cshader.collision_damping;
	}
	if(abs(position->y) > half_bound_size.y) {
		position->y = half_bound_size.y * sign(position->y);
		velocity->y *= -1 * info_for_cshader.collision_damping;
	}
}

ID3D12GraphicsCommandList* generate_compute_command_buffer(dx_context *ctx, memory_arena arena, arena_array<resource_and_view> r_n_v, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage, u32 width, u32 height)
{
	auto cmd_allocator = get_command_allocator(ctx->g_frame_index, ctx->g_command_allocators);
	auto srv_dsc_heap = heap;

	rendering_stage 	stage 		= rndr_stage;
	render_pass 		rndr_pass	= arena.load_by_idx(stage.render_passes.ptr, 1);
	compute_pipeline	c_p_1 		= arena.load(rndr_pass.compute.curr);

	//NOTE(DH): Reset command allocators for the next frame
	record_reset_cmd_allocator(cmd_allocator);

	//NOTE(DH): Reset command lists for the next frame
	record_reset_cmd_list(cmd_list, cmd_allocator, c_p_1.state);
	cmd_list->SetName(L"COMPUTE COMMAND LIST");

	// NOTE(DH): Set root signature and pipeline state
	cmd_list->SetComputeRootSignature(c_p_1.root_signature);
	cmd_list->SetPipelineState(c_p_1.state);

	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap.addr};
	record_dsc_heap(cmd_list, ppHeaps, _countof(ppHeaps));

	c_p_1.generate_binding_table(ctx, &srv_dsc_heap, &arena, cmd_list, c_p_1.bindings);

	u32 dispatch_X = (width / 8) + 1;
	u32 dispatch_y = (height / 8) + 1;

	cmd_list->Dispatch(dispatch_X, dispatch_y, 1);

	// 4.) Copy result from back buffer to screen
	c_p_1.copy_to_screen_rt(ctx, &arena, r_n_v, cmd_list, c_p_1.bindings);

	//Finally executing the filled Command List into the Command Queue
	return cmd_list;
}

ID3D12GraphicsCommandList* generate_compute_fxaa_CB(dx_context *ctx, memory_arena arena, arena_array<resource_and_view> r_n_v, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage, u32 width, u32 height)
{
	auto cmd_allocator = get_command_allocator(ctx->g_frame_index, ctx->g_command_allocators);
	auto srv_dsc_heap = heap;

	rendering_stage 	stage 		= rndr_stage;
	render_pass 		rndr_pass	= arena.load_by_idx(stage.render_passes.ptr, 2);
	compute_pipeline	c_p_1 		= arena.load(rndr_pass.compute.curr);

	//NOTE(DH): Reset command allocators for the next frame
	// record_reset_cmd_allocator(cmd_allocator);

	//NOTE(DH): Reset command lists for the next frame
	// record_reset_cmd_list(cmd_list, cmd_allocator, c_p_1.state);
	cmd_list->SetName(L"COMPUTE COMMAND LIST");

	// NOTE(DH): Set root signature and pipeline state
	cmd_list->SetComputeRootSignature(c_p_1.root_signature);
	cmd_list->SetPipelineState(c_p_1.state);

	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap.addr};
	record_dsc_heap(cmd_list, ppHeaps, _countof(ppHeaps));

	c_p_1.generate_binding_table(ctx, &srv_dsc_heap, &arena, cmd_list, c_p_1.bindings);

	// Copy from screen to buffer
	c_p_1.copy_screen_to_render_target(ctx, &arena, r_n_v, cmd_list, c_p_1.bindings);

	u32 dispatch_X = (width / 8) + 1;
	u32 dispatch_y = (height / 8) + 1;

	cmd_list->Dispatch(dispatch_X, dispatch_y, 1);

	// 4.) Copy result from back buffer to screen
	c_p_1.copy_to_screen_rt(ctx, &arena, r_n_v, cmd_list, c_p_1.bindings);

	//Finally executing the filled Command List into the Command Queue
	return cmd_list;
}

ID3D12GraphicsCommandList* generate_command_buffer(dx_context *context, memory_arena arena, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage)
{
	auto cmd_allocator = get_command_allocator(context->g_frame_index, context->g_command_allocators);
	auto srv_dsc_heap = heap;

	rendering_stage 	stage 		= rndr_stage;
	render_pass 		only_pass 	= arena.load_by_idx(stage.render_passes.ptr, 0);
	graphic_pipeline	g_p 		= arena.load(only_pass.graph.curr);

	//NOTE(DH): Reset command allocators for the next frame
	record_reset_cmd_allocator(cmd_allocator);

	//NOTE(DH): Reset command lists for the next frame
	record_reset_cmd_list(cmd_list, cmd_allocator, g_p.state);
	cmd_list->SetName(L"GRAPHICS COMMAND LIST");

	// NOTE(DH): Set root signature and pipeline state
	cmd_list->SetGraphicsRootSignature(g_p.root_signature);
	cmd_list->SetPipelineState(g_p.state);

	cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	record_viewports(1, context->viewport, cmd_list);
	record_scissors(1, context->scissor_rect, cmd_list);

    // Indicate that the back buffer will be used as a render target.
	auto transition = barrier_transition(context->g_back_buffers[context->g_frame_index],D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	record_resource_barrier(1, transition, cmd_list);

	set_render_target(cmd_list, context->g_rtv_descriptor_heap, 1, FALSE, context->g_frame_index, context->g_rtv_descriptor_size);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.0f, 0.06f, 1.0f };
    cmd_list->ClearRenderTargetView
	(
		get_rtv_descriptor_handle(context->g_rtv_descriptor_heap, context->g_frame_index, context->g_rtv_descriptor_size), 
		clearColor, 
		0, 
		nullptr
	);

	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap.addr};
	record_dsc_heap(cmd_list, ppHeaps, _countof(ppHeaps));

	g_p.generate_binding_table(context, &srv_dsc_heap, &arena, cmd_list, g_p.bindings);
	
	// Execute bundle
	// cmd_list->ExecuteBundle(entity.bundle);

    // Indicate that the back buffer will now be used to present.
	transition = barrier_transition(context->g_back_buffers[context->g_frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    record_resource_barrier(1, transition, cmd_list);

	//Finally executing the filled Command List into the Command Queue
	return cmd_list;
}

float example_function(v2 pos) {
	return cos(pos.y - 3 + sin(pos.x));
}

inline func particle_simulation::simulation_step(dx_context *ctx, f32 delta_time, u32 width, u32 height, v2 mouse_pos, bool is_left_mouse, bool is_right_mouse) -> void {
	this->sim_data_counter++;
	this->info_for_cshader.delta_time = delta_time;

	auto dnsties	= arena.get_array(this->densities);
	auto near_dnsties	= arena.get_array(this->near_densities);
	auto positions = arena.get_array(this->positions);
	auto velocities = arena.get_array(this->velocities);
	auto predicted_positions = arena.get_array(this->predicted_positions);

	f32 prediction_factor = 1.0f / 120.0f;
	f32 aspect = (f32)width / (f32)height;
	f32 scale = 5.0f;
	v2 mouse_centered = V2((-mouse_pos.x + (width / 2)), mouse_pos.y - (height / 2));
	mat4 MV = translation_matrix(V4(V3(mouse_centered, 1.0f), 1.0f)) * screen_to_ndc(width, height, scale, aspect);
	v4 ndc_mouse_pos = V4(V3(mouse_centered, 1.0f), 1.0f) * MV;
	// ndc_mouse_pos = camera * ndc_mouse_pos;
	// mat4 view = (translation_matrix(V3(mouse_pos, 1.0f)) * camera);

	for(u32 i = 0 ; i < this->positions.count; ++i) {
		velocities[i] += V2(0.0, 1.0f) * info_for_cshader.gravity * delta_time;

		if(is_left_mouse)
			velocities[i] += interaction_force(ndc_mouse_pos.xy, this->info_for_cshader.pull_push_radius, this->info_for_cshader.pull_push_strength, i);
		else if (is_right_mouse)
			velocities[i] += interaction_force(ndc_mouse_pos.xy, this->info_for_cshader.pull_push_radius, -this->info_for_cshader.pull_push_strength, i);

		predicted_positions[i] = positions[i] + velocities[i] * prediction_factor;
	}

	update_spatial_lookup(this->info_for_cshader.smoothing_radius);
	
	// u32 i = 0;
	// std::for_each(std::execution::par, velocities, velocities + this->velocities.count, [this, &i, delta_time, positions, predicted_positions, dnsties](v2& elem){
	// 	elem += V2(0.0, 1.0f) * gravity * delta_time;
	// 	predicted_positions[i] = positions[i] + elem * delta_time;
	// 	dnsties[i] = calculate_density(predicted_positions[i], this->info_for_cshader.smoothing_radius);
	// 	++i;
	// });


	// auto offsets = arena.get_array(this->cell_offsets);

	// auto start = std::chrono::high_resolution_clock::now();
	// i = 0;
	// std::for_each(std::execution::seq, dnsties, dnsties + this->densities.count, [this, &i, delta_time, predicted_positions](f32& elem){
	// 	elem = calculate_density(predicted_positions[i], this->info_for_cshader.smoothing_radius);
	// 	++i;
	// });
	// auto end = std::chrono::high_resolution_clock::now();
	// std::chrono::duration<f32>	duration = end - start;
	// printf("duration of calc density: %f\n", (f32)std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());

	for(u32 i = 0 ; i < this->positions.count; ++i) {
		dnsties[i].x = calculate_density(predicted_positions[i], this->info_for_cshader.smoothing_radius);
	}

	for(u32 i = 0 ; i < this->positions.count; ++i) {
		dnsties[i].y = calculate_near_density(predicted_positions[i], this->info_for_cshader.smoothing_radius);
	}

	for(u32 i = 0 ; i < this->positions.count; ++i) {
		v2 pressure_force = calculate_pressure_force(i, this->info_for_cshader.smoothing_radius);
		v2 pressure_acceleration = pressure_force /  dnsties[i].x;
		velocities[i] += pressure_acceleration * delta_time;
	}

	for(u32 i = 0 ; i < this->positions.count; ++i) {
		velocities[i] += calculate_viscosity(i) * delta_time;
	}

	// start = std::chrono::high_resolution_clock::now();
	// i = 0;
	// std::for_each(std::execution::par, velocities, velocities + this->velocities.count, [this, &i, delta_time, dnsties](v2& elem){
	// 	v2 pressure_force = calculate_pressure_force(i, this->info_for_cshader.smoothing_radius);
	// 	v2 pressure_acceleration = pressure_force * (1.0f /  dnsties[i]);
	// 	elem += pressure_acceleration * delta_time;

	// 	if(Length(elem) > this->max_velocity) {
	// 		elem = normalize(elem) * this->max_velocity;
	// 	}
	// 	++i;
	// });
	// end = std::chrono::high_resolution_clock::now();
	// duration = end - start;
	// printf("duration of calc pressure force: %f\n", (f32)std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());

	for(u32 i = 0 ; i < this->positions.count; ++i) {
		positions[i] += velocities[i] * delta_time;
		resolve_collisions(&positions[i], &velocities[i], particle_size);
		arena.get_array(matrices)[i] = translation_matrix(V3(positions[i], 0.0f));
	}
}

inline func particle_simulation::interaction_force(v2 input_pos, f32 radius, f32 strength, u32 particle_idx) -> v2 {
	auto pos_array = arena.get_array(predicted_positions);
	auto vel_array = arena.get_array(velocities);
	v2 interaction_force = {};
	v2 offset = input_pos - pos_array[particle_idx];
	f32 sqr_dst = Inner(offset, offset);

	if(sqr_dst < (radius * radius)) {
		f32 dst = sqrt(sqr_dst);
		v2 dir_to_input_point = (dst <= FLT_EPSILON) ? V2(0.0f) : offset / dst;
		f32 centre_t = 1.0f - dst / radius;
		interaction_force = (dir_to_input_point * strength - vel_array[particle_idx]) * centre_t;
	}

	return interaction_force;
}

inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation {
	particle_simulation result = {};
	result.arena				= initialize_arena(Megabytes(64));

	result.resources_and_views	= result.arena.alloc_array<resource_and_view>(1024);

	result.matrices 			= result.arena.alloc_array<mat4>(particle_count);
	result.matrices.count		= particle_count;
	result.positions 			= result.arena.alloc_array<v2>(particle_count);
	result.positions.count 		= particle_count;
	result.velocities 			= result.arena.alloc_array<v2>(particle_count);
	result.velocities.count 	= particle_count;
	result.predicted_positions	= result.arena.alloc_array<v2>(particle_count);
	result.predicted_positions.count = particle_count;

	result.particle_properties 	= result.arena.alloc_array<f32>(particle_count);
	result.particle_properties.count = particle_count;
	
	result.densities 			= result.arena.alloc_array<v2>(particle_count);
	result.densities.count 		= particle_count;
	
	result.final_gradient 		= result.arena.alloc_array<f32>(2560 * 1440);
	result.final_gradient.count = 2560 * 1440;

	result.cell_offsets			= result.arena.alloc_array<v2i>(9); //3 * 3 cells
	result.cell_offsets.count	= 9;

	result.spatial_lookup		= result.arena.alloc_array<spatial_data>(particle_count);
	result.spatial_lookup.count	= particle_count;

	result.start_indices		= result.arena.alloc_array<i32>(particle_count);
	result.start_indices.count	= particle_count;

	result.info_for_cshader.gravity 				= gravity;
	result.info_for_cshader.collision_damping 	= collision_damping;
	result.info_for_cshader.bounds_size			= V2(18.0f, 10.0f);

	result.info_for_cshader		= {};
	result.info_for_cshader.particle_count = particle_count;
	result.info_for_cshader.smoothing_radius = 0.3;
	result.info_for_cshader.max_velocity = 1.0f;
	result.info_for_cshader.target_density = 1.5f;
	result.info_for_cshader.pressure_multiplier = 0.0f;

	result.cmd_list 			= create_command_list<ID3D12GraphicsCommandList>(ctx, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	result.simulation_desc_heap = allocate_descriptor_heap(ctx->g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 32);

	for(u32 i = 0; i < g_NumFrames; ++i) {
		result.command_allocators[i] = create_command_allocator(ctx->g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	result.particle_size = 0.04f;

	u32 particles_row = (i32)sqrt(particle_count);
	u32 particle_per_col  = (particle_count - 1) / particles_row + 1;
	float spacing = result.particle_size * 2 + 0.03f;

	auto p_n_vs		= result.arena.get_array(result.positions);
	auto prprtes	= result.arena.get_array(result.particle_properties);
	auto final_grad	= result.arena.get_array(result.final_gradient);
	auto densities	= result.arena.get_array(result.densities);
	auto velocities = result.arena.get_array(result.velocities);
	auto predicted_positions = result.arena.get_array(result.predicted_positions);

	// NOTE(DH): Randomly create particles
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> distrib(0, 1.0f);

	auto cell_offsets = result.arena.get_array(result.cell_offsets);

	for(u32 i = 0; i < result.positions.count; ++i) {
		// float x = (distrib(gen) - 0.5f) * result.bounds_size.x;
		// float y = (distrib(gen) - 0.5f) * result.bounds_size.y;

		float x = (i % particles_row - particles_row / 2.0f + 0.5f) * spacing;
		float y = (i / particles_row - particle_per_col / 2.0f + 0.5f) * spacing;
		
		p_n_vs[i].x = x;
		p_n_vs[i].y = y;

		// velocities[i] += V2(0.0, 1.0f) * gravity * result.delta_time;
		// predicted_positions[i] = p_n_vs[i] + velocities[i] * (1.0f / 120.0f);
	}

	// NOTE(DH): Initialize cells for gather
	cell_offsets[0] = V2i(-1, 1);
	cell_offsets[1] = V2i(0, 1);
	cell_offsets[2] = V2i(1, 1);
	cell_offsets[3] = V2i(-1, 0);
	cell_offsets[4] = V2i(0, 0);
	cell_offsets[5] = V2i(1, 0);
	cell_offsets[6] = V2i(-1, -1);
	cell_offsets[7] = V2i(0, -1);
	cell_offsets[8] = V2i(1, -1);

	// result.update_spatial_lookup(result.info_for_cshader.smoothing_radius);

	// for(u32 i = 0 ; i < particle_count; ++i) {
	// 	densities[i] = result.calculate_density(predicted_positions[i], result.info_for_cshader.smoothing_radius);
	// }

	WCHAR shader_path[] = L"shaders.hlsl";
	ID3DBlob* vertex_shader = compile_shader(ctx->g_device, shader_path, "VSMain", "vs_5_0");
	ID3DBlob* pixel_shader 	= compile_shader(ctx->g_device, shader_path, "PSMain", "ps_5_0");

	std::vector<vertex> circ_data 	= generate_circle_vertices(result.particle_size, 0, 0, 64);
	std::vector<u32> circ_idc 		= generate_circle_indices(64);

	auto mtx_data = result.arena.get_array(result.matrices);
	auto vel_data = result.arena.get_array(result.velocities);

	// NOTE(DH): Graphics pipeline
	{
		// NOTE(DH): Create resources
		buffer_cbuf	cbuff2 		= buffer_cbuf	::	create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)&result.info_for_cshader, 1);
		buffer_cbuf cbuff 		= buffer_cbuf	::	create 	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)&ctx->common_cbuff_data,0);
		buffer_vtex mesh		= buffer_vtex	::	create 	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)circ_data.data(), circ_data.size() * sizeof(vertex), 64);
		buffer_idex idxs		= buffer_idex	::	create 	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)circ_idc.data(), circ_idc.size() * sizeof(u32), particle_count, 65);
		buffer_1d 	matrices 	= buffer_1d		::	create	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(mat4), (u8*)mtx_data, 0);
		buffer_1d 	vel_buf 	= buffer_1d		::	create	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(v2), (u8*)vel_data, 1);

		// NOTE(DH): Create bindings, also need to remember that the order MATTER!
		auto binds = mk_bindings()
			.bind_buffer<false, false, false, false>	(result.arena.push_data(idxs))
			.bind_buffer<false, false, false, false>	(result.arena.push_data(mesh))
			.bind_buffer<false, true, false, false>		(result.arena.push_data(matrices))
			.bind_buffer<false, true, false, false>		(result.arena.push_data(vel_buf))
			.bind_buffer<false, true, false, false>		(result.arena.push_data(cbuff2))
			.bind_buffer<false, true, false, false>		(result.arena.push_data(cbuff));
		
		auto binds_ar_ptr = result.arena.push_data(binds);
		auto binds_ptr = *(arena_ptr<void>*)(&binds_ar_ptr);

		auto resize = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			Resize<decltype(binds)::BUF_TS_U>::resize(bnds->data, ctx->g_device, *arena, heap, &resources_and_views, width, height); 
		};

		auto generate_binding_table = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			GCB<decltype(binds)::BUF_TS_U>::bind_root_sig_table(bnds->data, 0, cmd_list, ctx->g_device, heap->addr, arena);
		};

		auto update = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			Update<decltype(binds)::BUF_TS_U>::update(bnds->data, ctx, arena, resources_and_views, cmd_list);
		};

		graphic_pipeline graph_pipeline = 
		graphic_pipeline::init__(binds_ptr, resize, generate_binding_table, update)
			.bind_vert_shader	(vertex_shader)
			.bind_frag_shader	(pixel_shader)
			.create_root_sig	(binds, ctx->g_device, &result.arena)
			.finalize			(binds, ctx, &result.arena, result.resources_and_views, ctx->g_device, &result.simulation_desc_heap);

		result.rndr_stage = 
		rendering_stage::init__(&result.arena, 3)
			.bind_graphic_pass(graph_pipeline, &result.arena);
	}

	// NOTE(DH): Compute shader
	{
		WCHAR filename[] = L"particle_simulation.hlsl";
		ID3DBlob* compute_shader = compile_shader(ctx->g_device, filename, "CSMain", "cs_5_0");

		render_target2d render_target 	= render_target2d	::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, ctx->viewport.Width, ctx->viewport.Height, 0);
		buffer_1d 		possns 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(v2), (u8*)p_n_vs, 1);
		buffer_1d 		dnsts 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(f32), (u8*)densities, 2);
		// buffer_1d 		prprts 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, width * height, sizeof(f32), (u8*)final_grad, 3);
		buffer_1d 		prprts 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(f32), (u8*)prprtes, 3);
		buffer_cbuf		cbuff2 			= buffer_cbuf		::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)&ctx->common_cbuff_data, 0);
		buffer_cbuf		cbuff 			= buffer_cbuf		::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)&result.info_for_cshader, 1);

		auto binds = mk_bindings()
			.bind_buffer<true, false, true, false>(result.arena.push_data(render_target))
			.bind_buffer<false, true, false, false>(result.arena.push_data(possns))
			.bind_buffer<false, true, false, false>(result.arena.push_data(dnsts))
			.bind_buffer<false, true, false, false>(result.arena.push_data(prprts))
			.bind_buffer<false, true, false, false>(result.arena.push_data(cbuff2))
			.bind_buffer<false,true, false, false>(result.arena.push_data(cbuff));

		auto binds_ar_ptr = result.arena.push_data(binds);
		auto binds_ptr = *(arena_ptr<void>*)(&binds_ar_ptr);

		auto resize = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			Resize<decltype(binds)::BUF_TS_U>::resize(bnds->data, ctx->g_device, *arena, heap, &resources_and_views, width, height); 
		};

		auto generate_binding_table = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			GCBC<decltype(binds)::BUF_TS_U>::bind_root_sig_table(bnds->data, 0, cmd_list, ctx->g_device, heap->addr, arena);
		};

		auto update = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			Update<decltype(binds)::BUF_TS_U>::update(bnds->data, ctx, arena, resources_and_views, cmd_list);
		};

		auto copy_to_render_target = [](dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			CTRT<decltype(binds)::BUF_TS_U>::copy_to_render_target(bnds->data, ctx, arena, resources_and_views, cmd_list);
		};

		auto copy_screen_to_render_target = [](dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			return;
		};

		compute_pipeline compute_pipeline = 
		compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
			.bind_shader		(compute_shader)
			.create_root_sig	(binds, ctx->g_device, &result.arena)
			.finalize			(binds, ctx, &result.arena, ctx->resources_and_views, ctx->g_device, &result.simulation_desc_heap);

		result.rndr_stage
			.bind_compute_pass(compute_pipeline, &result.arena);
	}

	// NOTE(DH): Compute shader FXAA
	{
		WCHAR filename[] = L"fxaa.hlsl";
		ID3DBlob* compute_shader = compile_shader(ctx->g_device, filename, "CSMain", "cs_5_0");

		render_target2d render_target 	= render_target2d	::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, ctx->viewport.Width, ctx->viewport.Height, 0);

		auto binds = mk_bindings()
			.bind_buffer<true, false, true, false>(result.arena.push_data(render_target));

		auto binds_ar_ptr = result.arena.push_data(binds);
		auto binds_ptr = *(arena_ptr<void>*)(&binds_ar_ptr);

		auto resize = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			Resize<decltype(binds)::BUF_TS_U>::resize(bnds->data, ctx->g_device, *arena, heap, &resources_and_views, width, height); 
		};

		auto generate_binding_table = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			GCBC<decltype(binds)::BUF_TS_U>::bind_root_sig_table(bnds->data, 0, cmd_list, ctx->g_device, heap->addr, arena);
		};

		auto update = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			Update<decltype(binds)::BUF_TS_U>::update(bnds->data, ctx, arena, resources_and_views, cmd_list);
		};

		auto copy_to_render_target = [](dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			CTRT<decltype(binds)::BUF_TS_U>::copy_to_render_target(bnds->data, ctx, arena, resources_and_views, cmd_list);
		};

		auto copy_screen_to_render_target = [](dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = arena->get_ptr(bnds_ptr);
			CRTS<decltype(binds)::BUF_TS_U>::copy_screen_to_render_target(bnds->data, ctx, arena, resources_and_views, cmd_list);
		};

		compute_pipeline compute_pipeline = 
		compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
			.bind_shader		(compute_shader)
			.create_root_sig	(binds, ctx->g_device, &result.arena)
			.finalize			(binds, ctx, &result.arena, ctx->resources_and_views, ctx->g_device, &result.simulation_desc_heap);

		result.rndr_stage
			.bind_compute_pass(compute_pipeline, &result.arena);
	}

	return result;
}