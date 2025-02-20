#pragma once
#include "dmath.h"
#include "util/memory_management.h"
#include "dx_backend.h"

struct pos_and_vel {
	v2 position;
	v2 velocity;
};

struct particle {
	f32 particle_size;
	u32 pos_and_velocity_idx;
};

struct spatial_data {
	u32 particle_index;
	u32 cell_key;
};

struct particles_info {
	f32 particle_count;
	f32 smoothing_radius;
	f32 pressure;
	f32 ignored2;
	v4 color_a;
	v4 color_b;
	v4 color_c;
	v4 padding[12];
};
static_assert((sizeof(particles_info) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct particle_simulation {
	f32 gravity;
	f32 collision_damping;
	f32 target_density;
	f32 pressure_multiplier;
	v2 bounds_size;

	particles_info info_for_cshader;

	memory_arena 					arena;
	arena_array<particle> 			particles;
	arena_array<pos_and_vel>		p_n_v;
	arena_array<mat4> 				matrices;
	arena_array<resource_and_view> 	resources_and_views;
	arena_array<f32>				particle_properties;
	arena_array<f32>				densities;
	arena_array<f32>				final_gradient;
	arena_array<v2i>				cell_offsets;
	arena_array<i32>				start_indices;
	arena_array<spatial_data>		spatial_lookup;
	ID3D12CommandAllocator* 		command_allocators[g_NumFrames];

	ID3D12GraphicsCommandList *cmd_list;

	descriptor_heap simulation_desc_heap;

	rendering_stage rndr_stage;

	inline func update_particles(f32 delta_time) -> void;
	inline func resolve_collisions(pos_and_vel data, f32 particle_size) -> pos_and_vel;
	inline func calculate_density(v2 sample_point, f32 smoothing_radius) -> f32;
	inline func calculate_property(v2 sample_point, f32 smoothing_radius) -> f32;
	inline func calculate_pressure_force(u32 particle_idx, f32 smoothing_radius) -> v2;
	inline func calculate_shared_pressure(f32 density_a, f32 density_b) -> f32;
	inline func convert_density_to_pressure(f32 density) -> f32;
	inline func simulation_step(f32 delta_time, u32 width, u32 height) -> void;
	inline func update_spatial_lookup(f32 radius) -> void;
	inline func foreach_point_within_radius(v2 sample_point) -> void;
};

static inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation;

func generate_command_buffer(dx_context *context, memory_arena arena, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage) -> ID3D12GraphicsCommandList*;
func generate_compute_command_buffer(dx_context *ctx, memory_arena arena, arena_array<resource_and_view> r_n_v, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage, u32 width, u32 height) -> ID3D12GraphicsCommandList*;
