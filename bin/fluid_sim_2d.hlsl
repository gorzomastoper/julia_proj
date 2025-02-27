cbuffer simulation_properties : register(b0) {
	uint particle_count;
	float smoothing_radius;
	float pressure_multiplier;
	float max_velocity;
	float4 color_a;
	float4 color_b;
	float4 color_c;
	float pull_push_strength;
	float pull_push_radius;
	float viscosity_strength;
	float near_pressure_multiplier;
	float2 pull_push_input_point;
	float gravity;
	float delta_time;
	float spiky_kernel_pow_2_scaling_factor;
	float spiky_kernel_pow_3_scaling_factor;
	float derivative_spiky_kernel_pow_2_scaling_factor;
	float derivative_spiky_kernel_pow_3_scaling_factor;
	float smoothing_kernel_poly_6_scaling_factor;
	float collision_damping;
	float target_density;
	float Ignored2_;
	float2 bounds;
	float4 padding[8];
};

static const uint num_threads = 64;
static const int2 offsets_2d[9] = {
	int2(-1, +1),
	int2(+0, +1),
	int2(+1, +1),
	int2(-1, +0),
	int2(+0, +0),
	int2(+1, +0),
	int2(-1, -1),
	int2(+0, -1),
	int2(+1, -1),
};

static const uint hash_k1 = 15823;
static const uint hash_k2 = 9737333;

int2 get_cell_2d(float2 position, float radius) {
	return (int2)floor(position / radius);
}

uint hash_cell_2d(int2 cell) {
	cell = (uint2)cell;
	uint a = cell.x * hash_k1;
	uint b = cell.y * hash_k2;
	return (a + b);
}

uint key_from_hash(uint hash, uint table_size) {
	return hash % table_size;
}

struct spatial_data {
	uint particle_index;
	uint hash;
	uint key;
};

RWBuffer<float2> 					positions 			: register(u0);
RWBuffer<float2> 					predicted_positions : register(u1);
RWBuffer<float2> 					velocities 			: register(u2);
RWBuffer<float2> 					densities  			: register(u3);
RWStructuredBuffer<spatial_data> 	spacial_indices		: register(u4);
RWBuffer<uint3> 					spacial_offsets		: register(u5);
RWStructuredBuffer<float4x4> 		matrices			: register(u6);

float smoothing_kernel_poly_6(float dst, float radius) {
	if(dst < radius) {
		float v = radius * radius - dst * dst;
		return v * v * v * smoothing_kernel_poly_6_scaling_factor;
	}
	return 0;
}

float spiky_kernel_pow_3(float dst, float radius) {
	if(dst < radius) {
		float v = radius - dst;
		return v * v * v * spiky_kernel_pow_3_scaling_factor;
	}
	return 0;
}

float spiky_kernel_pow_2(float dst, float radius) {
	if(dst < radius) {
		float v = radius - dst;
		return v * v * spiky_kernel_pow_2_scaling_factor;
	}
	return 0;
}

float derivative_spiky_kernel_pow_3(float dst, float radius) {
	if(dst <= radius) {
		float v = radius - dst;
		return -v * v * derivative_spiky_kernel_pow_3_scaling_factor;
	}
	return 0;
}

float derivative_spiky_kernel_pow_2(float dst, float radius) {
	if(dst <= radius) {
		float v = radius - dst;
		return -v * derivative_spiky_kernel_pow_2_scaling_factor;
	}
	return 0;
}

float2 calculate_density(float2 pos) {
	int2 origin_cell = get_cell_2d(pos, smoothing_radius);
	float sqr_radius = smoothing_radius * smoothing_radius;
	float density = 0;
	float near_density = 0;

	for(uint i = 0 ; i < 8; ++i) {
		uint hash = hash_cell_2d(origin_cell + offsets_2d[i]);
		uint key = key_from_hash(hash, particle_count);
		uint curr_index = spacial_offsets[key];

		while(curr_index < particle_count) {
			spatial_data index_data = spacial_indices[curr_index];
			++curr_index;

			if(index_data.key != key) break;
			if(index_data.hash != hash) continue;

			uint neighbour_index = index_data.particle_index;
			float2 neighbour_pos = predicted_positions[neighbour_index];
			float2 offset_to_neighbour = neighbour_pos - pos;
			float sqr_dst_to_neighbour = dot(offset_to_neighbour, offset_to_neighbour);

			if(sqr_dst_to_neighbour > sqr_radius) continue;

			float dst = sqrt(sqr_dst_to_neighbour);
			density += spiky_kernel_pow_2(dst, smoothing_radius);
			near_density += spiky_kernel_pow_3(dst, smoothing_radius);
		}
	}

	return float2(density, near_density);
}

float pressure_from_density(float density) {
	return(density - target_density) * pressure_multiplier;
}

float near_pressure_from_density(float near_density) {
	return near_pressure_multiplier * near_density;
}

float4x4 translation_matrix(float2 position) {
	float4x4 result = float4x4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1);

	result[3][0] = position.x;
	result[3][1] = position.y;
	result[3][2] = 0.0f;
	return result;
}

float2 external_force(float2 pos, float2 velocity) {
	float2 gravity_accel = float2(0, gravity);

	if(pull_push_strength != 0) {
		float2 input_point_offset = pull_push_input_point - pos;
		float sqr_dst = dot(input_point_offset, input_point_offset);
		if(sqr_dst < pull_push_radius * pull_push_radius) {
			float dst = sqrt(sqr_dst);
			float edge_t = (dst / pull_push_radius);
			float center_t = 1.0f - edge_t;
			float2 dir_to_center = input_point_offset / dst;

			float gravity_weight = 1 - (center_t * saturate(pull_push_strength / 10));
			float2 accel = gravity_accel * gravity_weight + dir_to_center * center_t * pull_push_strength;
			accel -= velocity * center_t;
		}
	}

	return gravity_accel;
}

void handle_collisions(uint particle_index) {
	float2 pos = positions[particle_index];
	float2 vel = velocities[particle_index];

	float2 half_size = bounds * 0.5;
	float2 edge_dist = half_size - abs(pos);

	if(edge_dist.x <= 0) {
		pos.x = half_size.x * sign(pos.x);
		vel.x *= -1.0f * collision_damping;
	}
	if(edge_dist.y <= 0) {
		pos.y = half_size.y * sign(pos.y);
		vel.y *= -1.0f * collision_damping;
	}

	positions[particle_index] = pos;
	velocities[particle_index] = vel;
}

[numthreads(num_threads, 1, 1)]
void external_forces (uint3 id : SV_DispatchThreadID) {
	if(id.x >= particle_count) return;

	velocities[id.x] = external_force(positions[id.x], velocities[id.x]) * delta_time;

	float prediction_factor = 1.0f / 120.f;
	predicted_positions[id.x] = positions[id.x] + velocities[id.x] * prediction_factor;
}

[numthreads(num_threads, 1, 1)]
void calculate_viscosity (uint3 id : SV_DispatchThreadID) {
	if(id.x >= particle_count) return;

	float2 pos = predicted_positions[id.x];
	int2 origin_cell = get_cell_2d(pos, smoothing_radius);
	float sqr_radius = smoothing_radius * smoothing_radius;

	float2 viscosity_force = 0;
	float2 velocity = velocities[id.x];

	for(uint i = 0 ; i < 9; ++i) {
		uint hash = hash_cell_2d(origin_cell + offsets_2d[i]);
		uint key = key_from_hash(hash, particle_count);
		uint curr_index = spacial_offsets[key];

		while(curr_index < particle_count) {
			spatial_data index_data = spacial_indices[curr_index];

			if(index_data.key != key) break;
			if(index_data.hash != hash) continue;

			uint neighbour_index = index_data.particle_index;

			if(neighbour_index == id.x) continue;

			float2 neighbour_pos = predicted_positions[neighbour_index];
			float2 offset_to_neighbour = neighbour_pos - pos;
			float sqr_dst_to_neighbour = dot(offset_to_neighbour, offset_to_neighbour);

			if(sqr_dst_to_neighbour > sqr_radius) continue;

			float dst = sqrt(sqr_dst_to_neighbour);
			float2 neighbour_velocity = velocities[neighbour_index];
			viscosity_force += (neighbour_velocity - velocity) * smoothing_kernel_poly_6(dst, smoothing_radius);
		}
	}
	velocities[id.x] += viscosity_force * viscosity_strength * delta_time;
}

[numthreads(num_threads, 1, 1)]
void update_spatial_hash (uint3 id : SV_DispatchThreadID) {
	if(id.x >= particle_count) return;

	spacial_offsets[id.x] = particle_count;

	uint index = id.x;
	int2 cell = get_cell_2d(predicted_positions[index], smoothing_radius);
	uint hash = hash_cell_2d(cell);
	uint key = key_from_hash(hash, particle_count);
	spatial_data data;
	data.particle_index = index;
	data.hash = hash;
	data.key = key;
	spacial_indices[id.x] = data;
}

[numthreads(num_threads, 1, 1)]
void calculate_densities (uint3 id : SV_DispatchThreadID) {
	if(id.x >= particle_count) return;

	float2 pos = predicted_positions[id.x];
	densities[id.x] = calculate_density(pos);
}

[numthreads(num_threads, 1, 1)]
void calculate_pressure (uint3 id : SV_DispatchThreadID) {
	if(id.x >= particle_count) return;

	float density = densities[id.x].x;
	float density_near = densities[id.x].y;
	float pressure = pressure_from_density(density);
	float near_pressure = near_pressure_from_density(density_near);
	float2 pressure_force = 0;

	float2 pos = predicted_positions[id.x];
	int2 origin_cell = get_cell_2d(pos, smoothing_radius);
	float sqr_radius = smoothing_radius * smoothing_radius;

	float2 viscosity_force = 0;
	float2 velocity = velocities[id.x];

	for(uint i = 0 ; i < 9; ++i) {
		uint hash = hash_cell_2d(origin_cell + offsets_2d[i]);
		uint key = key_from_hash(hash, particle_count);
		uint curr_index = spacial_offsets[key];

		while(curr_index < particle_count) {
			spatial_data index_data = spacial_indices[curr_index];

			if(index_data.key != key) break;
			if(index_data.hash != hash) continue;

			uint neighbour_index = index_data.particle_index;

			if(neighbour_index == id.x) continue;

			float2 neighbour_pos = predicted_positions[neighbour_index];
			float2 offset_to_neighbour = neighbour_pos - pos;
			float sqr_dst_to_neighbour = dot(offset_to_neighbour, offset_to_neighbour);

			if(sqr_dst_to_neighbour > sqr_radius) continue;

			// Calculate pressure force

			float dst = sqrt(sqr_dst_to_neighbour);
			float2 dir_to_neighbour = dst > 0 ? offset_to_neighbour / dst : float2(0, 1);
			
			float neighbour_density = densities[id.x].x;
			float neighbour_near_density = densities[id.x].y;
			float neighbour_pressure = pressure_from_density(neighbour_density);
			float neighbour_near_pressure = near_pressure_from_density(neighbour_density);

			float shared_pressure = (pressure + neighbour_pressure) * 0.5f;
			float shared_near_pressure = (near_pressure + neighbour_near_pressure) * 0.5f;

			pressure_force += dir_to_neighbour * derivative_spiky_kernel_pow_2(dst, smoothing_radius) * shared_pressure / neighbour_density;
			pressure_force += dir_to_neighbour * derivative_spiky_kernel_pow_3(dst, smoothing_radius) * shared_near_pressure / neighbour_near_density;
		}
	}

	float2 acceleration = pressure_force / density;
	velocities[id.x] += acceleration * delta_time;
}

[numthreads(num_threads, 1, 1)]
void update_positions (uint3 id : SV_DispatchThreadID) {
	if(id.x >= particle_count) return;

	positions[id.x] += velocities[id.x] * delta_time;
	matrices[id.x] = translation_matrix(positions[id.x]);
	handle_collisions(id.x);
}