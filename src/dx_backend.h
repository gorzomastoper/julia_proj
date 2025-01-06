#pragma once

#define WIN32_LEAN_AND_MEAN //This will shrink the inclusion of windows.h to the essential functions
#include <windows.h>
#include "util/types.h"
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

	u32 entity_count;
	rendered_entity entities[32];

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
};

void resize(dx_context *context, u32 width, u32 height);
void dx_update(dx_context *context);
void dx_render(dx_context *context);
void flush(dx_context &context);
dx_context init_dx(HWND hwnd);