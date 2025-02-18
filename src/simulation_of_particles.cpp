#include "simulation_of_particles.h"
#include <cmath>
#include <numbers>

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
	f32 volume = std::numbers::pi * pow(radius, 8) / 4;
	f32 value = fmax(0.0f, radius * radius - dst * dst);
	return value * value * value / volume;
}

inline func particle_simulation::calculate_density(v2 sample_point, f32 smoothing_radius) -> f32 {
	auto particles = arena.get_array(this->particles);

	f32 density = 0;
	f32 mass = 1.0f;

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		f32 dst = Length(particles[i].position - sample_point);
		f32 influence = smoothing_kernel(smoothing_radius, dst);
		density += mass * influence;
	}

	return density;
}

template<typename T> func sign(T val) -> bool {
	return (T(0) < val) - (val < T(0));
}

func update_particle(f32 delta_time, particle p, f32 gravity) -> particle {
	p.velocity += V2(0, 1) * gravity * delta_time;
	p.position += p.velocity * delta_time;
	return p;
}

func particle_simulation::update_particles(f32 dt) -> void {
	auto particles = arena.get_array(this->particles);

	for(u32 i = 0 ; i < this->particles.count; ++i) {
		particles[i] = update_particle(dt, particles[i], gravity);
		particles[i] = resolve_collisions(particles[i], particles[i].particle_size);
		v4 ndc_pos = ndc_matrix * V4(V3(particles[i].position, 0.0f), 1.0f);
		arena.get_array(matrices)[i] = translation_matrix(V3(ndc_pos.xy, 0.0f));
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

ID3D12GraphicsCommandList* generate_command_buffer(dx_context *context, memory_arena arena, ID3D12GraphicsCommandList *cmd_list, descriptor_heap heap, rendering_stage rndr_stage)
{
	auto cmd_allocator = get_command_allocator(context->g_frame_index, context->g_command_allocators);
	auto srv_dsc_heap = heap;

	rendering_stage 	stage 		= rndr_stage;
	render_pass 		only_pass 	= arena.load_by_idx(stage.render_passes.ptr, 0);
	graphic_pipeline	g_p 		= arena.load(only_pass.curr_pipeline_g);

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
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmd_list->ClearRenderTargetView
	(
		get_rtv_descriptor_handle(context->g_rtv_descriptor_heap, context->g_frame_index, context->g_rtv_descriptor_size), 
		clearColor, 
		0, 
		nullptr
	);

	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap.addr};
	record_dsc_heap(cmd_list, ppHeaps, _countof(ppHeaps));

	u32 index_count_per_instance = 0;

	g_p.generate_binding_table(context, &srv_dsc_heap, &arena, cmd_list, g_p.bindings);
	
	// Execute bundle
	// cmd_list->ExecuteBundle(entity.bundle);

    // Indicate that the back buffer will now be used to present.
	transition = barrier_transition(context->g_back_buffers[context->g_frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    record_resource_barrier(1, transition, cmd_list);

	//Finally executing the filled Command List into the Command Queue
	return cmd_list;
}

inline func initialize_simulation(dx_context *ctx, u32 particle_count, f32 gravity, f32 collision_damping) -> particle_simulation {
	particle_simulation result = {};
	result.arena				= initialize_arena(Megabytes(1));
	result.particles 			= result.arena.alloc_array<particle>(particle_count);
	result.particles.count		= particle_count;
	result.gravity 				= gravity;
	result.collision_damping 	= collision_damping;
	result.bounds_size			= V2(1024, 1024);

	result.cmd_list 			= create_command_list<ID3D12GraphicsCommandList>(ctx, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	result.simulation_desc_heap = allocate_descriptor_heap(ctx->g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 32);
	result.matrices 			= result.arena.alloc_array<mat4>(particle_count);
	result.matrices.count		= particle_count;

	for(u32 i = 0; i < g_NumFrames; ++i) {
		result.command_allocators[i] = create_command_allocator(ctx->g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	float particle_size = 32.0f;

	u32 particles_row = (i32)sqrt(particle_count);
	u32 particle_per_col  = (particle_count - 1) / particles_row + 1;
	float spacing = particle_size * 2 + 0.4f;

	auto prtcles = result.arena.get_array(result.particles);
	for(u32 i = 0; i < result.particles.count; ++i) {
		prtcles[i].particle_size = particle_size;
		float x = (i % particles_row - particles_row / 2.0f + 0.5f) * spacing;
		float y = (i / particles_row - particle_per_col / 2.0f + 0.5f) * spacing;
		prtcles[i].position.x = x;
		prtcles[i].position.y = y;
	}

	WCHAR shader_path[] = L"shaders.hlsl";
	ID3DBlob* vertex_shader = compile_shader(ctx->g_device, shader_path, "VSMain", "vs_5_0");
	ID3DBlob* pixel_shader 	= compile_shader(ctx->g_device, shader_path, "PSMain", "ps_5_0");

	std::vector<vertex> circ_data 	= generate_circle_vertices(10, 0, 0, 32);
	std::vector<u32> circ_indices 	= generate_circle_indices(32);

	auto mtx_data = result.arena.get_array(result.matrices);

	// NOTE(DH): Create resources
	buffer_cbuf cbuff 		= buffer_cbuf	::	create 	(ctx->g_device, ctx->mem_arena, &result.simulation_desc_heap, &ctx->resources_and_views, 0);
	buffer_vtex mesh		= buffer_vtex	::	create 	(ctx->g_device, ctx->mem_arena, &result.simulation_desc_heap, &ctx->resources_and_views, (u8*)circ_data.data(), circ_data.size() * sizeof(vertex), 64);
	buffer_idex idxs		= buffer_idex	::	create 	(ctx->g_device, ctx->mem_arena, &result.simulation_desc_heap, &ctx->resources_and_views, (u8*)circ_indices.data(), circ_indices.size() * sizeof(u32), 65);
	buffer_1d 	matrices 	= buffer_1d		::	create	(ctx->g_device, ctx->mem_arena, &result.simulation_desc_heap, &ctx->resources_and_views, particle_count, sizeof(mat4), (u8*)mtx_data, 0);

	// NOTE(DH): Push resources in mem arena
	auto cbuff_ptr 	= ctx->mem_arena.push_data(cbuff);
	auto mesh_ptr 	= ctx->mem_arena.push_data(mesh);
	auto idx_ptr 	= ctx->mem_arena.push_data(idxs);
	auto mtx_ptr 	= ctx->mem_arena.push_data(matrices);

	// NOTE(DH): Create bindings, also need to remember that the order MATTER!
	auto binds = mk_bindings()
		.bind_buffer<false, false, false>	(idx_ptr)
		.bind_buffer<false, false, false>	(mesh_ptr)
		.bind_buffer<false, true, false>	(mtx_ptr)
		.bind_buffer<false, true, false>	(cbuff_ptr);
	
	auto binds_ar_ptr = ctx->mem_arena.push_data(binds);
	auto binds_ptr = *(arena_ptr<void>*)(&binds_ar_ptr);

	auto resize = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, u32 width, u32 height, arena_ptr<void> bindings) {
		arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
		auto bnds = ctx->mem_arena.get_ptr(bnds_ptr);
		Resize<decltype(binds)::BUF_TS_U>::resize(bnds->data, ctx->g_device, *arena, heap, &ctx->resources_and_views, width, height); 
	};

	auto generate_binding_table = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
		arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
		auto bnds = ctx->mem_arena.get_ptr(bnds_ptr);
		GCB<decltype(binds)::BUF_TS_U>::bind_root_sig_table(bnds->data, 0, cmd_list, ctx->g_device, heap->addr, arena);
	};

	auto update = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
		arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
		auto bnds = ctx->mem_arena.get_ptr(bnds_ptr);
		Update<decltype(binds)::BUF_TS_U>::update(bnds->data, ctx, &ctx->mem_arena, cmd_list);
	};

	graphic_pipeline graph_pipeline = 
	graphic_pipeline::init__(binds_ptr, resize, generate_binding_table, update)
		.bind_vert_shader	(vertex_shader)
		.bind_frag_shader	(pixel_shader)
		.create_root_sig	(binds, ctx->g_device, &ctx->mem_arena)
		.finalize			(binds, ctx, &ctx->mem_arena, ctx->g_device, &result.simulation_desc_heap);

	result.rndr_stage = 
	rendering_stage::init__(&ctx->mem_arena, 1)
		.bind_graphic_pass(graph_pipeline, &ctx->mem_arena);

	return result;
}