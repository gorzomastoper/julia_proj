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
	float padding[60]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(c_buffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct resource {
	enum FORMAT {
		c_buffer_256bytes = 0,
		uav_buffer_r32u,
		uav_buffer_r32i,
		uav_buffer_r32f,
		uav_buffer_r16u,
		uav_buffer_r16i,
		uav_buffer_r8u,
		uav_buffer_r8i,
		texture_f32,
		texture_u32,
		texture_i32,
		texture_u32_norm,
		texture_f32_rgba,
		texture_u8rgba_norm,
	};

	enum TYPE {
		constant_buffer = 0,
		texture2d,
		texture1d,
		buffer2d,
		buffer1d,
		sampler,
	};

	FORMAT format;
	TYPE type;

	union {
		struct { u32 res_and_view_idx; u32 width; 	u32 height; 		arena_ptr dsc_heap;}	texture2d;
		struct { u32 res_and_view_idx; u32 count_x;	u32 count_y; 		arena_ptr dsc_heap;} 	buffer2d;
		struct { u32 res_and_view_idx; u32 offset; 	u32 mapped_data_idx;arena_ptr dsc_heap;}	cbuffer;
		struct { u32 res_and_view_idx; u32 width; 	arena_ptr dsc_heap;} 						texture1d;
		struct { u32 res_and_view_idx; u32 count; 	arena_ptr dsc_heap;} 						buffer1d;
		struct { u32 res_and_view_idx; arena_ptr dsc_heap;} 									sampler;
	} info;
};

struct resource_and_view {
	ID3D12Resource				*addr;
	D3D12_CPU_DESCRIPTOR_HANDLE view;
};

struct descriptor_heap {
	ID3D12DescriptorHeap	*addr;
	u32 					descriptor_size;
	u32						next_resource_idx;
	u32						max_resources_count;
};

struct uav_buffer
{
	resource_and_view			res_n_view;
	u32 						index;
	u32 						width;
	u32 						height;
	DXGI_FORMAT 				format;
};

struct constant_buffer 
{
	ID3D12Resource				*addr;
	D3D12_CPU_DESCRIPTOR_HANDLE view;
	u32 						index;
	u32 						size; // NOTE(DH): Size is always should be 256 bytes (CBV boundaries)
	c_buffer					cb_scene_data;
	u8*							cb_data_begin;
	DXGI_FORMAT 				format;
};

struct pipeline {
	ID3D12RootSignature *root_signature;
	ID3D12PipelineState *state;
	ID3DBlob 			*shader_blob;
	u32					number_of_resources;
};

struct render_pass {
	pipeline curr_pipeline;
	pipeline prev_pipeline;
};

struct rendering_stage {
	descriptor_heap	dsc_heap;
	render_pass		rndr_pass_01;
	render_pass		rndr_pass_02;
	arena_ptr		resources_ptr;
	u32 			resource_count;
	// uav_buffer 		back_buffer;
	// uav_buffer 		atomic_buffer;
	// constant_buffer c_buffer;
};

struct rendered_entity
{
	ComPtr<ID3D12GraphicsCommandList>	bundle;

	ComPtr<ID3D12Resource> 				vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW 			vertex_buffer_view;
	ComPtr<ID3D12Resource>				constant_buffer;
	c_buffer							cb_scene_data;
	u8*									cb_data_begin;

	ComPtr<ID3DBlob> 					vertex_shader;
    ComPtr<ID3DBlob> 					pixel_shader;

	ComPtr<ID3D12Resource> 				texture_upload_heap;
	ComPtr<ID3D12Resource> 				texture;
};

struct dx_context
{
	ComPtr<ID3D12Device2> 				g_device;
	ComPtr<ID3D12CommandQueue> 			g_command_queue;
	ComPtr<IDXGISwapChain4> 			g_swap_chain;
	ComPtr<ID3D12Resource> 				g_back_buffers[g_NumFrames];
	ComPtr<ID3D12GraphicsCommandList> 	g_command_list;
	ComPtr<ID3D12GraphicsCommandList> 	g_imgui_command_list;
	ComPtr<ID3D12CommandAllocator> 		g_command_allocators[g_NumFrames];
	ComPtr<ID3D12CommandAllocator> 		g_imgui_command_allocators[g_NumFrames];
	ComPtr<ID3D12CommandAllocator> 		g_bundle_allocators[g_NumFrames];
	ComPtr<ID3D12DescriptorHeap> 		g_rtv_descriptor_heap;
	ComPtr<ID3D12DescriptorHeap> 		g_srv_descriptor_heap;
	ComPtr<ID3D12DescriptorHeap> 		g_imgui_descriptor_heap;
	ComPtr<ID3D12RootSignature> 		g_root_signature;
    ComPtr<ID3D12PipelineState> 		g_pipeline_state;

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
	ComPtr<ID3D12Fence> g_fence;
	uint64_t 			g_fence_value = 0;
	uint64_t 			g_frame_fence_values[g_NumFrames] = {};
	HANDLE 				g_fence_event;

	f32					time_elapsed; // NOTE(DH): In seconds
	f32					dt_for_frame;

	memory_arena dx_memory_arena;

	u32 entity_count;
	arena_ptr entities_array;

	u32 descriptors_count;
	u32 max_descriptor_heaps_count;
	arena_ptr descriptor_heaps;

	u32 resources_max_count;
	u32 resources_used;
	arena_ptr resources_and_views;

	rendering_stage compute_stage;

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

void resize(dx_context *context, u32 width, u32 height);
void update(dx_context *context);
void render(dx_context *context, ComPtr<ID3D12GraphicsCommandList> command_list);
ComPtr<ID3D12GraphicsCommandList> generate_command_buffer(dx_context *context);
ComPtr<ID3D12GraphicsCommandList> generate_compute_command_buffer(dx_context *context);
ComPtr<ID3D12GraphicsCommandList> generate_imgui_command_buffer(dx_context *context);

void 							move_to_next_frame		(dx_context *context);
void 							flush					(dx_context &context);
void 							present					(dx_context *context, ComPtr<IDXGISwapChain4> swap_chain);
dx_context 						init_dx					(HWND hwnd);
ComPtr<IDXGISwapChain4> 		get_current_swapchain	(dx_context *context);
CD3DX12_GPU_DESCRIPTOR_HANDLE 	get_uav_cbv_srv(u32 uav_idx, u32 uav_count, ID3D12Device2* device, ID3D12DescriptorHeap* desc_heap);

pipeline 						create_compute_pipeline	(ComPtr<ID3D12Device2> device, const char* entry_point_name, memory_arena *arena, ID3D12RootSignature *root_sig);
uav_buffer 						create_uav_texture_buffer(ComPtr<ID3D12Device2> device, u32 index, descriptor_heap dsc_heap, D3D12_HEAP_FLAGS heap_flags, u32 width, u32 height, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, D3D12_RESOURCE_DIMENSION res_dim);
CD3DX12_CPU_DESCRIPTOR_HANDLE 	create_uav_descriptor_view(ComPtr<ID3D12Device2> device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource);
uav_buffer 						create_uav_u32_buffer(ComPtr<ID3D12Device2> device, u32 index, descriptor_heap dsc_heap, D3D12_HEAP_FLAGS heap_flags, u32 width, u32 height, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, D3D12_RESOURCE_DIMENSION res_dim);
CD3DX12_CPU_DESCRIPTOR_HANDLE 	create_uav_u32_buffer_descriptor_view(ComPtr<ID3D12Device2> device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, u32 num_of_elements, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource);

ID3D12Resource*					allocate_data_on_gpu
(
	ComPtr<ID3D12Device2> 		device, 
	D3D12_HEAP_TYPE 			heap_type,
	D3D12_HEAP_FLAGS			heap_flags, 
	D3D12_TEXTURE_LAYOUT 		layout, 
	D3D12_RESOURCE_FLAGS 		flags, 
	D3D12_RESOURCE_STATES 		states, 
	D3D12_RESOURCE_DIMENSION 	dim, 
	u64 						width, 
	u64 						height, 
	DXGI_FORMAT 				format
);

CD3DX12_GPU_DESCRIPTOR_HANDLE 	get_gpu_handle_at(u32 index, ID3D12DescriptorHeap* desc_heap, u32 desc_size);
CD3DX12_CPU_DESCRIPTOR_HANDLE 	get_cpu_handle_at(u32 index, ID3D12DescriptorHeap* desc_heap, u32 desc_size);

void 	record_compute_stage(ComPtr<ID3D12Device2> device, rendering_stage stage, ComPtr<ID3D12GraphicsCommandList> command_list);
void 	recompile_shader(dx_context *ctx, rendering_stage rndr_stage);
void 	clear_render_target(dx_context ctx, ID3D12GraphicsCommandList *command_list, float clear_color[4]);
func 	initialize_compute_pipeline(ComPtr<ID3D12Device2> device, const char* entry_point_name, memory_arena *arena) -> pipeline;
func 	initialize_compute_resources(ComPtr<ID3D12Device2> device, u32 width, u32 height) -> uav_buffer;
func 	allocate_resources_and_views(memory_arena *arena, u32 max_resources_count) -> arena_ptr;
func 	allocate_descriptor_heaps(memory_arena *arena, u32 count) -> arena_ptr;
func 	allocate_descriptor_heap(dx_context *ctx, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, u32 count) -> descriptor_heap;
func 	create_compute_rendering_stage(dx_context *ctx, ID3D12Device2* device, D3D12_VIEWPORT viewport, memory_arena *arena) -> rendering_stage;