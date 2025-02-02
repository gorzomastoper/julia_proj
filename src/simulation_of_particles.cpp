#include "simulation_of_particles.h"

template<typename T> func sign(T val) -> bool {
	return (T(0) < val) - (val < T(0));
}

func update_particle(f32 delta_time, particle *particle, f32 gravity) -> v2 {
	particle->velocity += V2(0, 1) * gravity * delta_time;
	particle->position += particle->velocity * delta_time;
	return particle->position;
}

func resolve_collisions(v2 position, f32 particle_size, particle_simulation *data) -> v2 {
	v2 half_bound_size = data->bounds_size * 0.5f -  V2(particle_size, particle_size);

	// if(abs(position.x) > half_bound_size.x) {
	// 	position.x = half_bound_size.x * sign(position.x);
	// 	data->velocity.x *= -1 * data->collision_damping;
	// }
	// if(abs(position.y) > half_bound_size.y) {
	// 	position.y = half_bound_size.y * sign(position.y);
	// 	data->velocity.y *= -1 * data->collision_damping;
	// }

	return position;
}