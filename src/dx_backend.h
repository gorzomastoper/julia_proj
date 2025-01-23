#pragma once

#define WIN32_LEAN_AND_MEAN //This will shrink the inclusion of windows.h to the essential functions
#include <windows.h>
#include "util/types.h"
#include "util/memory_management.h" //NOTE(DH): Include memory buffer manager
#include "dmath.h"

//NOTE(DH): This is PIX support

#include <wrl.h> //For WRL::ComPtr
#include <d3d12.h>
#include <d3dx12.h> //Helper functions from https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/Libraries/D3DX12
#include <dxgi1_6.h> //For SwapChain4
#include <DirectXMath.h> // For vector and matrix functions
#include <d3dcompiler.h>
//#include <D3D12Window.h>
#include <shellapi.h>//for CommandLineToArgvW .. getting command line arguments
#include <assert.h>
#include <algorithm>
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
struct scene_constant_buffer
{
	v4 offset;
	float padding[60]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(scene_constant_buffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct descriptor_heap {
	ComPtr<ID3D12DescriptorHeap>	addr;
	u32 							descriptor_size;
};

struct compute_buffer
{
	ComPtr<ID3D12Resource>	addr;
	D3D12_CPU_DESCRIPTOR_HANDLE 	view;
	u32 					index;
	u32 					width;
	u32 					height;
	DXGI_FORMAT 			format;
};

struct compute_constant_buffer 
{
	ComPtr<ID3D12Resource>		addr;
	D3D12_CPU_DESCRIPTOR_HANDLE view;
	u32 						index;
	u32 						size; // NOTE(DH): Size is always should be 256 bytes (CBV boundaries)
	scene_constant_buffer		cb_scene_data;
	u8*							cb_data_begin;
	DXGI_FORMAT 				format;
};

struct compute_c_buffer
{
	ComPtr<ID3D12Resource>	constant_buffer;
};

struct compute_pipeline {
	ID3D12RootSignature *root_signature;
	ComPtr<ID3D12PipelineState> state;
	ComPtr<ID3DBlob> shader_blob;
};

struct compute_rendering_stage {
	descriptor_heap			dsc_heap;
	compute_pipeline 		compute_pipeline;
	compute_buffer 			back_buffer;
	compute_constant_buffer c_buffer;
};

struct rendered_entity
{
	ComPtr<ID3D12GraphicsCommandList>	bundle;
	ComPtr<ID3D12GraphicsCommandList> 	cmd_list; //Command list for entity

	ComPtr<ID3D12Resource> 				vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW 			vertex_buffer_view;
	ComPtr<ID3D12Resource>				constant_buffer;
	scene_constant_buffer				cb_scene_data;
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
	rendered_entity *entities;

	compute_rendering_stage compute_stage;

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
CD3DX12_GPU_DESCRIPTOR_HANDLE 	get_uav(u32 uav_idx, ComPtr<ID3D12Device2> device, ComPtr<ID3D12DescriptorHeap> desc_heap);
CD3DX12_GPU_DESCRIPTOR_HANDLE 	get_cbv(u32 cbv_idx, ComPtr<ID3D12Device2> device, ComPtr<ID3D12DescriptorHeap> desc_heap);

compute_pipeline 				create_compute_pipeline	(ComPtr<ID3D12Device2> device, memory_arena *arena, ID3D12RootSignature *root_sig);
compute_buffer 					create_compute_buffer	(ComPtr<ID3D12Device2> device, descriptor_heap dsc_heap, u32 width, u32 height, DXGI_FORMAT format);
CD3DX12_CPU_DESCRIPTOR_HANDLE 	create_compute_descriptor_view	(u32 idx, ComPtr<ID3D12Device2> device, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, ID3D12Resource *p_resource);

ComPtr<ID3D12Resource> 			allocate_data_on_gpu
(
	ComPtr<ID3D12Device2> 		device, 
	D3D12_HEAP_TYPE 			heap_type, 
	D3D12_TEXTURE_LAYOUT 		layout, 
	D3D12_RESOURCE_FLAGS 		flags, 
	D3D12_RESOURCE_STATES 		states, 
	D3D12_RESOURCE_DIMENSION 	dim, 
	u64 						width, 
	u64 						height, 
	DXGI_FORMAT 				format
);

descriptor_heap 				allocate_descriptor_heap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, u32 count);
CD3DX12_GPU_DESCRIPTOR_HANDLE 	get_gpu_handle_at(u32 index, ComPtr<ID3D12DescriptorHeap> desc_heap, u32 desc_size);
CD3DX12_CPU_DESCRIPTOR_HANDLE 	get_cpu_handle_at(u32 index, ComPtr<ID3D12DescriptorHeap> desc_heap, u32 desc_size);

compute_rendering_stage create_compute_rendering_stage(ComPtr<ID3D12Device2> device, D3D12_VIEWPORT viewport, memory_arena *arena);
compute_buffer initialize_compute_resources(ComPtr<ID3D12Device2> device, u32 width, u32 height);
compute_pipeline initialize_compute_pipeline(ComPtr<ID3D12Device2> device, memory_arena *arena);
void record_compute_stage(ComPtr<ID3D12Device2> device, compute_rendering_stage stage, ComPtr<ID3D12GraphicsCommandList> command_list);