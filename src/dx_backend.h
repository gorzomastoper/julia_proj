#pragma once

#define WIN32_LEAN_AND_MEAN //This will shrink the inclusion of windows.h to the essential functions
#include <windows.h>
#include "util/types.h"
#include "util/memory_management.h" //NOTE(DH): Include memory buffer manager
#include "dmath.h"
#define IMGUI_DEFINE_MATH_OPERATORS

//NOTE(DH): This is PIX support

#include <wrl.h> //For WRL::ComPtr
#include <d3d12.h>
#include <d3dx12.h> //Helper functions from https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/Libraries/D3DX12
#include <dxgi1_6.h> //For SwapChain4
#include <DirectXMath.h> // For vector and matrix functions
#include <d3dcompiler.h>
//#include <D3D12Window.h>
#include <shellapi.h>//for CommandLineToArgvW .. getting command line arguments

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
	v4 mouse_pos;
	mat4 MVP_matrix;
	v4 padding[10]; // Padding so the constant buffer is 256-byte aligned.
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
	u32 size_of_one_elem; 		
	u32 register_index;	
	u32 heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count_x, u32 count_y, u32 size_of_one_elem, u8* data, u32 register_index) -> buffer_2d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, 	u32 count_x, u32 count_y, u32 size_of_one_elem, u8* data) -> buffer_2d;
};

struct buffer_1d {
	u32 				res_and_view_idx;
	ID3D12Resource *	stg_buff; 
	u8* 				data;
	u32 				size_of_data;
	u32 				count;
	u32 				size_of_one_elem;
	u32 				register_index;
	u32 				heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count, u32 size_of_one_elem, u8* data, u32 register_index) -> buffer_1d;
	inline func recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, 	u32 count, u32 size_of_one_elem, u8* data) -> buffer_1d;
};

struct buffer_vtex {
	u32 res_and_view_idx; 
	ID3D12Resource *stg_buff; 
	D3D12_VERTEX_BUFFER_VIEW view;
	u8* data;
	u32 size_of_data;
	u32 size_of_one_elem;
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
	u32 size_of_one_elem;
	u32 instance_count;
	u32 register_index;
	u32 heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u8* data, u32 size_of_data, u32 instance_count, u32 register_index) -> buffer_idex;
};

struct buffer_cbuf {
	u32 res_and_view_idx;
	u32 data_idx;
	u8* mapped_view;
	u8* data;
	u32 register_index;
	u32 heap_idx;
	buf_format fmt;

	static inline func create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u8* data, u32 register_index) -> buffer_cbuf;
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

// NOTE(DH): RESIZE - UPDATE - COPY TO RTV
template<bool, bool, bool, typename T, typename TS>
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

	template<bool B, bool U, bool CP, typename BUF_TNEW>
	func bind_buffer(arena_ptr<BUF_TNEW> buffer) -> binding<t_list_cons<B, U, CP, BUF_TNEW, BUF_TS>> {
		return binding<t_list_cons<B, U, CP, BUF_TNEW, BUF_TS>> { .data = { .el = buffer, .tail = data} };
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
	u32								number_of_resources;
	u32								mesh_instance_num;
	arena_ptr<void> 				bindings;	

	public:
	static inline func init__(arena_ptr<void> bindings, 
			void (*RSZ)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings), 
			void (*GBT)(dx_context *context, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings), 
			void (*UPD)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings)) 
				-> graphic_pipeline {
		graphic_pipeline result 		= {};
		result.bindings 				= bindings;
		result.resize 					= RSZ;
		result.generate_binding_table 	= GBT;
		result.update					= UPD;
		return result; 
	};

	void (*resize)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings);

	void (*update)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings);

	void (*generate_binding_table)(dx_context *context, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings);
	
	template<typename T>
	inline func create_root_sig		(binding<T> bindings, ID3D12Device2 *device, memory_arena *arena) 	-> graphic_pipeline;

	inline func bind_vert_shader	(ID3DBlob* shader) 													-> graphic_pipeline;

	inline func bind_frag_shader	(ID3DBlob* shader) 													-> graphic_pipeline;
	
	template<typename T>
	inline func finalize			(binding<T> bindings, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12Device2* device, descriptor_heap *heap) -> graphic_pipeline;
};

struct compute_pipeline {
	ID3D12RootSignature*			root_signature;
	ID3D12PipelineState*			state;
	ID3DBlob* 						shader_blob;
	u32								number_of_resources;
	arena_ptr<void> 				bindings;

public:
	static inline func init__	(arena_ptr<void> bindings, 
			void (*RSZ)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings), 
			void (*GBT)(dx_context *context, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings), 
			void (*UPD)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings),
			void (*CTS)(dx_context *context, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings)) 
				-> compute_pipeline { 
		compute_pipeline result 		= {};
		result.bindings 				= bindings;
		result.resize 					= RSZ;
		result.generate_binding_table 	= GBT;
		result.update					= UPD;
		result.copy_to_screen_rt		= CTS;
		return result;
	};

	void (*resize)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings);

	void (*update)(dx_context *context, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings);

	void (*generate_binding_table)(dx_context *context, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings);

	void (*copy_to_screen_rt)(dx_context *context, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings);

	template<typename T>
	inline func create_root_sig	(binding<T> bindings, ID3D12Device2 *device, memory_arena *arena) 	-> compute_pipeline;

	inline func bind_shader		(ID3DBlob* shader) -> compute_pipeline;

	template<typename T>
	inline func finalize		(binding<T> bindings, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12Device2* device, descriptor_heap *heap) 	-> compute_pipeline;
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
		result.render_passes = arena->alloc_array<render_pass>(max_num_of_passes);
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

struct dx_context
{
	u32 update_counter;
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

	arena_array<resource_and_view> 	resources_and_views;
	
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

template<typename T>
func create_command_list			(dx_context *ctx, D3D12_COMMAND_LIST_TYPE cmdListType, ID3D12PipelineState *pInitialState, bool close) -> T*;

func resize							(dx_context *context, u32 width, u32 height) -> void;

func update							(dx_context *context) -> void;

func render							(dx_context *context, ID3D12GraphicsCommandList* command_list) -> void;

func generate_command_buffer		(dx_context *context) -> ID3D12GraphicsCommandList*;

func generate_compute_command_buffer(dx_context *ctx, u32 width, u32 height) -> ID3D12GraphicsCommandList*;

func generate_imgui_command_buffer	(dx_context *context) -> ID3D12GraphicsCommandList*;

func start_imgui_frame				() -> void;

func finish_imgui_frame				() -> void;

func imgui_draw_canvas				(dx_context *ctx) -> void;

func move_to_next_frame				(dx_context *context) -> void;

func flush							(dx_context &context) -> void;

func present						(dx_context *context, IDXGISwapChain4* swap_chain) -> void;

func init_dx						(HWND hwnd) -> dx_context;

func get_current_swapchain			(dx_context *context) -> IDXGISwapChain4*;

func get_uav_cbv_srv				(u32 uav_idx, u32 uav_count, ID3D12Device2* device, ID3D12DescriptorHeap* desc_heap) -> CD3DX12_GPU_DESCRIPTOR_HANDLE;

func get_uav_cbv_srv_gpu_handle		(u32 uav_idx, u32 uav_count, ID3D12Device2* device, ID3D12DescriptorHeap* desc_heap) -> CD3DX12_GPU_DESCRIPTOR_HANDLE;

func create_compute_pipeline		(ID3D12Device2* device, WCHAR* shader_path, LPCSTR entry_name, LPCSTR version_name, memory_arena *arena, ID3D12RootSignature *root_sig) -> compute_pipeline;

func create_uav_descriptor_view		(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func create_srv_descriptor_view		(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, D3D12_SRV_DIMENSION dim, ID3D12Resource *p_resource) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func create_uav_u32_buffer_descriptor_view(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, u32 num_of_elements, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func create_uav_buffer_descriptor_view(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, u32 num_of_elements, u32 size_of_one_elem, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

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

func get_command_allocator			(u32 frame_idx, ID3D12CommandAllocator** cmd_alloc_array) -> ID3D12CommandAllocator*;

func record_reset_cmd_allocator		(ComPtr<ID3D12CommandAllocator> cmd_alloc) -> void;

func record_reset_cmd_list			(ID3D12GraphicsCommandList* cmd_list, ID3D12CommandAllocator *cmd_alloc, ID3D12PipelineState *pipeline_state) -> void;

func record_viewports				(u32 num_of_viewports, D3D12_VIEWPORT viewport, ComPtr<ID3D12GraphicsCommandList> cmd_list) -> void;

func record_scissors				(u32 num_of_scissors, D3D12_RECT scissor_rect, ComPtr<ID3D12GraphicsCommandList> cmd_list) -> void;

func barrier_transition				(ID3D12Resource * p_resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after) -> CD3DX12_RESOURCE_BARRIER;

func record_resource_barrier		(u32 num_of_barriers, CD3DX12_RESOURCE_BARRIER transition, ComPtr<ID3D12GraphicsCommandList> cmd_list) -> void;

func set_render_target				(ID3D12GraphicsCommandList* cmd_list,	ID3D12DescriptorHeap* dsc_heap,	u32 rtv_num,bool single_handle_to_rtv_range, u32 frame_idx, u32 dsc_size) -> void;

func get_rtv_descriptor_handle		(ComPtr<ID3D12DescriptorHeap> dsc_heap, u32 frame_idx, u32 dsc_size) -> CD3DX12_CPU_DESCRIPTOR_HANDLE;

func record_dsc_heap				(ComPtr<ID3D12GraphicsCommandList> cmd_list, ID3D12DescriptorHeap* ppHeaps[], u32 descriptors_count) -> void;

func create_feature_data_root_signature(ComPtr<ID3D12Device2> device) -> D3D12_FEATURE_DATA_ROOT_SIGNATURE;

func serialize_versioned_root_signature(CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC &desc, D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data_root_sign) -> ID3DBlob*;

func create_root_signature_desc		(CD3DX12_ROOT_PARAMETER1 *root_parameters, u32 root_parameters_count, D3D12_STATIC_SAMPLER_DESC sampler, D3D12_ROOT_SIGNATURE_FLAGS flags) -> CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC;

func create_root_signature			(ID3D12Device2* device, ID3DBlob* signature) -> ID3D12RootSignature*;

func create_command_allocator		(ID3D12Device2* currentDevice, D3D12_COMMAND_LIST_TYPE cmdListType) -> ID3D12CommandAllocator*;

func pixel_default_sampler_desc		() -> D3D12_STATIC_SAMPLER_DESC;

func wait_for_gpu					(ID3D12CommandQueue* command_queue, ID3D12Fence* fence, HANDLE fence_event,u64 *frame_fence_value) -> void;

template<typename T>
struct CRS {
	static func create_root_sig(T list, u32 current_resource, bool *is_texture_arr, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void { static_assert(!1, "");};
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, buffer_cbuf, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, buffer_cbuf, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, arena->get_ptr(list.el)->register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		is_texture[current_resource] = false;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, buffer_2d, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, buffer_2d, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, arena->get_ptr(list.el)->register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		is_texture[current_resource] = false;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, buffer_1d, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, buffer_1d, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, arena->get_ptr(list.el)->register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		is_texture[current_resource] = false;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, buffer_vtex, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, buffer_vtex, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, arena->get_ptr(list.el)->register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		is_texture[current_resource] = false;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, buffer_idex, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, buffer_idex, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, arena->get_ptr(list.el)->register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		is_texture[current_resource] = false;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, texture_2d, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, texture_2d, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, arena->get_ptr(list.el)->register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		is_texture[current_resource] = true;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, texture_1d, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, texture_1d, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, arena->get_ptr(list.el)->register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		is_texture[current_resource] = true;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, render_target2d, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, render_target2d, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, arena->get_ptr(list.el)->register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		is_texture[current_resource] = false;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct CRS<t_list_cons<B, U, CP, render_target1d, TS>> {
	static func create_root_sig(t_list_cons<B, U, CP, render_target1d, TS> list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		ranges[current_resource].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, arena->get_ptr(list.el)->register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		is_texture[current_resource] = false;
		CRS<TS>::create_root_sig(list.tail, current_resource + 1, is_texture, ranges, arena);
	}
};

template<>
struct CRS<t_list_nil> {
	static func create_root_sig(t_list_nil list, u32 current_resource, bool *is_texture, CD3DX12_DESCRIPTOR_RANGE1 *ranges, memory_arena *arena) -> void {
		return;
	}
};

template<typename T>
struct GCB {
	static func bind_root_sig_table(T list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void { static_assert(!1, "");};
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, buffer_cbuf, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_cbuf, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, buffer_2d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_2d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, buffer_1d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_1d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, buffer_vtex, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_vtex, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->IASetVertexBuffers(0, 1, &arena->get_ptr(list.el)->view);
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, buffer_idex, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_idex, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->IASetIndexBuffer(&arena->get_ptr(list.el)->view);
		cmd_list->DrawIndexedInstanced(arena->get_ptr(list.el)->size_of_data / sizeof(u32), arena->get_ptr(list.el)->instance_count, 0, 0, 0);
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, texture_2d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, texture_2d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, texture_1d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, texture_1d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, render_target2d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, render_target2d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCB<t_list_cons<B, U, CP, render_target1d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, render_target1d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<>
struct GCB<t_list_nil> {
	static func bind_root_sig_table(t_list_nil list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		return;
	}
};

template<typename T>
struct GCBC {
	static func bind_root_sig_table(T list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void { static_assert(!1, "");};
};

template<bool B, bool U, bool CP, typename TS>
struct GCBC<t_list_cons<B, U, CP, buffer_cbuf, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_cbuf, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetComputeRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCBC<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCBC<t_list_cons<B, U, CP, buffer_2d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_2d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetComputeRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCBC<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCBC<t_list_cons<B, U, CP, buffer_1d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, buffer_1d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetComputeRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCBC<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCBC<t_list_cons<B, U, CP, texture_2d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, texture_2d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetComputeRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCBC<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCBC<t_list_cons<B, U, CP, texture_1d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, texture_1d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetComputeRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCBC<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCBC<t_list_cons<B, U, CP, render_target2d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, render_target2d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetComputeRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCB<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct GCBC<t_list_cons<B, U, CP, render_target1d, TS>> {
	static func bind_root_sig_table(t_list_cons<B, U, CP, render_target1d, TS> list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		cmd_list->SetComputeRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(arena->get_ptr(list.el)->heap_idx, 1, device, heap));
		GCBC<TS>::bind_root_sig_table(list.tail, current_resource + 1, cmd_list, device, heap, arena);
	}
};

template<>
struct GCBC<t_list_nil> {
	static func bind_root_sig_table(t_list_nil list, u32 current_resource, ID3D12GraphicsCommandList *cmd_list, ID3D12Device2* device, ID3D12DescriptorHeap *heap, memory_arena *arena) -> void {
		return;
	}
};

template<typename T>
struct Update {
	static func update(T list, dx_context*ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void { static_assert(!1, "");};
};

template<bool B, bool CP, typename TS>
struct Update<t_list_cons<B, true, CP, buffer_cbuf, TS>> {
	static func update(t_list_cons<B, true, CP, buffer_cbuf, TS> list, dx_context* ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void {
		buffer_cbuf buffer = *arena->get_ptr(list.el);
		memcpy(buffer.mapped_view, buffer.data, 256);
		Update<TS>::update(list.tail, ctx, arena, resources_and_views, cmd_list);
	}
};

template<bool B, bool CP, typename TS>
struct Update<t_list_cons<B, true, CP, buffer_1d, TS>> {
	static func update(t_list_cons<B, true, CP, buffer_1d, TS> list, dx_context* ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void {
		buffer_1d buffer = *arena->get_ptr(list.el);
		
		u8* p_idx_data_begin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(buffer.stg_buff->Map(0, &readRange, reinterpret_cast<void**>(&p_idx_data_begin)));
		memcpy(p_idx_data_begin, buffer.data, buffer.size_of_data);
		buffer.stg_buff->Unmap(0, nullptr);

		auto r_n_v = &arena->get_array(resources_and_views)[buffer.res_and_view_idx];
		auto addr4 = CD3DX12_RESOURCE_BARRIER::Transition(r_n_v->addr, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ/* | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
		cmd_list->CopyBufferRegion(r_n_v->addr, 0, buffer.stg_buff, 0, buffer.size_of_data);
		cmd_list->ResourceBarrier(1, &addr4);
		Update<TS>::update(list.tail, ctx, arena, resources_and_views, cmd_list);
	}
};

template<bool B, bool U, bool CP, typename T, typename TS>
struct Update<t_list_cons<B, U, CP, T, TS>> {
	static func update(t_list_cons<B, U, CP, T, TS> list, dx_context*ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void {
		Update<TS>::update(list.tail, ctx, arena, resources_and_views, cmd_list);
	}
};

template<>
struct Update<t_list_nil> {
	static func update(t_list_nil list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) {
		return;
	}
};

template<typename T>
struct Resize {
	static func resize(T list, ID3D12Device2* dev, memory_arena ar, descriptor_heap *heap, arena_array<resource_and_view> *rnv, u32 w, u32 h) -> void { static_assert(!1, "");};
};

template<bool U, bool CP, typename TS>
struct Resize<t_list_cons<true, U, CP, buffer_2d, TS>> {
	static func resize(t_list_cons<true, U, CP, buffer_2d, TS> list, ID3D12Device2* dev, memory_arena ar, descriptor_heap *heap, arena_array<resource_and_view> *rnv, u32 w, u32 h) -> void{
		ar.get_ptr(list.el)->recreate(dev, ar, heap, rnv, w, h, ar.get_ptr(list.el)->size_of_one_elem, ar.get_ptr(list.el)->data);
		Resize<TS>::resize(list.tail, dev, ar, heap, rnv, w, h);
	}
};

template<bool U, bool CP, typename TS>
struct Resize<t_list_cons<true, U, CP, buffer_1d, TS>> {
	static func resize(t_list_cons<true, U, CP, buffer_1d, TS> list, ID3D12Device2* dev, memory_arena ar, descriptor_heap *heap, arena_array<resource_and_view> *rnv, u32 w, u32 h)-> void {
		ar.get_ptr(list.el)->recreate(dev, ar, heap, rnv, w * h, ar.get_ptr(list.el)->size_of_one_elem, ar.get_ptr(list.el)->data);
		Resize<TS>::resize(list.tail, dev, ar, heap, rnv, w, h);
	}
};

template<bool U, bool CP, typename TS>
struct Resize<t_list_cons<true, U, CP, render_target2d, TS>> {
	static func resize(t_list_cons<true, U, CP, render_target2d, TS> list, ID3D12Device2* dev, memory_arena ar, descriptor_heap *heap, arena_array<resource_and_view> *rnv, u32 w, u32 h)-> void {
		ar.get_ptr(list.el)->recreate(dev, ar, heap, rnv, w, h);
		Resize<TS>::resize(list.tail, dev, ar, heap, rnv, w, h);
	}
};

template<bool U, bool CP, typename TS>
struct Resize<t_list_cons<true, U, CP, render_target1d, TS>> {
	static func resize(t_list_cons<true, U, CP, render_target1d, TS> list, ID3D12Device2* dev, memory_arena ar, descriptor_heap *heap, arena_array<resource_and_view> *rnv, u32 w, u32 h)-> void {
		ar.get_ptr(list.el)->recreate(dev, ar, heap, rnv, w * h);
		Resize<TS>::resize(list.tail, dev, ar, heap, rnv, w, h);
	}
};

template<bool U, bool CP, typename T, typename TS>
struct Resize<t_list_cons<false, U, CP, T, TS>> {
	static func resize(t_list_cons<false, U, CP, T, TS> list, ID3D12Device2* dev, memory_arena ar, descriptor_heap *heap, arena_array<resource_and_view> *rnv, u32 w, u32 h)-> void {
		Resize<TS>::resize(list.tail, dev, ar, heap, rnv, w, h);
	}
};

template<>
struct Resize<t_list_nil> {
	static func resize(t_list_nil list, ID3D12Device2* dev, memory_arena ar, descriptor_heap *heap, arena_array<resource_and_view> *rnv, u32 w, u32 h) -> void {
		return;
	}
};

template<typename T>
struct Finalize {
	static func finalize (T list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list, descriptor_heap* heap) -> void { static_assert(!1, "");};
};

template<bool B, bool U, bool CP, typename TS>
struct Finalize<t_list_cons<B, U, CP, texture_2d, TS>> {
	static func finalize(t_list_cons<B, U, CP, texture_2d, TS> list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list, descriptor_heap* heap) -> void {

		texture_2d *texture_dsc = arena->get_ptr(list.el);
		resource_and_view *r_n_v = &arena->get_array(resources_and_views)[texture_dsc->res_and_view_idx];
		u32 width = GET_WIDTH(texture_dsc->width_and_height);
		u32 height = GET_HEIGHT(texture_dsc->width_and_height);
	
		// Create the texture
		{
			const UINT64 upload_buffer_size = GetRequiredIntermediateSize(r_n_v->addr, 0, 1);

			// NOTE(DH): Here we create heap properties with texture data to be copied to from staiging buffer
			auto addr2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto addr3 = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

			// Create the GPU upload buffer.
			ThrowIfFailed(ctx->g_device->CreateCommittedResource(
				&addr2,
				D3D12_HEAP_FLAG_NONE,
				&addr3,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&texture_dsc->staging_buff)));

			D3D12_SUBRESOURCE_DATA texture_subresource_data = {};
			texture_subresource_data.pData = &texture_dsc->texture_data[0];
			texture_subresource_data.RowPitch = width * 4; //4 bytes per pixel
			texture_subresource_data.SlicePitch = texture_subresource_data.RowPitch * height; //256x256 Width x Height

			// NOTE(DH): This is important, because we use this resource not only in PIXEL SHADER BUT PROBABLY IN VERTEX SHADER, WE NEED EXPLICITLY MARK THIS FLAGS AS IT IS!!!
			auto addr4 = CD3DX12_RESOURCE_BARRIER::Transition(r_n_v->addr, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE/* | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
			UpdateSubresources(cmd_list, r_n_v->addr, texture_dsc->staging_buff, 0, 0, 1, &texture_subresource_data);
			cmd_list->ResourceBarrier(1, &addr4);

			// Describe and create a SRV for the texture.
			u32 inc_size = ctx->g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			r_n_v->view = create_srv_descriptor_view(ctx->g_device, texture_dsc->heap_idx, heap->addr, inc_size, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D, r_n_v->addr);
		}

		Finalize<TS>::finalize(list.tail, ctx, arena, resources_and_views, cmd_list, heap);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct Finalize<t_list_cons<B, U, CP, buffer_vtex, TS>> {
	static func finalize(t_list_cons<B, U, CP, buffer_vtex, TS> list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list, descriptor_heap* heap) -> void {

		buffer_vtex *buffer_desc = arena->get_ptr(list.el);
		resource_and_view *r_n_v = &arena->get_array(resources_and_views)[buffer_desc->res_and_view_idx];

		auto addr4 = CD3DX12_RESOURCE_BARRIER::Transition(r_n_v->addr, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER/* | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
		cmd_list->CopyBufferRegion(r_n_v->addr, 0, buffer_desc->stg_buff, 0, buffer_desc->size_of_data);
		cmd_list->ResourceBarrier(1, &addr4);

		// Initialize the vertex buffer view.
		buffer_desc->view.BufferLocation	= r_n_v->addr->GetGPUVirtualAddress();
		buffer_desc->view.StrideInBytes		= sizeof(vertex);
		buffer_desc->view.SizeInBytes		= buffer_desc->size_of_data;

		Finalize<TS>::finalize(list.tail, ctx, arena, resources_and_views, cmd_list, heap);
	}
};

template<bool B, bool U, bool CP, typename TS>
struct Finalize<t_list_cons<B, U, CP, buffer_idex, TS>> {
	static func finalize(t_list_cons<B, U, CP, buffer_idex, TS> list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list, descriptor_heap* heap) -> void {

		buffer_idex *buffer_desc = arena->get_ptr(list.el);
		resource_and_view *r_n_v = &arena->get_array(resources_and_views)[buffer_desc->res_and_view_idx];

		auto addr4 = CD3DX12_RESOURCE_BARRIER::Transition(r_n_v->addr, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER/* | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
		cmd_list->CopyBufferRegion(r_n_v->addr, 0, buffer_desc->stg_buff, 0, buffer_desc->size_of_data);
		cmd_list->ResourceBarrier(1, &addr4);

		// Initialize the vertex buffer view.
		buffer_desc->view.BufferLocation	= r_n_v->addr->GetGPUVirtualAddress();
		buffer_desc->view.Format 			= DXGI_FORMAT_R32_UINT;
		buffer_desc->view.SizeInBytes 		= buffer_desc->size_of_data;

		Finalize<TS>::finalize(list.tail, ctx, arena, resources_and_views, cmd_list, heap);
	}
};

template<bool B, bool U, bool CP, typename T, typename TS>
struct Finalize<t_list_cons<B, U, CP, T, TS>> {
	static func finalize(t_list_cons<B, U, CP, T, TS> list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views,ID3D12GraphicsCommandList *cmd_list, descriptor_heap* heap) -> void {
		Finalize<TS>::finalize(list.tail, ctx, arena, resources_and_views, cmd_list, heap);
	}
};

template<>
struct Finalize<t_list_nil> {
	static func finalize(t_list_nil list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list, descriptor_heap* heap) -> void {
		return;
	}
};

template<typename T>
struct CTRT {
	static func copy_to_render_target (T list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void { static_assert(!1, "");};
};

template<bool B, bool U, typename TS>
struct CTRT<t_list_cons<B, U, true, render_target2d, TS>> {
	static func copy_to_render_target(t_list_cons<B, U, true, render_target2d, TS> list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void {
		render_target2d *rt2_desc = arena->get_ptr(list.el);
		resource_and_view *r_n_v = &arena->get_array(resources_and_views)[rt2_desc->res_and_view_idx];

		ID3D12Resource* screen_buffer = ctx->g_back_buffers[ctx->g_frame_index];
		record_resource_barrier(1,  barrier_transition(screen_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST), cmd_list);
		cmd_list->CopyResource(screen_buffer, r_n_v->addr);
		record_resource_barrier(1,  barrier_transition(screen_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT), cmd_list);

		CTRT<TS>::copy_to_render_target(list.tail, ctx, arena, resources_and_views, cmd_list);
	}
};

template<bool B, bool U, bool CP, typename T, typename TS>
struct CTRT<t_list_cons<B, U, CP, T, TS>> {
	static func copy_to_render_target(t_list_cons<B, U, CP, T, TS> list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void {
		CTRT<TS>::copy_to_render_target(list.tail, ctx, arena, resources_and_views, cmd_list);
	}
};

template<>
struct CTRT<t_list_nil> {
	static func copy_to_render_target(t_list_nil list, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList *cmd_list) -> void {
		return;
	}
};

inline func graphic_pipeline::bind_vert_shader(ID3DBlob* shader) -> graphic_pipeline {
	this->vertex_shader_blob = shader;
	return *this;
}

inline func graphic_pipeline::bind_frag_shader(ID3DBlob* shader) -> graphic_pipeline {
	this->pixel_shader_blob = shader;
	return *this;
}

template<typename T>
inline func graphic_pipeline::create_root_sig(binding<T> bindings, ID3D12Device2* device, memory_arena *arena) -> graphic_pipeline {

	this->number_of_resources = bindings.data.get_size();
	
	bool is_texture[bindings.data.get_size()];
	CD3DX12_DESCRIPTOR_RANGE1 ranges[bindings.data.get_size()];
	
	CRS<T>::create_root_sig(bindings.data, 0, is_texture, ranges, arena);
		
	CD3DX12_ROOT_PARAMETER1 root_parameters[bindings.data.get_size()];
	for(u32 i = 0; i < this->number_of_resources; ++i) {
		root_parameters[i].InitAsDescriptorTable(1, &ranges[i], is_texture[i] ? D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_VERTEX);
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = 
	create_root_signature_desc(root_parameters, this->number_of_resources, pixel_default_sampler_desc(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ID3DBlob* versioned_signature 	= serialize_versioned_root_signature(root_signature_desc, create_feature_data_root_signature(device));
    this->root_signature 			= create_root_signature(device, versioned_signature);
	return *this;
}

template<typename T>
inline func graphic_pipeline::finalize(binding<T> bindings, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12Device2* device, descriptor_heap *heap) -> graphic_pipeline {

#if DEBUG
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	// Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC input_element_descs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Describe and create the graphics pipeline state object (PSO).
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend =
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend =
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp =
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { input_element_descs, _countof(input_element_descs) };
    psoDesc.pRootSignature = this->root_signature;
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(this->vertex_shader_blob);
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(this->pixel_shader_blob);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = blendDesc;//CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&this->state)));

	auto cmd_list = create_command_list<ID3D12GraphicsCommandList>(ctx, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx->g_pipeline_state, false);
	cmd_list->SetName(L"Uload to GPU list");

	Finalize<T>::finalize(bindings.data, ctx, arena, resources_and_views, cmd_list, heap);

	// Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(cmd_list->Close());
    ID3D12CommandList* ppCommandLists[] = { cmd_list };
    ctx->g_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(ctx->g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ctx->g_fence)));
        ctx->g_frame_fence_values[ctx->g_frame_index]++;

        // Create an event handle to use for frame synchronization.
        ctx->g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (ctx->g_fence_event == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
		wait_for_gpu(ctx->g_command_queue, ctx->g_fence, ctx->g_fence_event, &ctx->g_frame_fence_values[ctx->g_frame_index]);
    }

	return *this;
}

template<typename T>
inline func compute_pipeline::create_root_sig (binding<T> bindings, ID3D12Device2 *device, memory_arena *arena) -> compute_pipeline {

	this->number_of_resources = bindings.data.get_size();
	
	CD3DX12_DESCRIPTOR_RANGE1 ranges[this->number_of_resources];

	bool is_texture[bindings.data.get_size()];
	
	CRS<T>::create_root_sig(bindings.data, 0, is_texture, ranges, arena);
		
	CD3DX12_ROOT_PARAMETER1 root_parameters[this->number_of_resources];
	for(u32 i = 0; i < this->number_of_resources; ++i) {
		root_parameters[i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_ALL);
	}
	// root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
	// root_parameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = 
	create_root_signature_desc(root_parameters, this->number_of_resources, pixel_default_sampler_desc(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ID3DBlob* versioned_signature 	= serialize_versioned_root_signature(root_signature_desc, create_feature_data_root_signature(device));
    this->root_signature 			= create_root_signature(device, versioned_signature);
	this->root_signature->SetName(L"COMPUTE ROOT SIGN");
	return *this;
}

inline func compute_pipeline::bind_shader (ID3DBlob* shader) -> compute_pipeline {
	this->shader_blob = shader;
	return *this;
}

template<typename T>
inline func compute_pipeline::finalize (binding<T> bindings, dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resource_and_view, ID3D12Device2* device, descriptor_heap *heap) -> compute_pipeline {
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend =
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend =
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp =
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = this->root_signature;
    psoDesc.CS = CD3DX12_SHADER_BYTECODE(this->shader_blob);
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	psoDesc.NodeMask = 0;

    ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&this->state)));

	auto cmd_list = create_command_list<ID3D12GraphicsCommandList>(ctx, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx->g_pipeline_state, false);
	cmd_list->SetName(L"Uload to GPU list");

	Finalize<T>::finalize(bindings.data, ctx, arena, resource_and_view, cmd_list, heap);

	// Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(cmd_list->Close());
    ID3D12CommandList* ppCommandLists[] = { cmd_list };
    ctx->g_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(ctx->g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ctx->g_fence)));
        ctx->g_frame_fence_values[ctx->g_frame_index]++;

        // Create an event handle to use for frame synchronization.
        ctx->g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (ctx->g_fence_event == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
		wait_for_gpu(ctx->g_command_queue, ctx->g_fence, ctx->g_fence_event, &ctx->g_frame_fence_values[ctx->g_frame_index]);
    }

	return *this;
}

inline func buffer_cbuf::create (ID3D12Device2 *device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u8* data, u32 register_index) -> buffer_cbuf {
	buffer_cbuf result = {};

	u32 size_of_cbuffer = sizeof(c_buffer); // NOTE(DH): IMPORTANT, this is always will be 256 bytes!
	result.register_index = register_index;
	result.res_and_view_idx = r_n_v->count++;
	result.heap_idx = heap->next_resource_idx++;
	result.data = data;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, result.res_and_view_idx);
	
	DXGI_FORMAT 				format 		= DXGI_FORMAT_UNKNOWN;
	D3D12_HEAP_TYPE 			heap_type 	= D3D12_HEAP_TYPE_UPLOAD;
	D3D12_HEAP_FLAGS 			heap_flags 	= D3D12_HEAP_FLAG_NONE;
	D3D12_TEXTURE_LAYOUT 		layout 		= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS 		flags 		= D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES 		states 		= D3D12_RESOURCE_STATE_GENERIC_READ;
	D3D12_RESOURCE_DIMENSION 	dim 		= D3D12_RESOURCE_DIMENSION_BUFFER;

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, layout, flags, states, dim, size_of_cbuffer, 1, format);
	res_n_view->view = get_cpu_handle_at(result.heap_idx, heap->addr, heap->descriptor_size);
	res_n_view->addr->SetName(L"C_BUFFER");
	// NOTE(DH): IMPORTANT, Always needs to be 256 bytes!
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {.BufferLocation = res_n_view->addr->GetGPUVirtualAddress(), .SizeInBytes = size_of_cbuffer};
	device->CreateConstantBufferView(&cbv_desc, res_n_view->view);
	// Map and initialize the constant buffer. We don't unmap this until the
	// app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(res_n_view->addr->Map(0, &read_range, (void**)(&result.mapped_view)));
	
	return result;
}

inline func buffer_2d::create (ID3D12Device2 *device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 count_x, u32 count_y, u32 size_of_one_elem, u8* data, u32 register_index) -> buffer_2d {
	buffer_2d result = {};

	result.res_and_view_idx = r_n_v->count;
	result.register_index = register_index;
	result.count_x = count_x;
	result.count_y = count_y;
	result.heap_idx = heap->next_resource_idx++;
	result.data = data;
	result.size_of_data = count_x * count_y * size_of_one_elem;
	result.size_of_one_elem = size_of_one_elem;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);

	if(data) {
		auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size_of_one_elem * count_x * count_y);
		ThrowIfFailed(device->CreateCommittedResource(&heap_properties,D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.stg_buff)));
		
		// Copy the index data to the index buffer.
		u8* p_idx_data_begin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(result.stg_buff->Map(0, &readRange, reinterpret_cast<void**>(&p_idx_data_begin)));
		memcpy(p_idx_data_begin, data, size_of_one_elem * count_x * count_y);
		result.stg_buff->Unmap(0, nullptr);
	}
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;
	
	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, size_of_one_elem * result.count_x * result.count_y, 1, allocation_format);
	res_n_view->view = create_uav_buffer_descriptor_view(device, result.heap_idx, heap->addr, heap->descriptor_size, result.count_x * result.count_y, size_of_one_elem, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER2D");

	return result;
}

inline func buffer_1d::create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count, u32 size_of_one_elem, u8* data, u32 register_index) -> buffer_1d {
	buffer_1d result = {};

	result.res_and_view_idx = r_n_v->count;
	result.register_index = register_index;
	result.count = count;
	result.heap_idx = heap->next_resource_idx++;
	result.data = data;
	result.size_of_data = size_of_one_elem * count;
	result.size_of_one_elem = size_of_one_elem;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);
	
	if(data) {
		auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size_of_one_elem * count);
		ThrowIfFailed(device->CreateCommittedResource(&heap_properties,D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.stg_buff)));
		
		// Copy the index data to the index buffer.
		u8* p_idx_data_begin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(result.stg_buff->Map(0, &readRange, reinterpret_cast<void**>(&p_idx_data_begin)));
		memcpy(p_idx_data_begin, data, size_of_one_elem * count);
		result.stg_buff->Unmap(0, nullptr);
	}
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;
	
	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, size_of_one_elem * result.count, 1, allocation_format);
	res_n_view->view = create_uav_buffer_descriptor_view(device, result.heap_idx, heap->addr, heap->descriptor_size, result.count, size_of_one_elem, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER1D");

	return result;
}

inline func buffer_vtex::create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u8* data, u32 size_of_data, u32 register_index) -> buffer_vtex {
	buffer_vtex result = {};

	result.register_index = register_index;
	result.heap_idx = heap->next_resource_idx++;
	result.size_of_data = size_of_data;
	result.data = data;
	result.res_and_view_idx = r_n_v->count;
	result.size_of_one_elem = sizeof(vertex);

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);

	{
		auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size_of_data);
		ThrowIfFailed(device->CreateCommittedResource(&heap_properties,D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.stg_buff)));
		
		// Copy the triangle data to the vertex buffer.
		u8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(result.stg_buff->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, data, size_of_data);
		result.stg_buff->Unmap(0, nullptr);
	}

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, result.size_of_data, 1, allocation_format);
	res_n_view->addr->SetName(L"VTEX_BUFFER1D");

	return result;
}

inline func buffer_idex::create	(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u8* data, u32 size_of_data, u32 instance_count, u32 register_index) -> buffer_idex {
	buffer_idex result = {};

	result.register_index = register_index;
	result.heap_idx = heap->next_resource_idx++;
	result.size_of_data = size_of_data;
	result.data = data;
	result.res_and_view_idx = r_n_v->count;
	result.size_of_one_elem = sizeof(u32);
	result.instance_count = instance_count;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);

	{
		auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(size_of_data);
		ThrowIfFailed(device->CreateCommittedResource(&heap_properties,D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.stg_buff)));
		
		// Copy the index data to the index buffer.
		u8* p_idx_data_begin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(result.stg_buff->Map(0, &readRange, reinterpret_cast<void**>(&p_idx_data_begin)));
		memcpy(p_idx_data_begin, data, size_of_data);
		result.stg_buff->Unmap(0, nullptr);

		// Initialize the index buffer view.
		result.view.BufferLocation = result.stg_buff->GetGPUVirtualAddress();
		result.view.Format = DXGI_FORMAT_R32_UINT;
		result.view.SizeInBytes = size_of_data;
	}

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, result.size_of_data, 1, allocation_format);
	res_n_view->addr->SetName(L"IDEX_BUFFER1D");

	return result;
}

inline func buffer_2d::recreate	(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count_x, u32 count_y, u32 size_of_one_elem, u8* data) -> buffer_2d {
	this->count_x = count_x;
	this->count_y = count_y;
	this->size_of_data = count_x * count_y * size_of_one_elem;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, this->size_of_data, 1, allocation_format);
	res_n_view->view = create_uav_buffer_descriptor_view(device, this->heap_idx, heap->addr, heap->descriptor_size, this->count_x * this->count_y, size_of_one_elem, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER2D");

	return *this;
}

inline func buffer_1d::recreate	(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count, u32 size_of_one_elem, u8* data) -> buffer_1d {
	this->count = count;
	this->size_of_data = count * size_of_one_elem;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, this->res_and_view_idx);

	if(data) {
		auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(this->size_of_data);
		ThrowIfFailed(device->CreateCommittedResource(&heap_properties,D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&this->stg_buff)));
		
		// Copy the index data to the index buffer.
		u8* p_idx_data_begin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(this->stg_buff->Map(0, &readRange, reinterpret_cast<void**>(&p_idx_data_begin)));
		memcpy(p_idx_data_begin, data, this->size_of_data);
		this->stg_buff->Unmap(0, nullptr);
	}
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;
	
	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, this->size_of_data, 1, allocation_format);
	res_n_view->view = create_uav_buffer_descriptor_view(device, this->heap_idx, heap->addr, heap->descriptor_size, this->count, size_of_one_elem, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER1D");

	return *this;
}

inline func texture_2d::create(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width ,u32 height, u8* texture_data, u32 register_index) -> texture_2d {
	texture_2d result = {};
	result.fmt = texture_u8rgba_norm;
	result.texture_data = texture_data;
	result.register_index = register_index;
	result.width_and_height = SET_WIDTH_HEIGHT(width, height);
	result.heap_idx = heap->next_resource_idx++;
	result.res_and_view_idx = r_n_v->count++;

	resource_and_view *resource_and_view = arena.load_ptr_by_idx(r_n_v->ptr, result.res_and_view_idx);

	// Describe and create a Texture2D.
	auto heap_type 			= D3D12_HEAP_TYPE_DEFAULT;
	auto heap_flags 		= D3D12_HEAP_FLAG_NONE;
	auto texture_layout 	= D3D12_TEXTURE_LAYOUT_UNKNOWN;
    auto resource_flags 	= D3D12_RESOURCE_FLAG_NONE;
	auto resource_states 	= D3D12_RESOURCE_STATE_COPY_DEST;
 	auto resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    auto allocation_format 	= DXGI_FORMAT_R8G8B8A8_UNORM;
	
	resource_and_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, width, height, allocation_format);
	resource_and_view->addr->SetName(L"TEXTURE_2D_");

	return result;
}

inline func texture_1d::create (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width, u8* texture_data, u32 register_index) -> texture_1d {
	texture_1d result = {};

	result.texture_data = texture_data;
	result.register_index = register_index;
	result.width = width;
	result.heap_idx = heap->next_resource_idx++;
	result.res_and_view_idx = r_n_v->count++;

	resource_and_view *resource_and_view = arena.load_ptr_by_idx(r_n_v->ptr, result.res_and_view_idx);

	// Describe and create a Texture1D.
	auto heap_type 			= D3D12_HEAP_TYPE_DEFAULT;
	auto heap_flags 		= D3D12_HEAP_FLAG_NONE;
	auto texture_layout 	= D3D12_TEXTURE_LAYOUT_UNKNOWN;
    auto resource_flags 	= D3D12_RESOURCE_FLAG_NONE;
	auto resource_states 	= D3D12_RESOURCE_STATE_COPY_DEST;
 	auto resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    auto allocation_format 	= DXGI_FORMAT_R8G8B8A8_UNORM;
	
	resource_and_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, width, 1, allocation_format);
	resource_and_view->addr->SetName(L"TEXTURE1D_");

	return result;
}

inline func render_target2d::create (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height, u32 register_idx) ->render_target2d {
	render_target2d result = {};

	result.register_index = register_idx;
	result.width = width;
	result.height = height;
	result.heap_idx = heap->next_resource_idx++;

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	result.res_and_view_idx = r_n_v->count;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, result.width, result.height, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, result.heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET2D");
	return result;
}

inline func render_target1d::create (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 register_idx) ->render_target1d {
	render_target1d result = {};

	result.register_index = register_idx;
	result.width = width;
	result.heap_idx = heap->next_resource_idx++;

	D3D12_TEXTURE_LAYOUT 		texture_layout 			= D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS 		resource_flags 			= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES 		resource_states 		= D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION 	resource_dimension 		= D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	D3D12_UAV_DIMENSION 		uav_dimension 			= D3D12_UAV_DIMENSION_TEXTURE1D;
	D3D12_HEAP_TYPE				heap_type 				= D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS 			heap_flags 				= D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT 				allocation_format 		= DXGI_FORMAT_R8G8B8A8_UNORM;

	result.res_and_view_idx = r_n_v->count;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * result.width, 1, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, result.heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET1D");
	return result;
}

inline func render_target2d::recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height) -> render_target2d {
	this->width = width;
	this->height = height;

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, this->res_and_view_idx);

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, this->width, this->height, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, this->heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET2D");
	return *this;
}

inline func render_target1d::recreate (ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width) -> render_target1d {
	this->width = width;

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, this->res_and_view_idx);

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * this->width, 1, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, this->heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET1D");
	return *this;
}