#include "simulation_of_particles.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

// Generate simple circle
std::vector<vertex> generate_circle_vertices(f32 radius, f32 x, f32 y, u32 fragments)
{
	const float PI = 3.1415926f;
	std::vector<vertex> result;

	float increment = 2.0 * PI / fragments;

	result.push_back(vertex{V3(x, y, 0.0), V4(0.5, 0.5, 0, 0)});

	for(float curr_angle = 0.0f; curr_angle <= 2.0 * PI; curr_angle += increment)
	{
		result.push_back(vertex{V3(radius * cos(curr_angle) + x, radius * sin(curr_angle) + y, 0.0), V4(1.0, 1.0, 0, 0)});
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

static inline func smoothing_kernel(f32 radius, f32 dst) -> f32 {
	if(dst >= radius) return 0;
	f32 volume = (std::numbers::pi * pow(radius, 4.0f)) / 6.0f;
	return (radius - dst) * (radius - dst) / volume;
}

static inline func smoothing_kernel_derivative(f32 dst, f32 radius) -> f32 {
	if(dst >= radius) return 0;
	f32 f = radius * radius - dst * dst;
	float scale = 12 / (std::numbers::pi * pow(radius, 4.0f));
	return (dst - radius) * scale;
}

static inline func get_random_dir() -> v2 {
	// NOTE(DH): Randomly create particles
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

inline func particle_simulation::foreach_point_within_radius(v2 sample_point) -> void {
	auto indices = arena.get_array(this->start_indices);
	auto spatial_lookup = arena.get_array(this->spatial_lookup);
	auto points	= arena.get_array(this->p_n_v);

	v2i centre = position_to_cell_coord(sample_point, this->info_for_cshader.smoothing_radius);
	f32 sqr_radius = this->info_for_cshader.smoothing_radius * this->info_for_cshader.smoothing_radius;

	auto cell_offsets = arena.get_array(this->cell_offsets);

	for(u32 i = 0; i < this->cell_offsets.count; ++i) {
		u32 key = get_key_from_hash(hash_cell(centre.x + cell_offsets[i].x, centre.y + cell_offsets[i].y), this->cell_offsets.count);
		i32 cell_start_index = indices[key];

		for(i32 j = cell_start_index; j < this->spatial_lookup.count; ++j) {
			if(spatial_lookup[j].cell_key != key) break;

			u32 particle_index  = spatial_lookup[j].particle_index;
			f32 sqr_dst = Length(points[particle_index].position - sample_point);

			if(sqr_dst <= sqr_radius) {
				// NOTE(DH): Test if the point is inside the radius
			}
		}
	}
}

inline func particle_simulation::update_spatial_lookup(f32 radius) -> void {
	auto indices = arena.get_array(this->start_indices);
	auto lookup = arena.get_array(this->spatial_lookup);
	auto points	= arena.get_array(this->p_n_v);

	for(u32 i = 0 ; i < this->p_n_v.count; ++i) {
		v2i cell = position_to_cell_coord(points[i].position, radius);
		u32 cell_key = get_key_from_hash(hash_cell(cell.x, cell.y), this->p_n_v.count);
		lookup[i] = {.particle_index = i, .cell_key = cell_key};
		indices[i] = 0xFFFFFFFF;
	}

	auto sort_func = [](spatial_data a, spatial_data b) {
		return a.cell_key < b.cell_key;
	};

	std::sort(lookup, lookup + this->spatial_lookup.count, sort_func);

	for(u32 i = 0; i < this->p_n_v.count; ++i) {
		u32 key = lookup[i].cell_key;
		u32 key_prev = (i == 0) ? 0xFFFFFFFF : lookup[i - 1].cell_key;
		if(key != key_prev) {
			indices[key] = i;
		}
	}
}

inline func particle_simulation::calculate_shared_pressure(f32 density_a, f32 density_b) -> f32 {
	f32 pressure_A = convert_density_to_pressure(density_a);
	f32 pressure_B = convert_density_to_pressure(density_b);
	return (pressure_A + pressure_B) / 2;
}

inline func particle_simulation::calculate_pressure_force(u32 particle_idx, f32 smoothing_radius) -> v2 {
	auto particles = arena.get_array(this->particles);
	auto positions = arena.get_array(this->p_n_v);
	auto prprtes   = arena.get_array(this->particle_properties);
	auto densities = arena.get_array(this->densities);

	v2 pressure_force = {};

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		if(particle_idx == i) continue;
		v2 offset = positions[i].position - positions[particle_idx].position;
		f32 dst = Length(offset);
		v2 dir = (dst <= 0.00001f) ?  get_random_dir() : offset * (1.0f / dst);

		f32 slope = smoothing_kernel_derivative(dst, smoothing_radius);
		f32 density = densities[i];
		f32 shared_pressure = calculate_shared_pressure(density, densities[particle_idx]);
		pressure_force.x += SafeRatio0(shared_pressure * dir.x * slope, density);
		pressure_force.y += SafeRatio0(shared_pressure * dir.y * slope, density);
	}

	return pressure_force;
}

inline func particle_simulation::calculate_density(v2 sample_point, f32 smoothing_radius) -> f32 {
	auto particles = arena.get_array(this->particles);
	auto positions = arena.get_array(this->p_n_v);

	f32 density = 0;
	f32 mass = 1.0f;

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		f32 dst = Length(positions[i].position - sample_point);
		f32 influence = smoothing_kernel(smoothing_radius, dst);
		density += mass * influence;
	}

	return density;
}

inline func particle_simulation::calculate_property(v2 sample_point, f32 smoothing_radius) -> f32 {
	auto particles = arena.get_array(this->particles);
	auto positions = arena.get_array(this->p_n_v);
	auto prprtes   = arena.get_array(this->particle_properties);
	auto densities = arena.get_array(this->densities);

	f32 property = 0;
	f32 mass = 1.0f;

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		f32 dst = Length(positions[i].position - sample_point);
		f32 influence = smoothing_kernel(smoothing_radius, dst);
		f32 density = densities[i];
		property += SafeRatio0(prprtes[i] * mass * influence, density);
	}

	return property;
}

inline func particle_simulation::convert_density_to_pressure(f32 density) -> f32 {
	f32 density_error = density - target_density;
	f32 pressure = density_error * pressure_multiplier;
	return pressure;
}

func sign(f32 val) -> f32 {
	
	if(val > 0) return 1;
	else if(val < 0) return -1.0f;
	return 0;
}

func update_particle(f32 delta_time, pos_and_vel p_n_v, f32 gravity) -> pos_and_vel {
	p_n_v.velocity += V2(0.0, -1.0) * gravity * delta_time;
	p_n_v.position += p_n_v.velocity * delta_time;
	return p_n_v;
}

func particle_simulation::update_particles(f32 dt) -> void {
	auto particles = arena.get_array(this->particles);
	auto positions = arena.get_array(this->p_n_v);

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		positions[i] = update_particle(dt, positions[i], gravity);
		positions[i] = resolve_collisions(positions[i], particles[i].particle_size);
		arena.get_array(matrices)[i] = translation_matrix(V3(positions[i].position, 0.0f));
	}
}

func particle_simulation::resolve_collisions(pos_and_vel data, f32 particle_size) -> pos_and_vel {
	v2 half_bound_size = this->bounds_size * 0.5f - V2(particle_size, particle_size);

	if(abs(data.position.x) > half_bound_size.x) {
		data.position.x = half_bound_size.x * sign(data.position.x);
		data.velocity.x *= -1 * collision_damping;
	}
	if(abs(data.position.y) > half_bound_size.y) {
		f32 signe = sign(-1.0f);
		signe = sign(0.0f);
		signe = sign(1.0f);
		data.position.y = half_bound_size.y * sign(data.position.y);
		data.velocity.y *= -1 * collision_damping;
	}

	return data;
}

ID3D12GraphicsCommandList* generate_compute_command_buffer(dx_context *ctx, memory_arena arena, arena_array<resource_and_view> r_n_v, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage, u32 width, u32 height)
{
	auto cmd_allocator = get_command_allocator(ctx->g_frame_index, ctx->g_command_allocators);
	auto srv_dsc_heap = heap;

	rendering_stage 	stage 		= rndr_stage;
	render_pass 		rndr_pass	= arena.load_by_idx(stage.render_passes.ptr, 1);
	compute_pipeline	c_p_1 		= arena.load(rndr_pass.curr_pipeline_c);

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

ID3D12GraphicsCommandList* generate_command_buffer(dx_context *context, memory_arena arena, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage)
{
	auto cmd_allocator = get_command_allocator(context->g_frame_index, context->g_command_allocators);
	auto srv_dsc_heap = heap;

	rendering_stage 	stage 		= rndr_stage;
	render_pass 		only_pass 	= arena.load_by_idx(stage.render_passes.ptr, 0);
	graphic_pipeline	g_p 		= arena.load(only_pass.curr_pipeline_g);

	//NOTE(DH): Reset command allocators for the next frame
	// record_reset_cmd_allocator(cmd_allocator);

	//NOTE(DH): Reset command lists for the next frame
	// record_reset_cmd_list(cmd_list, cmd_allocator, g_p.state);
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
    // const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    // cmd_list->ClearRenderTargetView
	// (
	// 	get_rtv_descriptor_handle(context->g_rtv_descriptor_heap, context->g_frame_index, context->g_rtv_descriptor_size), 
	// 	clearColor, 
	// 	0, 
	// 	nullptr
	// );

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

inline func particle_simulation::simulation_step(f32 delta_time, u32 width, u32 height) -> void {
	auto dnsties	= arena.get_array(densities);
	auto particles = arena.get_array(this->particles);
	auto positions = arena.get_array(this->p_n_v);

	update_spatial_lookup(this->info_for_cshader.smoothing_radius);

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		v2 position = positions[i].position;
		v2 velocity = positions[i].velocity;
		velocity += V2(0.0, 1.0f) * gravity * delta_time;
		positions[i].velocity = velocity;
	}

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		dnsties[i] = calculate_density(positions[i].position, this->info_for_cshader.smoothing_radius);
	}

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		v2 pressure_force = calculate_pressure_force(i, this->info_for_cshader.smoothing_radius);
		v2 pressure_acceleration = pressure_force * (1.0f /  dnsties[i]);
		positions[i].velocity += pressure_acceleration * delta_time;
	}

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		positions[i].position += positions[i].velocity * delta_time;
		positions[i] = resolve_collisions(positions[i], particles[i].particle_size);

		arena.get_array(matrices)[i] = translation_matrix(V3(positions[i].position, 0.0f));
	}
}

inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation {
	particle_simulation result = {};
	result.arena				= initialize_arena(Megabytes(64));

	result.resources_and_views	= result.arena.alloc_array<resource_and_view>(1024);
	result.particles 			= result.arena.alloc_array<particle>(particle_count);
	result.particles.count		= particle_count;
	result.matrices 			= result.arena.alloc_array<mat4>(particle_count);
	result.matrices.count		= particle_count;
	result.p_n_v 				= result.arena.alloc_array<pos_and_vel>(particle_count);
	result.p_n_v.count 			= particle_count;

	result.particle_properties 	= result.arena.alloc_array<f32>(particle_count);
	result.particle_properties.count = particle_count;
	
	result.densities 			= result.arena.alloc_array<f32>(particle_count);
	result.densities.count 		= particle_count;
	
	result.final_gradient 		= result.arena.alloc_array<f32>(2560 * 1440);
	result.final_gradient.count = 2560 * 1440;

	result.cell_offsets			= result.arena.alloc_array<v2i>(2048);

	result.gravity 				= gravity;
	result.collision_damping 	= collision_damping;
	result.bounds_size			= V2(18.0f, 10.0f);

	result.info_for_cshader		= {};
	result.info_for_cshader.particle_count = particle_count;
	result.info_for_cshader.smoothing_radius = 0.6f;

	result.cmd_list 			= create_command_list<ID3D12GraphicsCommandList>(ctx, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	result.simulation_desc_heap = allocate_descriptor_heap(ctx->g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 32);

	for(u32 i = 0; i < g_NumFrames; ++i) {
		result.command_allocators[i] = create_command_allocator(ctx->g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	float particle_size = 0.08f;

	u32 particles_row = (i32)sqrt(particle_count);
	u32 particle_per_col  = (particle_count - 1) / particles_row + 1;
	float spacing = particle_size * 2 + 0.1f;

	auto prtcles 	= result.arena.get_array(result.particles);
	auto p_n_vs		= result.arena.get_array(result.p_n_v);
	auto prprtes	= result.arena.get_array(result.particle_properties);
	auto final_grad	= result.arena.get_array(result.final_gradient);
	auto densities	= result.arena.get_array(result.densities);

	// NOTE(DH): Randomly create particles
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> distrib(0, 1.0f);

	for(u32 i = 0; i < result.particles.count; ++i) {
		// float x = (distrib(gen) - 0.5f) * result.bounds_size.x;
		// float y = (distrib(gen) - 0.5f) * result.bounds_size.y;

		float x = (i % particles_row - particles_row / 2.0f + 0.5f) * spacing;
		float y = (i / particles_row - particle_per_col / 2.0f + 0.5f) * spacing;
		
		p_n_vs[i].position.x = x;
		p_n_vs[i].position.y = y;

		prtcles[i].pos_and_velocity_idx = i;
		prtcles[i].particle_size = particle_size;

		prprtes[i] = example_function(p_n_vs[i].position);
	}

	for(u32 i = 0 ; i < particle_count; ++i) {
		densities[i] = result.calculate_density(p_n_vs[i].position, result.info_for_cshader.smoothing_radius);
	}
	
	// u32 width = 2560;
	// u32 height = 1440;
	// f32 aspect = ((f32)width / (f32)height);
	// f32 scale = 5.0f;
	// f32 x_advance = (2.0 / width * scale * aspect);
	// f32 y_advance = (2.0 / height * scale);
	
	// for(u32 i = 0; i < height; ++i) {
	// 	for(u32 j = 0; j < width; ++j) {
	// 		final_grad[j + (i32)width * i] = result.calculate_density(V2(-scale * aspect + ((f32)j * x_advance), -scale + ((f32)i * y_advance)), 0.6f);
	// 		if(final_grad[j + (i32)width * i] > 0.5f) {
	// 			int ar = 5;
	// 		}
	// 	}
	// }

	// u32 width = 2560;
	// u32 height = 1440;
	// f32 aspect = ((f32)width / (f32)height);
	// mat4 view_matrix = look_at_matrix(V3(0.0f, 0.0f, 1.0f), V3(0, 0, 0), V3(0, 1, 0));
	// mat4 ortho_projection = create_ortho_matrix(1.0f, aspect, 1.0f, -1.0, -1.0, 1.0f, 0.0f, 0.0f) * view_matrix;
	// f32 x_advance = (2.0 / width);
	// f32 y_advance = (2.0 / height);
	// for(u32 i = 0; i < height; ++i) {
	// 	for(u32 j = 0; j < width; ++j) {
	// 		v4 converted_pos = ortho_projection * V4(j, i, 0.0f, 1.0f);
	// 		densities[j + (i32)width * i] = result.calculate_density(converted_pos.xy, 0.6f);
	// 	}
	// }
	
	// for(u32 i = 0; i < height; ++i) {
	// 	for(u32 j = 0; j < width; ++j) {
	// 		v4 converted_pos = ortho_projection * V4(j, i, 0.0f, 1.0f);
	// 		final_grad[j + (i32)width * i] = result.calculate_property(converted_pos.xy, 0.6f);
	// 	}
	// }


	// NOTE(DH): Create particles in cols and rows evenly spaced
	// for(u32 i = 0; i < result.particles.count; ++i) {
	// 	float x = (i % particles_row - particles_row / 2.0f + 0.5f) * spacing;
	// 	float y = (i / particles_row - particle_per_col / 2.0f + 0.5f) * spacing;
		
	// 	p_n_vs[i].position.x = x;
	// 	p_n_vs[i].position.y = y;

	// 	prtcles[i].pos_and_velocity_idx = i;
	// 	prtcles[i].particle_size = particle_size;
	// }

	WCHAR shader_path[] = L"shaders.hlsl";
	ID3DBlob* vertex_shader = compile_shader(ctx->g_device, shader_path, "VSMain", "vs_5_0");
	ID3DBlob* pixel_shader 	= compile_shader(ctx->g_device, shader_path, "PSMain", "ps_5_0");

	std::vector<vertex> circ_data 	= generate_circle_vertices(particle_size, 0, 0, 64);
	std::vector<u32> circ_idc 	= generate_circle_indices(64);

	auto mtx_data = result.arena.get_array(result.matrices);

	// NOTE(DH): Graphics pipeline
	{
		// NOTE(DH): Create resources
		buffer_cbuf cbuff 		= buffer_cbuf	::	create 	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)&ctx->common_cbuff_data,0);
		buffer_vtex mesh		= buffer_vtex	::	create 	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)circ_data.data(), circ_data.size() * sizeof(vertex), 64);
		buffer_idex idxs		= buffer_idex	::	create 	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)circ_idc.data(), circ_idc.size() * sizeof(u32), particle_count, 65);
		buffer_1d 	matrices 	= buffer_1d		::	create	(ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(mat4), (u8*)mtx_data, 0);

		// NOTE(DH): Push resources in mem arena
		auto cbuff_ptr 	= result.arena.push_data(cbuff);
		auto mesh_ptr 	= result.arena.push_data(mesh);
		auto idx_ptr 	= result.arena.push_data(idxs);
		auto mtx_ptr 	= result.arena.push_data(matrices);

		// NOTE(DH): Create bindings, also need to remember that the order MATTER!
		auto binds = mk_bindings()
			.bind_buffer<false, false, false>	(idx_ptr)
			.bind_buffer<false, false, false>	(mesh_ptr)
			.bind_buffer<false, true, false>	(mtx_ptr)
			.bind_buffer<false, true, false>	(cbuff_ptr);
		
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
		rendering_stage::init__(&result.arena, 2)
			.bind_graphic_pass(graph_pipeline, &result.arena);
	}

	// NOTE(DH): Compute shader
	{
		WCHAR filename[] = L"particle_simulation.hlsl";
		ID3DBlob* compute_shader = compile_shader(ctx->g_device, filename, "CSMain", "cs_5_0");

		render_target2d render_target 	= render_target2d	::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, ctx->viewport.Width, ctx->viewport.Height, 0);
		buffer_1d 		possns 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(v4), (u8*)p_n_vs, 1);
		buffer_1d 		dnsts 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(f32), (u8*)densities, 2);
		// buffer_1d 		prprts 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, width * height, sizeof(f32), (u8*)final_grad, 3);
		buffer_1d 		prprts 			= buffer_1d			::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, particle_count, sizeof(f32), (u8*)prprtes, 3);
		buffer_cbuf		cbuff2 			= buffer_cbuf		::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)&ctx->common_cbuff_data, 0);
		buffer_cbuf		cbuff 			= buffer_cbuf		::create (ctx->g_device, result.arena, &result.simulation_desc_heap, &result.resources_and_views, (u8*)&result.info_for_cshader, 1);

		auto binds = mk_bindings()
			.bind_buffer<true, false, true>(result.arena.push_data(render_target))
			.bind_buffer<false, true, false>(result.arena.push_data(possns))
			.bind_buffer<false, true, false>(result.arena.push_data(dnsts))
			.bind_buffer<false, true, false>(result.arena.push_data(prprts))
			.bind_buffer<false, true, false>(result.arena.push_data(cbuff2))
			.bind_buffer<false,true, false>(result.arena.push_data(cbuff));

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

		compute_pipeline compute_pipeline = 
		compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target)
			.bind_shader		(compute_shader)
			.create_root_sig	(binds, ctx->g_device, &result.arena)
			.finalize			(binds, ctx, &result.arena, ctx->resources_and_views, ctx->g_device, &result.simulation_desc_heap);

		result.rndr_stage
			.bind_compute_pass(compute_pipeline, &result.arena);
	}

	return result;
}