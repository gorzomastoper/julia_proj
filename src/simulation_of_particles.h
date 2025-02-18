#pragma once
#include "dmath.h"
#include "util/memory_management.h"
#include "dx_backend.h"

struct particle {
	f32 particle_size;
	v2 position;
	v2 velocity;
};

struct particle_simulation {
	f32 gravity;
	f32 collision_damping;
	v2 bounds_size;

	mat4 ndc_matrix;

	memory_arena arena;
	arena_array<particle> 		particles;
	arena_array<mat4> 			matrices;
	ID3D12CommandAllocator* 	command_allocators[g_NumFrames];

	ID3D12GraphicsCommandList *cmd_list;

	descriptor_heap simulation_desc_heap;

	rendering_stage rndr_stage;

	inline func update_particles(f32 delta_time) -> void;
	inline func resolve_collisions(particle data, f32 particle_size) -> particle;
	inline func calculate_density(v2 sample_point, f32 smoothing_radius) -> f32;
};

static inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation;

func generate_command_buffer(dx_context *context, memory_arena arena, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage) -> ID3D12GraphicsCommandList*;
