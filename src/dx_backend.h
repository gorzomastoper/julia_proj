#pragma once

#define WIN32_LEAN_AND_MEAN //This will shrink the inclusion of windows.h to the essential functions
#include <windows.h>
#include "util/types.h"
#include "util/memory_management.h" //NOTE(DH): Include memory buffer manager
#include "dmath.h"
#define IMGUI_DEFINE_MATH_OPERATORS

#include "simulation_of_particles.h"

//NOTE(DH): This is PIX support

#include <wrl.h> //For WRL::ComPtr
#include <d3d12.h>
#include <d3dx12.h> //Helper functions from https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/Libraries/D3DX12
#include <dxgi1_6.h> //For SwapChain4
#include <DirectXMath.h> // For vector and matrix functions
#include <d3dcompiler.h>
//#include <D3D12Window.h>
#include <shellapi.h>//for CommandLineToArgvW .. getting command line arguments
#include <chrono>

#if 1
#include <initguid.h>
#include <dxgidebug.h>
#endif

static const u8 g_NumFrames = 2;
static u32 g_ClientWidth = 1280;
static u32 g_ClientHeight = 720;

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		printf("Error: %ld", hr);
		throw std::exception();
	}
}

using namespace Microsoft::WRL;

struct vertex
{
	v3 position;
	v4 color;
};

// Introduce constant buffers
struct c_buffer
{
	v4 offset;
	//v4 mouse_pos;
	//mat4 MVP_matrix;
	v4 padding[15]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(c_buffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

#define GET_WIDTH(value) (value >> 16) & 0xFFFFu
#define GET_HEIGHT(value) (value & 0xFFFF)
#define SET_WIDTH_HEIGHT(width, height)	((width << 16) + height)

// NOTE(DH): forward declarations
struct resource_and_view;
struct descriptor_heap;
struct dx_context;

enum buf_format {
	c_buffer_256bytes = 0,
	uav_buffer_r32u,
	uav_buffer_r32i,
	uav_buffer_r32f,
	uav_buffer_r16u,
	uav_buffer_r16i,
	uav_buffer_r8u,
	uav_buffer_r8i,
};

struct buffer_2d {
	u32 res_and_view_idx; 
	ID3D12Resource *stg_buff; 
	u8* data; 
	u32 size_of_data; 
	u32 count_x; 
	u32 count_y; 		
	u32 register_index;	
	u32 heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, 	u32 count_x, u32 count_y, u32 register_index) -> buffer_2d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, 	u32 count_x, u32 count_y) -> buffer_2d;
};

struct buffer_1d {
	u32 				res_and_view_idx;
	ID3D12Resource *	stg_buff; 
	u8* 				data;
	u32 				size_of_data;
	u32 				count;
	u32 				register_index;
	u32 				heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count, u32 register_index) -> buffer_1d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, 	u32 count) -> buffer_1d;
};

struct buffer_vtex {
	u32 res_and_view_idx; 
	ID3D12Resource *stg_buff; 
	D3D12_VERTEX_BUFFER_VIEW view;
	u8* data;
	u32 size_of_data;
	u32 register_index;
	u32 heap_idx;
	bool need_to_upload;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u8* data, u32 size_of_data, u32 register_index) -> buffer_vtex;
};

struct buffer_idex {
	u32 res_and_view_idx;
	ID3D12Resource *stg_buff;
	D3D12_INDEX_BUFFER_VIEW view;
	u8* data;
	u32 size_of_data;
	u32 register_index;
	u32 heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u8* data, u32 size_of_data, u32 register_index) -> buffer_idex;
};

struct buffer_cbuf {
	u32 res_and_view_idx;
	u32 data_idx;
	u8* mapped_view;
	u32 register_index;
	u32 heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 register_index) -> buffer_cbuf;
};

enum tex_format {
	texture_f32 = 0,
	texture_u32,
	texture_i32,
	texture_u32_norm,
	texture_f32_rgba,
	texture_u8rgba_norm,
};

struct texture_2d {
	ID3D12Resource *staging_buff; 
	u32 res_and_view_idx; 
	u32 width_and_height;	
	u8* texture_data; 	
	u32 register_index;	
	u32 heap_idx;
	tex_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width ,u32 height,	u8* texture_data, u32 register_index) -> texture_2d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height,	u8* texture_data) -> texture_2d;
};

struct texture_1d {
	ID3D12Resource *staging_buff;
	u32 res_and_view_idx;
	u32 width;
	u8* texture_data;
	u32 register_index;
	u32 heap_idx;
	tex_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width, u8* texture_data, u32 register_index) -> texture_1d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u8* texture_data) -> texture_1d;
};

enum rt_format {
	rttexture_f32 = 0,
	rttexture_u32,
	rttexture_i32,
	rttexture_u32_norm,
	rttexture_f32_rgba,
	rttexture_u8rgba_norm,
};

struct render_target2d {
	u32 res_and_view_idx;
	u32 width;
	u32 height;
	u32 register_index;
	u32 heap_idx;
	rt_format format;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height, u32 register_idx) -> render_target2d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height) -> render_target2d;
};

struct render_target1d {
	u32 res_and_view_idx;
	u32 width;
	u32 register_index;
	u32 heap_idx;
	rt_format format;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 register_idx) -> render_target1d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width) -> render_target1d;
};

template<bool, typename T, typename TS>
struct t_list_cons {
	arena_ptr<T> el;
	TS tail;
	
	func get_size() { return tail.get_size() + 1; }
};
 
struct t_list_nil {
	func get_size() { return 0; };
};

template<typename BUF_TS>
struct binding {
	BUF_TS data;
	using BUF_TS_U = BUF_TS;

	template<bool B, typename BUF_TNEW>
	func bind_buffer(arena_ptr<BUF_TNEW> buffer) -> binding<t_list_cons<B, BUF_TNEW, BUF_TS>> {
		return binding<t_list_cons<B, BUF_TNEW, BUF_TS>> { .data = { .el = buffer, .tail = data} };
	}
};

static inline func mk_bindings() -> binding<t_list_nil> {
	return binding<t_list_nil>{ .data = {}};
}

struct sampler {
	u32 res_and_view_idx;
	u32 register_idx;
};

struct resource_and_view {
	ID3D12Resource*				addr;
	D3D12_CPU_DESCRIPTOR_HANDLE view;
};

struct descriptor_heap {
	ID3D12DescriptorHeap*	addr;
	u32 					descriptor_size;
	u32						next_resource_idx;
	u32						max_resources_count;
};

struct graphic_pipeline {
	ID3D12RootSignature*			root_signature;
	ID3D12PipelineState*			state;
	ID3DBlob* 						vertex_shader_blob;
	ID3DBlob* 						pixel_shader_blob;
	D3D12_VERTEX_BUFFER_VIEW		vert_buff_view;
	D3D12_INDEX_BUFFER_VIEW			indx_buff_view;
	u32								number_of_resources;
	u32								mesh_instance_num;
	public:
	static inline func init__	() -> graphic_pipeline { 
		graphic_pipeline result = {};
		return result; 
	};

	void(*build_binding_table)(ID3D12GraphicsCommandList *cmd_list, void* bindings);
	
	template<typename T>
	inline func create_root_sig		(binding<T> bindings, ID3D12Device2 *device, memory_arena *arena) 	-> graphic_pipeline;

	inline func bind_vert_shader	(ID3DBlob* shader) 													-> graphic_pipeline;

	inline func bind_frag_shader	(ID3DBlob* shader) 													-> graphic_pipeline;
	
	template<typename T>
	inline func finalize			(binding<T> bindings, dx_context *ctx, ID3D12Device2* device, descriptor_heap *heap) -> graphic_pipeline;
};

struct compute_pipeline {
	ID3D12RootSignature*			root_signature;
	ID3D12PipelineState*			state;
	ID3DBlob* 						shader_blob;
	u32								number_of_resources;
public:
	static inline func init__	() -> compute_pipeline { 
		compute_pipeline result = {};
		return result;
	};

	template<typename T>
	inline func create_root_sig	(binding<T> bindings, ID3D12Device2 *device, memory_arena *arena) 	-> compute_pipeline;

	inline func bind_shader		(ID3DBlob* shader) -> compute_pipeline;

	template<typename T>
	inline func finalize		(binding<T> bindings, dx_context *ctx, ID3D12Device2* device, descriptor_heap *heap) 	-> compute_pipeline;
};

struct render_pass {
	enum pipeline_type {
		compute = 0,
		graphics
	} type;

	arena_ptr<graphic_pipeline> curr_pipeline_g;
	arena_ptr<graphic_pipeline> prev_pipeline_g; // NOTE(DH): We need this for runtime shader compilation!

	arena_ptr<compute_pipeline> curr_pipeline_c;
	arena_ptr<compute_pipeline> prev_pipeline_c; // NOTE(DH): We need this for runtime shader compilation!
};

struct rendering_stage {
	arena_array<render_pass>	render_passes;

public:
	static inline func init__(memory_arena *arena, u32 max_num_of_passes) -> rendering_stage {
		rendering_stage result = {};
		result.render_passes = arena->push_array<render_pass>(max_num_of_passes);
		return result;
	};

	inline func bind_compute_pass(compute_pipeline pipeline, memory_arena *arena) -> rendering_stage {
		auto passes = arena->get_array(this->render_passes);
		passes[this->render_passes.count++].curr_pipeline_c = arena->write(pipeline);
		return *this;
	};

	inline func bind_graphic_pass(graphic_pipeline pipeline, memory_arena *arena) -> rendering_stage {
		auto passes = arena->get_array(this->render_passes);
		passes[this->render_passes.count++].curr_pipeline_g = arena->write(pipeline);
		return *this;
	};
};

struct rendered_entity
{
	ID3D12GraphicsCommandList*	bundle;

	ID3D12Resource* 			vertex_buffer;
	ID3D12Resource* 			index_buffer;
    D3D12_VERTEX_BUFFER_VIEW 	vertex_buffer_view;
    D3D12_INDEX_BUFFER_VIEW 	index_buffer_view;
	ID3D12Resource*				constant_buffer;
	c_buffer					cb_scene_data;
	u8*							cb_data_begin;

	ID3DBlob* 					vertex_shader;
    ID3DBlob* 					pixel_shader;

	ID3D12Resource* 			texture_upload_heap;
	ID3D12Resource* 			texture;
};

struct dx_context
{
	ID3D12Device2* 				g_device;
	ID3D12CommandQueue* 		g_command_queue;
	IDXGISwapChain4* 			g_swap_chain;
	ID3D12Resource* 			g_back_buffers[g_NumFrames];
	ID3D12GraphicsCommandList* 	g_command_list;
	ID3D12GraphicsCommandList* 	g_imgui_command_list;
	ID3D12CommandAllocator* 	g_command_allocators[g_NumFrames];
	ID3D12CommandAllocator* 	g_imgui_command_allocators[g_NumFrames];
	ID3D12CommandAllocator* 	g_bundle_allocators[g_NumFrames];
	ID3D12DescriptorHeap* 		g_rtv_descriptor_heap;
	ID3D12DescriptorHeap* 		g_srv_descriptor_heap;
	ID3D12DescriptorHeap* 		g_imgui_descriptor_heap;
	ID3D12RootSignature* 		g_root_signature;
    ID3D12PipelineState* 		g_pipeline_state;

	ID3D12GraphicsCommandList* 	g_compute_command_list;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissor_rect;
	f32 zmin;
	f32 zmax;
	f32 aspect_ratio;

	//Vendor Specific render target view descriptor size
	UINT g_rtv_descriptor_size;

	//We have a back buffer index because depending of the choosen swap chain Flip Mode Type, 
	//back buffer order might not be sequential
	UINT g_frame_index;

	//Synchronization objects
	ID3D12Fence* g_fence;
	uint64_t 			g_fence_value = 0;
	uint64_t 			g_frame_fence_values[g_NumFrames] = {};
	HANDLE 				g_fence_event;

	f32					time_elapsed; // NOTE(DH): In seconds
	f32					dt_for_frame;

	memory_arena mem_arena;

	arena_array<rendered_entity> 	entities_array;
	arena_array<resource_and_view> 	resources_and_views;

	arena_ptr<void> bindings;
	
	rendering_stage	graphic_stage;
	rendering_stage	compute_stage;

	descriptor_heap graphics_stage_heap;
	descriptor_heap compute_stage_heap;

	ID3D12GraphicsCommandList *graph_command_list;
	ID3D12GraphicsCommandList *compute_command_list;

	// NOTE(DH): Common constant buffer data
	c_buffer common_cbuff_data;

	f32 speed_multiplier = 1.0f;

	//Window Handle
	HWND g_hwnd;
	//Window rectangle used to store window dimensions when in window mode
	RECT g_wind_rect;

	void (*resize)(dx_context *context, u32 width, u32 height, arena_ptr<void> bindings);
	void (*generate_binding_table)(dx_context *context, arena_ptr<void> bindings);

	//NOTE: When you "signal" the Command Queue for a Frame, it means we set a new frame fence value
	//Other window state variables
	bool g_vsync 				= true;
	bool g_tearing_supported 	= false;
	bool g_fullscreen 			= false;
	bool g_use_warp 			= false;
	bool g_is_initialized 		= false;

	//IMGUI STUFF
	bool g_is_menu_active = true;
	
	// Recompile shader
	bool g_recompile_shader;

	bool g_is_quitting = false;
};

func resize							(dx_context *context, u32 width, u32 height) -> void;

func update							(dx_context *context) -> void;

func render							(dx_context *context, ID3D12GraphicsCommandList* command_list) -> void;

func generate_command_buffer		(dx_context *context) -> ID3D12GraphicsCommandList*;

func generate_compute_command_buffer(dx_context *ctx) -> ID3D12GraphicsCommandList*;

func generate_imgui_command_buffer	(dx_context *context) -> ID3D12GraphicsCommandList*;

func move_to_next_frame				(dx_context *context) -> void;

func flush							(dx_context &context) -> void;

func present						(dx_context *context, IDXGISwapChain4* swap_chain) -> void;

func init_dx						(HWND hwnd) -> dx_context;

func get_current_swapchain			(dx_context *context) -> IDXGISwapChain4*;

func get_uav_cbv_srv				(u32 uav_idx, u32 uav_count, ID3D12Device2* device, ID3D12DescriptorHeap* desc_heap) -> CD3DX12_GPU_DESCRIPTOR_HANDLE;

func create_compute_pipeline		(ID3D12Device2* device, WCHAR* shader_path, LPCSTR entry_name, LPCSTR version_name, memory_arena *arena, ID3D12RootSignature *root_sig) -> compute_pipeline;

func create_uav_descriptor_view		(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func create_srv_descriptor_view		(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, D3D12_SRV_DIMENSION dim, ID3D12Resource *p_resource) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func create_uav_u32_buffer_descriptor_view(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, u32 num_of_elements, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func allocate_data_on_gpu 			(ID3D12Device2* device, D3D12_HEAP_TYPE hep_ty, D3D12_HEAP_FLAGS hep_fgs, D3D12_TEXTURE_LAYOUT lyot, D3D12_RESOURCE_FLAGS r_fgs, D3D12_RESOURCE_STATES sts, D3D12_RESOURCE_DIMENSION dim, u64 w, u64 h, DXGI_FORMAT fmt) -> ID3D12Resource*;

func get_gpu_handle_at				(u32 index, ID3D12DescriptorHeap* desc_heap, u32 desc_size) -> CD3DX12_GPU_DESCRIPTOR_HANDLE;

func get_cpu_handle_at				(u32 index, ID3D12DescriptorHeap* desc_heap, u32 desc_size) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func record_compute_stage			(ID3D12Device2* device, rendering_stage stage, ID3D12GraphicsCommandList* command_list) -> void;

func compile_shader					(ID3D12Device2* device, WCHAR* shader_path, LPCSTR entry_name, LPCSTR version_name) -> ID3DBlob*;

func recompile_shader				(dx_context *ctx, rendering_stage rndr_stage) -> void;

func clear_render_target			(dx_context ctx, ID3D12GraphicsCommandList *command_list, float clear_color[4]) -> void;

func allocate_resources_and_views	(memory_arena *arena, u32 max_resources_count) -> arena_array<resource_and_view>;

func allocate_descriptor_heaps		(memory_arena *arena, u32 count) -> arena_array<descriptor_heap>;

func allocate_rendering_stages		(memory_arena *arena, u32 max_count) -> arena_array<rendering_stage>;

func allocate_descriptor_heap		(ID3D12Device2* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, u32 count) -> descriptor_heap;

func create_compute_rendering_stage	(dx_context *ctx, ID3D12Device2* device, D3D12_VIEWPORT viewport, memory_arena *arena, u32 num_of_piepelines) -> rendering_stage;

func record_copy_buffer 			(ID3D12Resource *p_dst, ID3D12Resource *p_src, ID3D12GraphicsCommandList *command_list, u32 size) -> void;