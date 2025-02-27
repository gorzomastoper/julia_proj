#include "simulation_of_particles.h"
#include "util/types.h"
#include <algorithm>
#include <cmath>
#include <numbers>

v2 calculate_circle_uv(f32 radius, f32 angle, f32 max_radius) {
	f32 v = radius / max_radius;
	f32 u = angle / (2.0f * std::numbers::pi);
	return V2(u,v);
}

// Generate simple circle
std::vector<vertex> generate_circle_vertices(f32 radius, f32 x, f32 y, u32 fragments)
{
	const float PI = 3.1415926f;
	std::vector<vertex> result;

	float increment = 2.0 * PI / fragments;

	result.push_back(vertex{V3(x, y, 0.0), V4(0.5, 0.5, 0, 0)});

	for(float curr_angle = 0.0f; curr_angle < 2.0 * PI; curr_angle += increment)
	{
		v2 uv = calculate_circle_uv(radius, curr_angle, radius * 4.0f);
		result.push_back(vertex{V3(radius * cos(curr_angle) + x, radius * sin(curr_angle) + y, 0.0), V4(uv.x, uv.y, 0, 0)});
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

func particle_simulation::particle_sim_start_frame(u32 frame_idx, ID3D12GraphicsCommandList *cmd_list, ID3D12CommandAllocator **cmd_allocator, ID3D12PipelineState *pipeline_state) -> void {
	//NOTE(DH): Reset command allocators for the next frame
	record_reset_cmd_allocator(command_allocators[frame_idx]);

	//NOTE(DH): Reset command lists for the next frame
	record_reset_cmd_list(cmd_list, command_allocators[frame_idx], pipeline_state);
}

ID3D12GraphicsCommandList* generate_command_buffer(dx_context *context, memory_arena arena, ID3D12GraphicsCommandList *cmd_list, ID3D12CommandAllocator **cmd_allocators, descriptor_heap heap, rendering_stage rndr_stage)
{
	auto cmd_allocator = get_command_allocator(context->g_frame_index, cmd_allocators);
	auto srv_dsc_heap = heap;

	rendering_stage 	stage 		= rndr_stage;
	render_pass 		only_pass 	= arena.load_by_idx(stage.render_passes.ptr, 8);
	graphic_pipeline	graph_pipe 	= arena.load(only_pass.graph.curr);

	cmd_list->SetName(L"GRAPHICS COMMAND LIST");

	// NOTE(DH): Set root signature and pipeline state
	cmd_list->SetGraphicsRootSignature(graph_pipe.root_signature);
	cmd_list->SetPipelineState(graph_pipe.state);

	cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	record_viewports(1, context->viewport, cmd_list);
	record_scissors(1, context->scissor_rect, cmd_list);

    // Indicate that the back buffer will be used as a render target.
	auto transition = barrier_transition(context->g_back_buffers[context->g_frame_index],D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	record_resource_barrier(1, transition, cmd_list);

	set_render_target(cmd_list, context->g_rtv_descriptor_heap, 1, FALSE, context->g_frame_index, context->g_rtv_descriptor_size);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.0f, 0.06f, 1.0f };
    cmd_list->ClearRenderTargetView
	(
		get_rtv_descriptor_handle(context->g_rtv_descriptor_heap, context->g_frame_index, context->g_rtv_descriptor_size), 
		clearColor, 
		0, 
		nullptr
	);

	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap.addr};
	record_dsc_heap(cmd_list, ppHeaps, _countof(ppHeaps));

	graph_pipe.generate_binding_table(context, &srv_dsc_heap, &arena, cmd_list, graph_pipe.bindings);
	
    // Indicate that the back buffer will now be used to present.
	transition = barrier_transition(context->g_back_buffers[context->g_frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    record_resource_barrier(1, transition, cmd_list);

	//Finally executing the filled Command List into the Command Queue
	return cmd_list;
}

inline func update_settings(particle_simulation* sim, f32 delta_time, v2 mouse_pos, bool is_left_mouse, bool is_right_mouse) -> void {
	sim->info_for_cshader.smoothing_kernel_poly_6_scaling_factor 		= 4.0f / (std::numbers::pi * pow(sim->info_for_cshader.smoothing_radius, 8));
	sim->info_for_cshader.spiky_kernel_pow_3_scaling_factor 			= 10.0f / (std::numbers::pi * pow(sim->info_for_cshader.smoothing_radius, 5));
	sim->info_for_cshader.spiky_kernel_pow_2_scaling_factor 			= 6.0f / (std::numbers::pi * pow(sim->info_for_cshader.smoothing_radius, 4));
	sim->info_for_cshader.derivative_spiky_kernel_pow_3_scaling_factor 	= 30.0f / (std::numbers::pi * pow(sim->info_for_cshader.smoothing_radius, 5));
	sim->info_for_cshader.spiky_kernel_pow_2_scaling_factor 			= 12.0f / (std::numbers::pi * pow(sim->info_for_cshader.smoothing_radius, 4));

	f32 current_interact_strength = 0;
	if(is_left_mouse || is_right_mouse) {
		current_interact_strength = is_left_mouse ? -sim->interaction_strength : sim->interaction_strength;
	}
	sim->info_for_cshader.pull_push_strength = current_interact_strength;
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

inline func particle_simulation::simulation_step(dx_context *ctx, f32 delta_time, u32 width, u32 height, v2 mouse_pos, bool is_left_mouse, bool is_right_mouse) -> void {
	this->sim_data_counter++;
	this->info_for_cshader.delta_time = delta_time;
	auto sorting_infos = arena.get_array(this->sorting_infos);
	// NOTE(DH): Init resources and get all pipelines, set initial states {
	rendering_stage 	stage 						= rndr_stage;
	compute_pipeline	external_forces_pipeline 	= arena.load(arena.load_by_idx(stage.render_passes.ptr, 0).compute.curr);
	compute_pipeline	density_pipeline 			= arena.load(arena.load_by_idx(stage.render_passes.ptr, 1).compute.curr);
	compute_pipeline	pressure_pipeline 			= arena.load(arena.load_by_idx(stage.render_passes.ptr, 2).compute.curr);
	compute_pipeline	viscosity_pipeline 			= arena.load(arena.load_by_idx(stage.render_passes.ptr, 3).compute.curr);
	compute_pipeline	update_positions_pipeline 	= arena.load(arena.load_by_idx(stage.render_passes.ptr, 4).compute.curr);
	compute_pipeline	spatial_hash_pipeline 		= arena.load(arena.load_by_idx(stage.render_passes.ptr, 5).compute.curr);
	compute_pipeline	gpu_sort_pipeline 			= arena.load(arena.load_by_idx(stage.render_passes.ptr, 6).compute.curr);
	compute_pipeline	gpu_offsets_pipeline 		= arena.load(arena.load_by_idx(stage.render_passes.ptr, 7).compute.curr);

	// cmd_list->SetName(L"PARTICLE SIM2D COMMAND LIST");

	// NOTE(DH): Set root signature and pipeline state
	cmd_list->SetComputeRootSignature(external_forces_pipeline.root_signature);
	cmd_list->SetPipelineState(external_forces_pipeline.state);

	ID3D12DescriptorHeap* ppHeaps[] = { this->simulation_desc_heap.addr};
	record_dsc_heap(cmd_list, ppHeaps, _countof(ppHeaps));

	external_forces_pipeline.generate_binding_table(ctx, &simulation_desc_heap, &arena, cmd_list, external_forces_pipeline.bindings);
	// NOTE(DH): Init resources and get all pipelines, set initial states }

	// DISPATCH EXTERNAL FORCES
	cmd_list->Dispatch(positions.count, 1, 1);

	// DISPATCH SPATIAL HASH
	cmd_list->SetPipelineState(spatial_hash_pipeline.state);
	cmd_list->Dispatch(positions.count, 1, 1);

	// // DISPATCH SORTING AND CALC OFFSETS
	// info_for_sorting.num_entries = positions.count;
	cmd_list->SetComputeRootSignature(gpu_sort_pipeline.root_signature);
	cmd_list->SetPipelineState(gpu_sort_pipeline.state);

	gpu_sort_pipeline.generate_binding_table(ctx, &simulation_desc_heap, &arena, cmd_list, gpu_sort_pipeline.bindings);

	u32 num_stages = log2f(next_power_of_two(positions.count));
	u32 num_of_dispatches = (num_stages * (num_stages + 1)) / 2;
	cmd_list->Dispatch((next_power_of_two(positions.count) / 2) * num_of_dispatches, 1, 1);

	cmd_list->SetComputeRootSignature(gpu_offsets_pipeline.root_signature);
	cmd_list->SetPipelineState(gpu_offsets_pipeline.state);
	cmd_list->Dispatch(positions.count, 1, 1);
	
	// // DISPATCH DENSITY CALC
	cmd_list->SetComputeRootSignature(density_pipeline.root_signature);
	cmd_list->SetPipelineState(density_pipeline.state);
	
	density_pipeline.generate_binding_table(ctx, &simulation_desc_heap, &arena, cmd_list, density_pipeline.bindings);
	cmd_list->Dispatch(positions.count, 1, 1);

	// // DISPATCH PRESSURE CALC
	// cmd_list->SetPipelineState(pressure_pipeline.state);
	// cmd_list->Dispatch(positions.count, 1, 1);

	// // DISPATCH VISCOSITY CALC
	// cmd_list->SetPipelineState(viscosity_pipeline.state);
	// cmd_list->Dispatch(positions.count, 1, 1);

	// // DISPATCH UPDATE POSITIONS
	cmd_list->SetPipelineState(update_positions_pipeline.state);
	cmd_list->Dispatch(positions.count, 1, 1);
}

static inline func initialize_simulation(dx_context *ctx, u32 particle_count) -> particle_simulation {

	// NOTE(DH): Initialize resources
	particle_simulation sim 		= {};
	sim.arena						= initialize_arena(Megabytes(32));

	sim.resources_and_views			= sim.arena.alloc_array<resource_and_view>(32);

	sim.matrices 					= sim.arena.alloc_array<mat4>(particle_count);
	sim.matrices.count				= particle_count;

	sim.positions 					= sim.arena.alloc_array<v2>(particle_count);
	sim.positions.count 			= particle_count;

	sim.velocities 					= sim.arena.alloc_array<v2>(particle_count);
	sim.velocities.count 			= particle_count;

	sim.predicted_positions			= sim.arena.alloc_array<v2>(particle_count);
	sim.predicted_positions.count 	= particle_count;

	sim.particle_properties 		= sim.arena.alloc_array<f32>(particle_count);
	sim.particle_properties.count 	= particle_count;
	
	sim.densities 					= sim.arena.alloc_array<v2>(particle_count);
	sim.densities.count 			= particle_count;

	sim.spatial_lookup				= sim.arena.alloc_array<spatial_data>(particle_count);
	sim.spatial_lookup.count		= particle_count;

	sim.start_indices				= sim.arena.alloc_array<i32>(particle_count);
	sim.start_indices.count			= particle_count;

	sim.sorting_infos				= sim.arena.alloc_array<sorting_info>(particle_count);
	sim.sorting_infos.count			= particle_count;

	sim.info_for_cshader			= {};
	sim.info_for_cshader.particle_count 		= particle_count;
	sim.info_for_cshader.smoothing_radius 		= 0.3;
	sim.info_for_cshader.max_velocity 			= 1.0f;
	sim.info_for_cshader.target_density 		= 1.5f;
	sim.info_for_cshader.pressure_multiplier 	= 0.0f;
	sim.info_for_cshader.bounds_size			= V2(18.0f, 10.0f);

	sim.info_for_sorting.group_width = 1;
	sim.info_for_sorting.group_height = 1;
	sim.info_for_sorting.step_index = 0;
	sim.info_for_sorting.num_entries = 1;

	sim.cmd_list 				= create_command_list<ID3D12GraphicsCommandList>(ctx, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	sim.simulation_desc_heap 	= allocate_descriptor_heap(ctx->g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 32);

	for(u32 i = 0; i < g_NumFrames; ++i) {
		sim.command_allocators[i] = create_command_allocator(ctx->g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	sim.particle_size = 0.04f;

	u32 particles_row = (i32)sqrt(particle_count);
	u32 particle_per_col  = (particle_count - 1) / particles_row + 1;
	float spacing = sim.particle_size * 2 + 0.03f;

	auto positions				= sim.arena.get_array(sim.positions);
	auto densities				= sim.arena.get_array(sim.densities);
	auto velocities 			= sim.arena.get_array(sim.velocities);
	auto predicted_positions 	= sim.arena.get_array(sim.predicted_positions);
	auto spacial_indices		= sim.arena.get_array(sim.start_indices);
	auto spacial_offsets		= sim.arena.get_array(sim.spatial_lookup);
	auto sorting_infos			= sim.arena.get_array(sim.sorting_infos);
	auto matrices				= sim.arena.get_array(sim.matrices);

	u32 idx_of_dispatch = 0;
	u32 num_stages = log2f(next_power_of_two(sim.positions.count));
	u32 num_of_dispatches = (num_stages * (num_stages + 1)) / 2;
	sim.info_for_sorting.num_entries 	= sim.positions.count;
	for(u32 stage_idx = 0; stage_idx < num_stages; ++stage_idx) {
		for(u32 step_idx = 0; step_idx < stage_idx + 1; ++step_idx) {
			u32 group_width = 1 << (stage_idx - step_idx);
			u32 group_height = 2 * group_width - 1;
			sim.info_for_sorting.group_width 	= group_width;
			sim.info_for_sorting.group_height 	= group_height;
			sim.info_for_sorting.step_index 	= step_idx;
			sim.info_for_sorting.nums_per_dispatch = num_of_dispatches;

			sorting_infos[idx_of_dispatch] = sim.info_for_sorting;
			++idx_of_dispatch;
		}
	}

	for(u32 i = 0; i < particle_count; ++i) {

		float x = (i % particles_row - particles_row / 2.0f + 0.5f) * spacing;
		float y = (i / particles_row - particle_per_col / 2.0f + 0.5f) * spacing;
		
		positions[i] = V2(x, y);
	}

	buffer_1d 		matrices_buffer				= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(mat4),			(u8*)matrices, 				6);
	buffer_1d 		spacial_indices_buffer		= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(spatial_data), 	(u8*)spacial_indices, 		4);
	buffer_1d 		spacial_offsets_buffer		= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(u32), 			(u8*)spacial_offsets, 		5);
	// NOTE(DH): Compute shader
	{
		sim.rndr_stage = rendering_stage::init__(&sim.arena, 9);

		WCHAR filename[] = L"fluid_sim_2d.hlsl";
		ID3DBlob* external_forces_shader = compile_shader(ctx->g_device, filename, "external_forces", "cs_5_0");
		ID3DBlob* calculate_viscosity_shader = compile_shader(ctx->g_device, filename, "calculate_viscosity", "cs_5_0");
		ID3DBlob* calculate_spatial_hash_shader = compile_shader(ctx->g_device, filename, "update_spatial_hash", "cs_5_0");
		ID3DBlob* calculate_densities_shader = compile_shader(ctx->g_device, filename, "calculate_densities", "cs_5_0");
		ID3DBlob* calculate_pressure_shader = compile_shader(ctx->g_device, filename, "calculate_pressure", "cs_5_0");
		ID3DBlob* update_positions_shader = compile_shader(ctx->g_device, filename, "update_positions", "cs_5_0");

		WCHAR gpu_sort_filename[] = L"bitonic_merge_sort.hlsl";
		ID3DBlob* sort_shader = compile_shader(ctx->g_device, gpu_sort_filename, "Sort", "cs_5_0");
		ID3DBlob* calculate_offsets_shader = compile_shader(ctx->g_device, gpu_sort_filename, "Calculate_Offsets", "cs_5_0");

		buffer_1d 		possitions_buffer 			= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(v2), 			(u8*)positions, 			0);
		buffer_1d 		predicted_positions_buffer	= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(v2), 			(u8*)predicted_positions, 	1);
		buffer_1d 		velocities_buffer			= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(v2),			(u8*)velocities, 			2);
		buffer_1d 		densities_buffer 			= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(v2), 			(u8*)densities, 			3);
		buffer_cbuf		particle_info_buffer 		= buffer_cbuf		::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, (u8*)&sim.info_for_cshader, 0);

		// NOTE(DH): GPU Fluid 2D Sim {
		{
			auto binds = mk_bindings()
				.bind_buffer<false, false, false, false>	(sim.arena.push_data(possitions_buffer			))
				.bind_buffer<false, false, false, false>	(sim.arena.push_data(predicted_positions_buffer	))
				.bind_buffer<false, false, false, false>	(sim.arena.push_data(velocities_buffer			))
				.bind_buffer<false, false, false, false>	(sim.arena.push_data(densities_buffer			))
				.bind_buffer<false, true, false, false>		(sim.arena.push_data(spacial_indices_buffer		))
				.bind_buffer<false, true, false, false>		(sim.arena.push_data(spacial_offsets_buffer		))
				.bind_buffer<false, false, false, true>		(sim.arena.push_data(matrices_buffer			))
				.bind_buffer<false, true, false, false>		(sim.arena.push_data(particle_info_buffer		));

			auto binds_ar_ptr = sim.arena.push_data(binds);
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

			auto copy_screen_to_render_target = [](dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
				return;
			};

			compute_pipeline external_forces_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(external_forces_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			compute_pipeline spatial_hash_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(calculate_spatial_hash_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			compute_pipeline density_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(calculate_densities_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			compute_pipeline pressure_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(calculate_pressure_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			compute_pipeline viscosity_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(calculate_viscosity_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			compute_pipeline update_positions_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(update_positions_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);
				
			sim.rndr_stage = sim.rndr_stage
			.bind_compute_pass(external_forces_pipeline, &sim.arena) // 0
			.bind_compute_pass(density_pipeline, &sim.arena) // 1
			.bind_compute_pass(pressure_pipeline, &sim.arena) // 2
			.bind_compute_pass(viscosity_pipeline, &sim.arena) // 3
			.bind_compute_pass(update_positions_pipeline, &sim.arena) //4
			.bind_compute_pass(spatial_hash_pipeline, &sim.arena); // 5
		}
		// NOTE(DH): GPU Fluid 2D Sim }

		// NOTE(DH): GPU Bitonic sorting {
		{
			buffer_1d 		sorting_infos_buffer		= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(sorting_info), 	(u8*)sorting_infos, 		0);
			// buffer_1d 		spacial_indices_buffer		= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(spatial_data), 	(u8*)spacial_indices, 		0);
			// buffer_1d 		spacial_offsets_buffer		= buffer_1d			::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(u32), 			(u8*)spacial_offsets, 		1);
			// buffer_cbuf		sorting_info_buffer 		= buffer_cbuf		::create (ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, (u8*)&sim.info_for_sorting, 0);

			auto binds = mk_bindings()
			.bind_buffer<false,false, false, false>		(sim.arena.push_data(spacial_indices_buffer		))
			.bind_buffer<false,false, false, false>		(sim.arena.push_data(spacial_offsets_buffer		))
			.bind_buffer<false,false, false, false>		(sim.arena.push_data(sorting_infos_buffer		));

			auto binds_ar_ptr = sim.arena.push_data(binds);
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

			auto copy_screen_to_render_target = [](dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
				return;
			};

			compute_pipeline gpu_sort_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(sort_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			compute_pipeline gpu_offsets_pipeline = 
			compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target, copy_screen_to_render_target)
				.bind_shader		(calculate_offsets_shader)
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, ctx->resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			sim.rndr_stage = sim.rndr_stage
			.bind_compute_pass(gpu_sort_pipeline, &sim.arena) // 6
			.bind_compute_pass(gpu_offsets_pipeline, &sim.arena); // 7

		}
		// NOTE(DH): GPU Bitonic sorting }

		// NOTE(DH): Graphics pipeline
		{
			WCHAR shader_path[] = L"render_particles.hlsl";
			ID3DBlob* vertex_shader = compile_shader(ctx->g_device, shader_path, "VSMain", "vs_5_0");
			ID3DBlob* pixel_shader 	= compile_shader(ctx->g_device, shader_path, "PSMain", "ps_5_0");

			std::vector<vertex> circ_data 	= generate_circle_vertices(sim.particle_size, 0, 0, 64);
			std::vector<u32> circ_idc 		= generate_circle_indices(64);

			auto mtx_data = sim.arena.get_array(sim.matrices);
			// NOTE(DH): Create resources
			buffer_cbuf cbuff 		= buffer_cbuf	::	create 	(ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, (u8*)&ctx->common_cbuff_data,0);
			buffer_vtex mesh		= buffer_vtex	::	create 	(ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, (u8*)circ_data.data(), circ_data.size() * sizeof(vertex), 64);
			buffer_idex idxs		= buffer_idex	::	create 	(ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, (u8*)circ_idc.data(), circ_idc.size() * sizeof(u32), particle_count, 65);
			buffer_1d 	matrices 	= buffer_1d		::	create	(ctx->g_device, sim.arena, &sim.simulation_desc_heap, &sim.resources_and_views, particle_count, sizeof(mat4), (u8*)mtx_data, 0);

			// NOTE(DH): Create bindings, also need to remember that the order MATTER!
			auto binds = mk_bindings()
				.bind_buffer<false, false, false, false>	(sim.arena.push_data(idxs))
				.bind_buffer<false, false, false, false>	(sim.arena.push_data(mesh))
				.bind_buffer<false, false, false, false>	(sim.arena.push_data(matrices_buffer))
				.bind_buffer<false, true, false, false>		(sim.arena.push_data(cbuff));
			
			auto binds_ar_ptr = sim.arena.push_data(binds);
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
				.create_root_sig	(binds, ctx->g_device, &sim.arena)
				.finalize			(binds, ctx, &sim.arena, sim.resources_and_views, ctx->g_device, &sim.simulation_desc_heap);

			sim.rndr_stage = sim.rndr_stage
				.bind_graphic_pass(graph_pipeline, &sim.arena); // 8
		}
	}

	return sim;
}