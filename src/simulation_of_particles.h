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

	memory_arena mem_arena;
	arena_array<particle> particles;

	func update_particles(f32 delta_time) -> void;
	func resolve_collisions(particle data, f32 particle_size) -> particle;
};

static inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation;
