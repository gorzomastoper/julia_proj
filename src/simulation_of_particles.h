#pragma once
#include "dmath.h"
#include "util/memory_management.h"
#include "dx_backend.h"

struct pos_and_vel {
	v2 position;
	v2 velocity;
};

struct spatial_data {
	u32 particle_index;
	u32 hash;
	u32 cell_key;
};

struct particles_info {
	u32 particle_count;
	f32 smoothing_radius;
	f32 pressure_multiplier;
	f32 max_velocity;
	v4 color_a;
	v4 color_b;
	v4 color_c;
	f32 pull_push_strength;
	f32 pull_push_radius;
	f32 viscosity_strength;
	f32 near_pressure_multiplier;
	v2 pull_push_input_point;
	f32 gravity;
	f32 delta_time;
	f32 spiky_kernel_pow_2_scaling_factor;
	f32 spiky_kernel_pow_3_scaling_factor;
	f32 derivative_spiky_kernel_pow_2_scaling_factor;
	f32 derivative_spiky_kernel_pow_3_scaling_factor;
	f32 smoothing_kernel_poly_6_scaling_factor;
	f32 collision_damping;
	f32 target_density;
	f32 Ignored2_;
	v2 bounds_size;
	v2 ignored3_;
	v4 padding[7];
};
static_assert((sizeof(particles_info) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct sorting_info {
	u32 num_entries;
	u32 group_width;
	u32 group_height;
	u32 step_index;
	u32 nums_per_dispatch;
};

struct particle_simulation {
	u32 sim_data_counter;
	f32 particle_size;
	f32 interaction_strength;

	particles_info info_for_cshader;
	sorting_info info_for_sorting;

	memory_arena 					arena;
	arena_array<sorting_info>		sorting_infos;
	arena_array<v2>					positions;
	arena_array<v2>					predicted_positions;
	arena_array<v2>					velocities;
	arena_array<mat4> 				matrices;
	arena_array<resource_and_view> 	resources_and_views;
	arena_array<f32>				particle_properties;
	arena_array<v2>					densities;
	arena_array<f32>				near_densities;
	arena_array<f32>				final_gradient;
	arena_array<v2i>				cell_offsets;
	arena_array<i32>				start_indices;
	arena_array<spatial_data>		spatial_lookup;
	ID3D12CommandAllocator* 		command_allocators[g_NumFrames];

	ID3D12GraphicsCommandList *cmd_list;
	ID3D12GraphicsCommandList *compute_cmd_list;

	descriptor_heap simulation_desc_heap;

	rendering_stage rndr_stage;

	inline func update_particles(f32 delta_time) -> void;
	inline func resolve_collisions(v2* position, v2* velocity,f32 particle_size) -> void;
	inline func calculate_density(v2 sample_point, f32 smoothing_radius) -> f32;
	inline func calculate_near_density(v2 sample_point, f32 smoothing_radius) -> f32;
	inline func calculate_viscosity(u32 particle_index) -> v2;
	inline func calculate_property(v2 sample_point, f32 smoothing_radius) -> f32;
	inline func calculate_pressure_force(u32 particle_idx, f32 smoothing_radius) -> v2;
	inline func calculate_shared_pressure(f32 density_a, f32 density_b, f32 near_density_a, f32 near_density_b) -> v2;
	inline func simulation_step(dx_context* ctx, f32 delta_time, u32 width, u32 height, v2 mouse_pos, bool is_left_mouse, bool is_right_mouse) -> void;
	inline func update_spatial_lookup(f32 radius) -> void;
	inline func interaction_force(v2 input_pos, f32 radius, f32 strength, u32 particle_idx) -> v2;
	inline func foreach_point_within_radius(f32 dt, v2 sample_point, u8* data, void(*lambda)(particle_simulation *sim, u32 particle_idx, f32 dt, f32 gravity, u8* data)) -> void;
	inline func convert_density_to_pressure(f32 density, f32 near_density) -> v2;
	inline func particle_sim_start_frame(u32 frame_idx, ID3D12GraphicsCommandList *cmd_list, ID3D12CommandAllocator **cmd_allocator, ID3D12PipelineState *pipeline_state) -> void;
};

static inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation;
inline func update_settings(particle_simulation* sim, f32 delta_time, v2 mouse_pos, bool is_left_mouse, bool is_right_mouse) -> void;
func generate_command_buffer(dx_context *context, memory_arena arena, ID3D12GraphicsCommandList *cmd_list, ID3D12CommandAllocator **cmd_allocators, descriptor_heap heap, rendering_stage rndr_stage) -> ID3D12GraphicsCommandList*;
func generate_compute_command_buffer(dx_context *ctx, memory_arena arena, arena_array<resource_and_view> r_n_v, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage, u32 width, u32 height) -> ID3D12GraphicsCommandList*;
func generate_compute_fxaa_CB(dx_context *ctx, memory_arena arena, arena_array<resource_and_view> r_n_v, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage, u32 width, u32 height) -> ID3D12GraphicsCommandList*;
