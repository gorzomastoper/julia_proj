#pragma once
#include "dmath.h"

struct particle {
	f32 particle_size;
	v2 position;
	v2 velocity;
};

struct particle_simulation {
	f32 gravity;
	f32 collision_damping;
	v2 bounds_size;
};

func update_particles(f32 delta_time, particle_simulation *data) -> void;
func resolve_collisions(v2 position, f32 particle_size, particle_simulation *data) -> v2;
