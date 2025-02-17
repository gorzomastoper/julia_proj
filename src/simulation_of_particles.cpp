#include "simulation_of_particles.h"

template<typename T> func sign(T val) -> bool {
	return (T(0) < val) - (val < T(0));
}

func update_particle(f32 delta_time, particle p, f32 gravity) -> particle {
	p.velocity += V2(0, 1) * gravity * delta_time;
	p.position += p.velocity * delta_time;
	return p;
}

func particle_simulation::update_particles(f32 dt) -> void {
	auto particles = mem_arena.get_array(this->particles);

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		particles[i] = update_particle(dt, particles[i], gravity);
		particles[i] = resolve_collisions(particles[i], particles[i].particle_size);
	}
}

func particle_simulation::resolve_collisions(particle data, f32 particle_size) -> particle {
	v2 half_bound_size = this->bounds_size * 0.5f -  V2(particle_size, particle_size);

	if(abs(data.position.x) > half_bound_size.x) {
		data.position.x = half_bound_size.x * sign(data.position.x);
		data.velocity.x *= -1 * collision_damping;
	}
	if(abs(data.position.y) > half_bound_size.y) {
		data.position.y = half_bound_size.y * sign(data.position.y);
		data.velocity.y *= -1 * collision_damping;
	}

	return data;
}

inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation {
	particle_simulation result = {};
	result.mem_arena = initialize_arena(Megabytes(1));
	result.particles = result.mem_arena.alloc_array<particle>(particle_count);
	result.gravity = gravity;
	result.collision_damping = collision_damping;

	

	return result;
}