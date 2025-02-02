#include "util/types.h"
#include "util/memory_management.h" //NOTE(DH): Include memory buffer manager

#include "dx_backend.h"
#include <cassert>
#include <corecrt_wstdio.h>
#include <cstddef>
#include <cstdio>
#include <d3d12.h>
#include <debugapi.h>
#include <dxgi1_6.h>
#include <stdlib.h>
#include <synchapi.h>
#include <vector>
#include <winnt.h>
#include <wrl/client.h>

//NOTE(DH): Import IMGUI DX12 Implementation
#include "imgui-docking/backends/imgui_impl_dx12.h"
#include "imgui-docking/backends/imgui_impl_win32.h"
//NOTE(DH): Import IMGUI DX12 Implementation END

#define TIME_MAX 10000

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
	result.push_back(vertices_count-1);

	return result;
}

// Generate a simple black and white checkerboard texture.
std::vector<UINT8> generate_texture_data()
{
    const UINT rowPitch = 256 * 4;
    const UINT cellPitch = rowPitch >> 3;// The width of a cell in the checkboard texture.
    const UINT cellHeight = 256 >> 3;    // The height of a cell in the checkerboard texture.
    const UINT textureSize = rowPitch * 256;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += 4)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n] = 0xff;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}

IDXGIAdapter4* get_adapter(bool useWarp)
{
	IDXGIFactory4* dxgiFactory;
	UINT createFactoryFlags = 0;

#ifdef _DEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // _DEBUG

	//Create Adapter Factory
	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	IDXGIAdapter1* dxgiAdapter1;
	IDXGIAdapter4* dxgiAdapter4;

	if (useWarp)
	{
		//Retrieve the first adapter from the factory (as type Adapter1)
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		dxgiAdapter4 = (IDXGIAdapter4*)dxgiAdapter1;
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1,
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				dxgiAdapter4 = (IDXGIAdapter4*)dxgiAdapter1;
			}
		}
	}

	return dxgiAdapter4;
}

ID3D12Device2* create_device(IDXGIAdapter4* adapter) {
	ID3D12Device2* d3dDevice2;
	ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice2)));

	return d3dDevice2;
}

ID3D12CommandQueue* create_command_queue(ID3D12Device2* inDevice, D3D12_COMMAND_LIST_TYPE type)
{
	ID3D12CommandQueue* d3d12CommandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(inDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

	return d3d12CommandQueue;
}

bool check_tearing_support()
{
	BOOL allowTearing = FALSE;
	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport( //NOTE: CheckFeatureSupport is filling allowTearing variable
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing)
			)))
			{
				allowTearing = FALSE;
			}
		}

	}

	return allowTearing == TRUE;
}

void wait_for_gpu(ID3D12CommandQueue* command_queue, ID3D12Fence* fence, HANDLE fence_event,u64 *frame_fence_value)
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(command_queue->Signal(fence, *frame_fence_value));

    // Wait until the previous frame is finished.
    ThrowIfFailed(fence->SetEventOnCompletion(*frame_fence_value, fence_event));
    WaitForSingleObjectEx(fence_event, INFINITE, FALSE);

	// Increment the fence value for current frame
    (*frame_fence_value)++;
}

void move_to_next_frame(dx_context *context)
{
	// Schedule a Signal command in the queue
	const u64 current_fence_value = context->g_frame_fence_values[context->g_frame_index];
	ThrowIfFailed(context->g_command_queue->Signal(context->g_fence, current_fence_value));

	// Update the frame index
	context->g_frame_index = context->g_swap_chain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if(context->g_fence->GetCompletedValue() < context->g_frame_fence_values[context->g_frame_index])
	{
		ThrowIfFailed(context->g_fence->SetEventOnCompletion(context->g_frame_fence_values[context->g_frame_index], context->g_fence_event));
		WaitForSingleObjectEx(context->g_fence_event, INFINITE, FALSE);
	}

	context->g_frame_fence_values[context->g_frame_index] = current_fence_value + 1;
}

void present(dx_context *context)
{
	//Presenting the frame
	UINT syncInterval = context->g_vsync ? 1 : 0;
	UINT presentFlags = context->g_tearing_supported && !context->g_vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(context->g_swap_chain->Present(syncInterval, presentFlags));
}

IDXGISwapChain4* create_swap_chain(HWND hWnd, ID3D12CommandQueue* cmdQueue, u32 width, u32 height, u32 bufferCount)
{
	IDXGISwapChain4* dxgiSwapChain4;
	IDXGIFactory4* dxgiFactory4;

	UINT createFactoryFlags = 0;
#ifdef _DEBUG
	createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif // _DEBUG

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH; //Will scale backbuffer content to specified width and height (presentation target)
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //discard previous backbuffers content
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; //backbuffer transparency behavior
	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = check_tearing_support() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	IDXGISwapChain1* swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
		cmdQueue,
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	));

	//Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen will be enabled manually
	ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	// //Try casting and store to swapchain4
	// ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

	dxgiSwapChain4 = (IDXGISwapChain4*)swapChain1;
	return dxgiSwapChain4;
}

ID3D12DescriptorHeap *create_descriptor_heap(ComPtr<ID3D12Device> currentDevice, D3D12_DESCRIPTOR_HEAP_TYPE descHeapType, D3D12_DESCRIPTOR_HEAP_FLAGS descHeapFlags, uint32_t numDescriptors)
{
	ID3D12DescriptorHeap *result;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.NumDescriptors = numDescriptors;
	descHeapDesc.Type = descHeapType;
	descHeapDesc.Flags = descHeapFlags;
	ThrowIfFailed(currentDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&result)));

	return result;
}

void update_render_target_views(dx_context *context, ComPtr<ID3D12Device2> currentDevice, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> rtvDescrHeap)
{
	//RTV Descriptor size is vendor-dependendent.
	//We need to retrieve it in order to know how much space to reserve per each descriptor in the Descriptor Heap
	auto rtvDescriptorSize = currentDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//To iterate trough the Descriptor Heap, we use a Descriptor Handle initially pointing at the heap start
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescrHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < g_NumFrames; i++)
	{
		ID3D12Resource* back_buffer;
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));

		//Create RTV and store reference where rtvDescrHeap Handle is currently pointing to
		currentDevice->CreateRenderTargetView(back_buffer, nullptr, rtvHandle);

		context->g_back_buffers[i] = back_buffer;

		//Increment rtvDescrHeap Handle pointing position
		rtvHandle.Offset(rtvDescriptorSize);
	}

}

ID3D12CommandAllocator* create_command_allocator(ID3D12Device2* currentDevice, D3D12_COMMAND_LIST_TYPE cmdListType)
{
	ID3D12CommandAllocator* cmdAllocator;
	ThrowIfFailed(currentDevice->CreateCommandAllocator(cmdListType, IID_PPV_ARGS(&cmdAllocator)));

	return cmdAllocator;
}

template<typename T>
T* create_command_list(ComPtr<ID3D12Device2> currentDevice, ComPtr<ID3D12CommandAllocator> cmdAllocator, D3D12_COMMAND_LIST_TYPE cmdListType, ID3D12PipelineState *pInitialState, bool close)
{
	T* cmdList;
	ThrowIfFailed(currentDevice->CreateCommandList(0, cmdListType, cmdAllocator.Get(), pInitialState, IID_PPV_ARGS(&cmdList)));

	//NOTE: By default the Command list will be created in Open state, we manually need to close it!
	//This will allow resetting it at the beginning of the Render function before recording any command.
	if(close == true) ThrowIfFailed(cmdList->Close());

	return cmdList;
}

ID3D12Fence* create_fence(ID3D12Device2* currentDevice)
{
	ID3D12Fence* fence; 
	ThrowIfFailed(currentDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

	return fence;
}

//Used for handling CPU fence event
HANDLE create_fence_event_handle()
{
	HANDLE fenceEvent;
	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

//Command Queue Signal Fence (GPU)
u64 signal_fence(ComPtr<ID3D12CommandQueue> cmdQueue, ComPtr<ID3D12Fence> fence, u64& fenceValue) {
	u64 fenceValueForSignal = ++fenceValue;
	ThrowIfFailed(cmdQueue->Signal(fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

//This will stall the main thread up until the requested fence value is reached OR duration has expired
void wait_for_fence_value(ComPtr<ID3D12Fence> fence, u64 fenceValue, HANDLE fenceEvent,
	std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
		WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count())); //count returns the number of ticks
	}
}

//Flush will stall the main thread up until all the previous rendering commands sent on the command queue have been executed.
//This is done by signaling the referenced fence and waiting for the value to be reached. When that happens, fenceEvent gets executed.
void flush(dx_context &context)
{
	uint64_t fenceValueForSignal = signal_fence(context.g_command_queue, context.g_fence, context.g_fence_value);
	wait_for_gpu(context.g_command_queue, context.g_fence, context.g_fence_event, &context.g_frame_fence_values[context.g_frame_index]);
	//wait_for_fence_value(fence, fenceValue, fenceEvent);
}

//NOTE(DH): My own wrapper
D3D12_VIEWPORT create_viewport(f32 width, f32 height, f32 pos_x /*not in use*/, f32 pos_y /*not in use*/)
{
	return CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
}

//NOTE: whenever a resize happens, ALL the backbuffer resources referenced by the SwapChain need to be released.
//In order to do it, we first need to Flush the GPU for any "in-flight" command.
//After swapchain's resize back buffers, RTVs derscriptor heap is also updated.
void resize(dx_context *context, u32 width, u32 height)
{
	if (g_ClientWidth != width || g_ClientHeight != height)
	{
		//Don't allow 0 size swap chain back buffers
		g_ClientWidth = std::max(1u, width);
		g_ClientHeight = std::max(1u, height);

		// Update screen related data for proper viewport size and actual window size.
		// Scissors are not touched right now
		context->aspect_ratio = (f32)width / (f32)height;
		context->viewport = create_viewport((f32)g_ClientWidth, (f32)g_ClientHeight, 0, 0);

		//Flush the GPU queue to make sure the swap chain's back buffers
		//are not being referenced by an in-flight command list.
		flush(*context);
	}

	//NOTE: Any references to the back buffers must be released
	//before the swap chain can be resized!
	//All the per-frame fence values are also reset to the fence value of the current backbuffer index.
	for (int i = 0; i < g_NumFrames; ++i)
	{
		context->g_back_buffers[i]->Release();
		context->g_frame_fence_values[i] = context->g_frame_fence_values[context->g_frame_index];
	}

	// Resize compute shader buffer
	auto render_pass_01 = context->dx_memory_arena.load_by_idx<render_pass>				(context->compute_stage.render_passes.ptr, 0);
	auto pipeline 		= context->dx_memory_arena.get_ptr								(render_pass_01.curr_pipeline_c);
	auto back_buffer 	= context->dx_memory_arena.load_ptr_by_idx<res_render_target>	(pipeline->rtv_s.ptr, 0);
	auto atomic_buffer 	= context->dx_memory_arena.load_ptr_by_idx<res_buffer>			(pipeline->buffers.ptr,0);

	back_buffer->recreate_rt_2d			(context->g_device, context->dx_memory_arena, &context->compute_stage_heap, &context->resources_and_views, width, height);
	atomic_buffer->recreate_buffer_1d	(context->g_device, context->dx_memory_arena, &context->compute_stage_heap, &context->resources_and_views, width * height);
	// *back_buffer	= create_resource(
	// 	context, 
	// 	{.format = back_buffer->format, .type = back_buffer->type, .info.render_target2d = {.width = width, .height = height}},
	// 	context->resources_and_views, 
	// 	compute_stage->dsc_heap, 
	// 	back_buffer->info.render_target2d.register_idx,
	// 	true,
	// 	0
	// );

	// *atomic_buffer	= create_resource(
	// 	context, 
	// 	{.format = atomic_buffer->format, .type = atomic_buffer->type, .info.buffer1d = {.count = width * height}}, 
	// 	context->resources_and_views, 
	// 	compute_stage->dsc_heap, 
	// 	atomic_buffer->info.buffer1d.register_idx,
	// 	true,
	// 	1
	// );

	//Finally resizing the SwapChain and relative Descriptor Heap
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	ThrowIfFailed(context->g_swap_chain->GetDesc(&swapChainDesc)); //We get the current swapchain's desc so we can resize with same flags and same bugger color format
	ThrowIfFailed(context->g_swap_chain->ResizeBuffers(g_NumFrames, g_ClientWidth, g_ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
	
	context->g_frame_index = context->g_swap_chain->GetCurrentBackBufferIndex();

	update_render_target_views(context, context->g_device, context->g_swap_chain, context->g_rtv_descriptor_heap);
}

D3D12_FEATURE_DATA_ROOT_SIGNATURE create_feature_data_root_signature(ComPtr<ID3D12Device2> device)
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data_root_signature;
	feature_data_root_signature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data_root_signature, sizeof(feature_data_root_signature))))
	{
		feature_data_root_signature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	return feature_data_root_signature;
}


CD3DX12_GPU_DESCRIPTOR_HANDLE get_gpu_handle_at(u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(desc_heap->GetGPUDescriptorHandleForHeapStart(), index, desc_size);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE get_cpu_handle_at(u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(desc_heap->GetCPUDescriptorHandleForHeapStart(), index, desc_size);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE get_uav_cbv_srv_gpu_handle(u32 uav_idx, u32 uav_count, ID3D12Device2* device, ID3D12DescriptorHeap* desc_heap)
{
	auto descriptor_inc_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * uav_count;
	return CD3DX12_GPU_DESCRIPTOR_HANDLE (desc_heap->GetGPUDescriptorHandleForHeapStart(), uav_idx, descriptor_inc_size);
}


CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC create_root_signature_desc(CD3DX12_ROOT_PARAMETER1 *root_parameters, u32 root_parameters_count, D3D12_STATIC_SAMPLER_DESC sampler, D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {};
	root_signature_desc.Init_1_1(root_parameters_count, root_parameters, 1, &sampler, flags);

	return root_signature_desc;
}

D3D12_STATIC_SAMPLER_DESC create_sampler_desc
(
	D3D12_FILTER filter, 
	D3D12_TEXTURE_ADDRESS_MODE U, 
	D3D12_TEXTURE_ADDRESS_MODE V, 
	D3D12_TEXTURE_ADDRESS_MODE W,
	float mip_lod_bias,
	u32 max_aniso,
	D3D12_COMPARISON_FUNC cmp_func,
	D3D12_STATIC_BORDER_COLOR border_color,
	float min_lod,
	float max_lod,
	u32 shader_register,
	u32 shader_register_space,
	D3D12_SHADER_VISIBILITY shader_visibility
)
{
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = filter;
	sampler.AddressU = U;
	sampler.AddressV = V;
	sampler.AddressW = W;
	sampler.MipLODBias = mip_lod_bias;
	sampler.MaxAnisotropy = max_aniso;
	sampler.ComparisonFunc = cmp_func;
	sampler.BorderColor = border_color;
	sampler.MinLOD = min_lod;
	sampler.MaxLOD = max_lod;
	sampler.ShaderRegister = shader_register;
	sampler.RegisterSpace = shader_register_space;
	sampler.ShaderVisibility = shader_visibility;
	return sampler;
}

D3D12_STATIC_SAMPLER_DESC pixel_default_sampler_desc()
{
	return create_sampler_desc
	(
		D3D12_FILTER_MIN_MAG_MIP_POINT, 
		D3D12_TEXTURE_ADDRESS_MODE_BORDER, 
		D3D12_TEXTURE_ADDRESS_MODE_BORDER, 
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0,
		0,
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0.0f,
		D3D12_FLOAT32_MAX,
		0,
		0,
		D3D12_SHADER_VISIBILITY_PIXEL
	);
}

D3D12_STATIC_SAMPLER_DESC vertex_default_sampler_desc()
{
	return create_sampler_desc
	(
		D3D12_FILTER_MIN_MAG_MIP_POINT, 
		D3D12_TEXTURE_ADDRESS_MODE_BORDER, 
		D3D12_TEXTURE_ADDRESS_MODE_BORDER, 
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0,
		0,
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0.0f,
		D3D12_FLOAT32_MAX,
		0,
		0,
		D3D12_SHADER_VISIBILITY_VERTEX
	);
}

ID3DBlob* serialize_versioned_root_signature(CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC &desc, D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data_root_sign)
{
	ID3DBlob *error;
	ID3DBlob* signature;
	auto hr = D3DX12SerializeVersionedRootSignature(&desc, feature_data_root_sign.HighestVersion, &signature, &error);
	if(hr != 0)
	{
		OutputDebugString((char*)error->GetBufferPointer());
		ThrowIfFailed(hr);
	}
	
	return signature;
}

ID3D12RootSignature* create_root_signature(ID3D12Device2* device, ID3DBlob* signature)
{
	ID3D12RootSignature* root_signature;
	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
	
	return root_signature;
}

u32 get_module_file_name(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) 
{
	return GetModuleFileNameW(hModule, lpFilename, nSize);
}

u32 get_module_file_name(HMODULE hModule, LPSTR lpFilename, DWORD nSize) 
{
	return GetModuleFileName(hModule, lpFilename, nSize);
}

template<typename T>
T *get_shader_code_path(T *shader_file_name)
{
	u32 letter_count = 0;
	while(1) { if(shader_file_name[letter_count] == L'\0') break; else ++letter_count;};

	T shader_file_name_path[512];

	T assets_path[256]; //NOTE(DH): Max path for now!
		
	auto get_path = [](T *path, u32 pathSize) -> WCHAR* 
	{
		if (path == nullptr)
		{
			throw std::exception();
		}
		u32 length = get_module_file_name(nullptr, path, pathSize);
		u32 pointer = 0;
		for(u32 idx = length-1; idx > 0; --idx)
		{
			if(path[idx] == '\\') {
				pointer = idx;
				break;
			}
		}
		path[pointer] = '\0';
		return path;
	};

	_snwprintf_s(shader_file_name_path, _countof(assets_path),_countof(assets_path), L"%s\\%s", get_path(assets_path, _countof(assets_path)), shader_file_name);
	return shader_file_name_path;
}

ID3DBlob *compile_shader_from_file(wchar_t *filename_path, const D3D_SHADER_MACRO * pDefines, ID3DInclude * pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, u32 flags1, u32 flags2)
{
	ID3DBlob *result;
	ID3DBlob *error;
	//ThrowIfFailed(D3DCompileFromFile(filename_path, pDefines, pInclude, pEntrypoint, pTarget, flags1, flags2, &result, &error));
	auto hr = D3DCompileFromFile(filename_path, pDefines, pInclude, pEntrypoint, pTarget, flags1, flags2, &result, &error);
	if(hr != 0)
	{
		OutputDebugString((char*)error->GetBufferPointer());
		ThrowIfFailed(hr);
	}
	
	return result;
}

ComPtr<ID3DBlob> compile_shader(char *shader, u32 shader_size, const D3D_SHADER_MACRO * pDefines, ID3DInclude * pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, u32 flags1, u32 flags2)
{
	ComPtr<ID3DBlob> result;
	ThrowIfFailed(D3DCompile(shader,shader_size,"prekol",pDefines,pInclude, pEntrypoint, pTarget, flags1, flags2, &result, nullptr));
	
	return result;
}

// NOTE(DH): This is an IMPORTANT function, where we can see how to create resources and copy them onto GPU!
rendered_entity create_entity(dx_context &context)
{
	rendered_entity result = {};

	// NOTE(DH): Create root signature
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		
		CD3DX12_ROOT_PARAMETER1 root_parameters[2];
		root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		root_parameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = 
			create_root_signature_desc(root_parameters, _countof(root_parameters), pixel_default_sampler_desc(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ID3DBlob* versioned_signature 	= serialize_versioned_root_signature(root_signature_desc, create_feature_data_root_signature(context.g_device));
        context.g_root_signature 		= create_root_signature(context.g_device, versioned_signature);
    }

	// NOTE(DH): Create pipeline state
	{
	#if DEBUG
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#else
		UINT compileFlags = 0;
	#endif
		// NOTE(DH): Compile shaders (vertex and pixel)
		auto final_path 		= get_shader_code_path<WCHAR>(L"shaders.hlsl");
		WCHAR buffer[512];
		_snwprintf_s(buffer, sizeof(buffer), L"%s", final_path);
		result.vertex_shader 	= compile_shader_from_file(buffer, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0);
		result.pixel_shader 	= compile_shader_from_file(buffer, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0);

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
        psoDesc.pRootSignature = context.g_root_signature;
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(result.vertex_shader);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(result.pixel_shader);
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = blendDesc;//CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(context.g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&context.g_pipeline_state)));
	}

	// NOTE(DH): Create the command list, it's temporary and only for uploading resources!
	auto cmd_list = create_command_list<ID3D12GraphicsCommandList>(context.g_device, context.g_command_allocators[context.g_frame_index], D3D12_COMMAND_LIST_TYPE_DIRECT, context.g_pipeline_state, false);

 	// Create the vertex buffer.
	std::vector<vertex> circle_vertices = generate_circle_vertices(100, 512, 512, 32);
	std::vector<u32> circle_indices = generate_circle_indices(circle_vertices.size());

    {
        // Define the geometry for a triangle.
        // vertex quad_vertices[] =
        // {
        //     { { -1.0f, 1.0f, 0.0f}, { 0.0f, 1.0f, 0.0f, 0.0f } },
        //     { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
        //     { { -1.0, -1.0f, 0.0f}, { 0.0f, 0.0f, 1.0f, 0.0f } },
        //     { { -1.0, -1.0f, 0.0f}, { 0.0f, 0.0f, 1.0f, 0.0f } },
        //     { { 1.0, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
        //     { { 1.0, -1.0f, 0.0f}, { 1.0f, 0.0f, 1.0f, 0.0f } }
        // };
        // vertex quad_vertices[] =
        // {
        //     { { 0.0f, 32.0f, 0.0f}, { 0.0f, 1.0f, 0.0f, 0.0f } },
        //     { { 32.0f, 32.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
        //     { { 0.0, 0.0f, 0.0f}, { 0.0f, 0.0f, 1.0f, 0.0f } },
        //     { { 0.0, 0.0f, 0.0f}, { 0.0f, 0.0f, 1.0f, 0.0f } },
        //     { { 32.0, 32.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f } },
        //     { { 32.0, 0.0f, 0.0f}, { 1.0f, 0.0f, 1.0f, 0.0f } }
        // };

        // const UINT vertexBufferSize = sizeof(quad_vertices);

        const UINT vertexBufferSize = circle_vertices.size() * sizeof(vertex);
		const UINT index_buffer_size = circle_indices.size() * sizeof(u32);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
		{
			auto addr1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto addr2 = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

			ThrowIfFailed(context.g_device->CreateCommittedResource(&addr1,D3D12_HEAP_FLAG_NONE, &addr2, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.vertex_buffer)));

			// Copy the triangle data to the vertex buffer.
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
			ThrowIfFailed(result.vertex_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, circle_vertices.data(), vertexBufferSize);
			result.vertex_buffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view.
			result.vertex_buffer_view.BufferLocation = result.vertex_buffer->GetGPUVirtualAddress();
			result.vertex_buffer_view.StrideInBytes = sizeof(vertex);
			result.vertex_buffer_view.SizeInBytes = vertexBufferSize;
		}

		{
			auto addr1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto addr2 = CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size);

			ThrowIfFailed(context.g_device->CreateCommittedResource(&addr1,D3D12_HEAP_FLAG_NONE, &addr2, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.index_buffer)));

			// Copy the triangle data to the vertex buffer.
			UINT8* pIndexDataBegin;
			CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
			ThrowIfFailed(result.index_buffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, circle_indices.data(), index_buffer_size);
			result.index_buffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view.
			result.index_buffer_view.BufferLocation = result.index_buffer->GetGPUVirtualAddress();
			result.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
			result.index_buffer_view.SizeInBytes = index_buffer_size;
		}
    }

	// Create the texture
	{
		// Describe and create a Texture2D.
        D3D12_RESOURCE_DESC texture_desc = {};
        texture_desc.MipLevels = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.Width = 256;
        texture_desc.Height = 256;
        texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        texture_desc.DepthOrArraySize = 1;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		auto addr1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto addr2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		ThrowIfFailed(context.g_device->CreateCommittedResource(
            &addr1,
            D3D12_HEAP_FLAG_NONE,
            &texture_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&result.texture)));

		const UINT64 upload_buffer_size = GetRequiredIntermediateSize(result.texture, 0, 1);
		auto addr3 = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

		// Create the GPU upload buffer.
        ThrowIfFailed(context.g_device->CreateCommittedResource(
            &addr2,
            D3D12_HEAP_FLAG_NONE,
            &addr3,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&result.texture_upload_heap)));

		std::vector<UINT8> texture_data = generate_texture_data();

		D3D12_SUBRESOURCE_DATA texture_subresource_data = {};
		texture_subresource_data.pData = &texture_data[0];
		texture_subresource_data.RowPitch = 256 * 4; //4 bytes per pixel
		texture_subresource_data.SlicePitch = texture_subresource_data.RowPitch * 256; //256x256 Width x Height

		auto addr4 = CD3DX12_RESOURCE_BARRIER::Transition(result.texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		UpdateSubresources(cmd_list, result.texture, result.texture_upload_heap, 0, 0, 1, &texture_subresource_data);
        cmd_list->ResourceBarrier(1, &addr4);

		// Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = texture_desc.Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        context.g_device->CreateShaderResourceView(result.texture, &srv_desc, context.g_srv_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
	}

	// Create constant buffer
	{
		// This stuff is needed by one descriptor heap (SRV/CBV/UAV)
		CD3DX12_CPU_DESCRIPTOR_HANDLE desc_handle(context.g_srv_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
		u32 cbv_srv_size = context.g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		const u32 constant_buffer_size = sizeof(c_buffer); // CB size is required to be 256-byte aligned.

		auto addr1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto addr2 = CD3DX12_RESOURCE_DESC::Buffer(constant_buffer_size);

		ThrowIfFailed (
			context.g_device->CreateCommittedResource(&addr1, D3D12_HEAP_FLAG_NONE, &addr2, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result.constant_buffer))
		);

		// Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
        cbv_desc.BufferLocation = result.constant_buffer->GetGPUVirtualAddress();
        cbv_desc.SizeInBytes = constant_buffer_size;
		desc_handle.Offset(1, cbv_srv_size);
        context.g_device->CreateConstantBufferView(&cbv_desc, desc_handle);

		// Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE read_range(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(result.constant_buffer->Map(0, &read_range, (void**)(&result.cb_data_begin)));
        memcpy(result.cb_data_begin, &result.cb_scene_data, sizeof(result.cb_scene_data));
	}

	{
		// Create and record the bundle.
		{
			result.bundle = create_command_list<ID3D12GraphicsCommandList> (
				context.g_device,
				context.g_bundle_allocators[context.g_frame_index],
				D3D12_COMMAND_LIST_TYPE_BUNDLE,
				context.g_pipeline_state,
				false
			);
			
			result.bundle->SetGraphicsRootSignature(context.g_root_signature);
			result.bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			result.bundle->IASetVertexBuffers(0, 1, &result.vertex_buffer_view);
			result.bundle->IASetIndexBuffer(&result.index_buffer_view);
			result.bundle->DrawIndexedInstanced(circle_indices.size(), 1, 0, 0, 0);
			ThrowIfFailed(result.bundle->Close());
		}
	}

	// Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(cmd_list->Close());
    ID3D12CommandList* ppCommandLists[] = { cmd_list };
    context.g_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(context.g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context.g_fence)));
        context.g_frame_fence_values[context.g_frame_index]++;

        // Create an event handle to use for frame synchronization.
        context.g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (context.g_fence_event == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
		wait_for_gpu(context.g_command_queue, context.g_fence, context.g_fence_event, &context.g_frame_fence_values[context.g_frame_index]);
    }

	return result;
}

dx_context init_dx(HWND hwnd)
{
	dx_context result = {};
	result.g_hwnd = hwnd;
	printf("Initializing DX12...\n");

	printf("Initialize memory...\n");
	result.dx_memory_arena = initialize_arena(Megabytes(128));
	result.entities_array = result.dx_memory_arena.push_array<rendered_entity>(32);

	printf("Allocate resources from mem arena...\n");
	// TODO(DH): When nodes will be ready to use, rework this to be dynamic
	// This will create resource for translator, it will be created later
	// Here is the max resource count for all stages and pipelines
	result.resources_and_views = allocate_resources_and_views(&result.dx_memory_arena, 2048);

	//This means that the swap chain buffers will be resized to fill the 
	//total number of screen pixels(true 4K or 8K resolutions) when resizing the client area 
	//of the window instead of scaling the client area based on the DPI scaling settings.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	result.g_tearing_supported = check_tearing_support();

	//Create all the needed DX12 object
	IDXGIAdapter4* dxgiAdapter4 		= get_adapter			(result.g_use_warp);
	result.g_device 					= create_device			(dxgiAdapter4);
	result.g_command_queue 				= create_command_queue	(result.g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	result.g_swap_chain 				= create_swap_chain		(hwnd,result.g_command_queue, g_ClientWidth, g_ClientHeight, g_NumFrames);
	result.g_frame_index 				= result.g_swap_chain->GetCurrentBackBufferIndex();
	result.g_srv_descriptor_heap 		= create_descriptor_heap(result.g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 2);
	result.g_imgui_descriptor_heap 		= create_descriptor_heap(result.g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 64);
	result.g_rtv_descriptor_heap 		= create_descriptor_heap(result.g_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, g_NumFrames);
	result.g_rtv_descriptor_size 		= result.g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	result.scissor_rect = CD3DX12_RECT(0l, 0l, LONG_MAX, LONG_MAX);
	result.viewport 	= CD3DX12_VIEWPORT(0.0f, 0.0f, (f32)g_ClientWidth, (f32)g_ClientHeight);

	result.zmin 		= 0.1f;
	result.zmax 		= 100.0f;

	//Set aspect ratio
	result.aspect_ratio = (f32)g_ClientWidth / (f32)g_ClientHeight;

	update_render_target_views(&result, result.g_device, result.g_swap_chain, result.g_rtv_descriptor_heap);

	//Create Command List and Allocators
	{
		for (int i = 0; i < g_NumFrames; i++)//there needs to be one command allocator per each in-flight render frames
		{
			result.g_command_allocators[i] = create_command_allocator(result.g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
			result.g_bundle_allocators[i] = create_command_allocator(result.g_device, D3D12_COMMAND_LIST_TYPE_BUNDLE);
			result.g_imgui_command_allocators[i] = create_command_allocator(result.g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		}
	}

	result.g_command_list = create_command_list<ID3D12GraphicsCommandList>(result.g_device, result.g_command_allocators[result.g_frame_index], D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	result.g_imgui_command_list = create_command_list<ID3D12GraphicsCommandList>(result.g_device, result.g_imgui_command_allocators[result.g_frame_index], D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	result.g_compute_command_list = create_command_list<ID3D12GraphicsCommandList>(result.g_device, result.g_command_allocators[result.g_frame_index], D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);

	//Create fence objects
	result.g_fence = create_fence(result.g_device);
	result.g_fence_event = create_fence_event_handle();

	auto entities = result.dx_memory_arena.get_ptr<rendered_entity>(result.entities_array.ptr);
	entities[result.entities_array.count] = create_entity(result);
	// ++result.entities_array.count;

	// 1.) Create graphics rendering stage
	{
		// NOTE(DH): Allocate and init render stage first
		result.graphics_stage_heap = allocate_descriptor_heap(result.g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 32);

		WCHAR shader_path[] = L"shaders.hlsl";
		ID3DBlob* vertex_shader = compile_shader(result.g_device, shader_path, "VSMain", "vs_5_0");
		ID3DBlob* pixel_shader 	= compile_shader(result.g_device, shader_path, "PSMain", "ps_5_0");

		std::vector<u8> texture_data = generate_texture_data();

		// NOTE(DH): Get resources buffer
		res_texture texture = res_texture::create_texture_2d(result.g_device, result.dx_memory_arena, &result.graphics_stage_heap, &result.resources_and_views, 256, 256, texture_data.data(), 0);
		res_buffer cbuff 	= res_buffer::create_cbuffer	(result.g_device, result.dx_memory_arena, &result.graphics_stage_heap, &result.resources_and_views, 0);

		graphic_pipeline graph_pipeline = 
		graphic_pipeline::init__(&result.dx_memory_arena, 32)
			.bind_texture		(texture, result.dx_memory_arena)
			.bind_buffer		(cbuff, result.dx_memory_arena)
			.bind_vert_shader	(vertex_shader)
			.bind_frag_shader	(pixel_shader)
			.create_root_sig	(result.g_device, &result.dx_memory_arena)
			.finalize			(&result, result.g_device, &result.graphics_stage_heap);

		result.graphic_stage = 
		rendering_stage::init__(&result.dx_memory_arena, 2)
		.bind_graphic_pass(graph_pipeline, &result.dx_memory_arena);
	}

	// 2.) Create compute rendering stage
	{
		result.compute_stage_heap = allocate_descriptor_heap(result.g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 32);
		
		ID3DBlob* shader_for_pass_01 = compile_shader(result.g_device, L"example_03.hlsl", "Splat", "cs_5_0");
		ID3DBlob* shader_for_pass_02 = compile_shader(result.g_device, L"example_03.hlsl", "CSMain", "cs_5_0");

		res_render_target render_target = res_render_target::create_rt_2d(result.g_device, result.dx_memory_arena, &result.compute_stage_heap, &result.resources_and_views, result.viewport.Width, result.viewport.Height, 0);
		res_buffer	hist_buffer = res_buffer::create_buffer_1d(result.g_device, result.dx_memory_arena, &result.compute_stage_heap, &result.resources_and_views, result.viewport.Width * result.viewport.Height, 1);
		res_buffer	cbuff = res_buffer::create_cbuffer(result.g_device, result.dx_memory_arena, &result.compute_stage_heap, &result.resources_and_views, 0);

		compute_pipeline compute_pipeline_pass_01 = 
		compute_pipeline::init__(&result.dx_memory_arena, 32)
			.bind_rndr_target	(render_target, result.dx_memory_arena)
			.bind_buffer		(hist_buffer, result.dx_memory_arena)
			.bind_buffer		(cbuff, result.dx_memory_arena)
			.bind_shader		(shader_for_pass_01)
			.create_root_sig	(result.g_device, &result.dx_memory_arena)
			.finalize			(&result, result.g_device, &result.compute_stage_heap);

		compute_pipeline compute_pipeline_pass_02 = 
		compute_pipeline::init__(&result.dx_memory_arena, 32)
			.bind_rndr_target	(render_target, result.dx_memory_arena)
			.bind_buffer		(hist_buffer, result.dx_memory_arena)
			.bind_buffer		(cbuff, result.dx_memory_arena)
			.bind_shader		(shader_for_pass_02)
			.create_root_sig	(result.g_device, &result.dx_memory_arena)
			.finalize			(&result, result.g_device, &result.compute_stage_heap);

		result.compute_stage = 
		rendering_stage::init__	(&result.dx_memory_arena, 2)
			.bind_compute_pass	(compute_pipeline_pass_01, &result.dx_memory_arena)
			.bind_compute_pass	(compute_pipeline_pass_02, &result.dx_memory_arena);
	}

	//All should be initialised, show the window
	result.g_is_initialized = true;

	return result;
}

inline func graphic_pipeline::create_root_sig(ID3D12Device2* device, memory_arena *arena) -> graphic_pipeline {

	CD3DX12_DESCRIPTOR_RANGE1 ranges[this->number_of_resources];
	auto textures 	= arena->get_array(this->textures);
	auto buffers 	= arena->get_array(this->buffers);
	auto rtv_s 		= arena->get_array(this->rtv_s);

	u32 current_resource = 0;
	for(u32 i = 0; i < this->buffers.count; ++i) {
		switch(buffers[i].type) {
			case res_buffer::TYPE::constant_buffer: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, buffers[i].info.cbuffer.register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			}; break;

			case res_buffer::TYPE::buffer2d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, buffers[i].info.buffer2d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;

			case res_buffer::TYPE::buffer1d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, buffers[i].info.buffer1d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;
		}
	}

	for(u32 i = 0; i < this->textures.count; ++i) {
		switch(textures[i].type) {
			case res_texture::TYPE::texture2d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, textures[i].info.texture2d.register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;

			case res_texture::TYPE::texture1d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, textures[i].info.texture1d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;
		}
	}

	for(u32 i = 0; i < this->rtv_s.count; ++i) {
		switch(rtv_s[i].type) {
			case res_render_target::TYPE::render_target2d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, rtv_s[i].info.render_target2d.register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;

			case res_render_target::TYPE::render_target1d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, rtv_s[i].info.render_target1d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;
		}
	}
		
	CD3DX12_ROOT_PARAMETER1 root_parameters[this->number_of_resources];
	for(u32 i = 0; i < this->number_of_resources; ++i) {
		root_parameters[i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL);
	}
	// root_parameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
	// root_parameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = 
	create_root_signature_desc(root_parameters, this->number_of_resources, pixel_default_sampler_desc(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ID3DBlob* versioned_signature 	= serialize_versioned_root_signature(root_signature_desc, create_feature_data_root_signature(device));
    this->root_signature 			= create_root_signature(device, versioned_signature);
	return *this;
}

inline func graphic_pipeline::bind_texture(res_texture texture, memory_arena arena) -> graphic_pipeline {
	res_texture *array = arena.get_array(this->textures);
	assert(this->textures.count <= this->textures.capacity);
	array[this->textures.count++] = texture;
	++this->number_of_resources;
	return *this;
}

inline func graphic_pipeline::bind_buffer(res_buffer buffer, memory_arena arena) -> graphic_pipeline {
	res_buffer *array = arena.get_array(this->buffers);
	assert(this->buffers.count <= this->buffers.capacity);
	array[this->buffers.count++] = buffer;
	++this->number_of_resources;
	return *this;
}

inline func graphic_pipeline::bind_rndr_target(res_render_target rt, memory_arena arena) -> graphic_pipeline {
	res_render_target *array = arena.get_array(this->rtv_s);
	assert(this->rtv_s.count <= this->rtv_s.capacity);
	array[this->rtv_s.count++] = rt;
	++this->number_of_resources;
	return *this;
}

inline func graphic_pipeline::bind_vert_shader(ID3DBlob* shader) -> graphic_pipeline {
	this->vertex_shader_blob = shader;
	return *this;
}

inline func graphic_pipeline::bind_frag_shader(ID3DBlob* shader) -> graphic_pipeline {
	this->pixel_shader_blob = shader;
	return *this;
}

inline func graphic_pipeline::finalize(dx_context *ctx, ID3D12Device2* device, descriptor_heap *heap) -> graphic_pipeline {

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

	auto cmd_list = create_command_list<ID3D12GraphicsCommandList>(device, ctx->g_command_allocators[ctx->g_frame_index], D3D12_COMMAND_LIST_TYPE_DIRECT, ctx->g_pipeline_state, false);
	cmd_list->SetName(L"Uload to GPU list");

	auto finalize_texture_2D = [device](u32 idx, ID3D12GraphicsCommandList *cmd_list, descriptor_heap* heap, u32 width, u32 height, ID3D12Resource *texture_resource, DXGI_FORMAT format, res_texture *texture_dsc){
		// Create the texture
		{
			const UINT64 upload_buffer_size = GetRequiredIntermediateSize(texture_resource, 0, 1);

			// NOTE(DH): Here we create heap properties with texture data to be copied to from staiging buffer
			auto addr2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto addr3 = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

			// Create the GPU upload buffer.
			ThrowIfFailed(device->CreateCommittedResource(
				&addr2,
				D3D12_HEAP_FLAG_NONE,
				&addr3,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&texture_dsc->info.texture2d.staging_buff)));

			D3D12_SUBRESOURCE_DATA texture_subresource_data = {};
			texture_subresource_data.pData = &texture_dsc->info.texture2d.texture_data[0];
			texture_subresource_data.RowPitch = width * 4; //4 bytes per pixel
			texture_subresource_data.SlicePitch = texture_subresource_data.RowPitch * height; //256x256 Width x Height

			// NOTE(DH): This is important, because we use this resource not only in PIXEL SHADER BUT PROBABLY IN VERTEX SHADER, WE NEED EXPLICITLY MARK THIS FLAGS AS IT IS!!!
			auto addr4 = CD3DX12_RESOURCE_BARRIER::Transition(texture_resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE/* | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
			UpdateSubresources(cmd_list, texture_resource, texture_dsc->info.texture2d.staging_buff, 0, 0, 1, &texture_subresource_data);
			cmd_list->ResourceBarrier(1, &addr4);

			// Describe and create a SRV for the texture.
			u32 inc_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			D3D12_CPU_DESCRIPTOR_HANDLE view = create_srv_descriptor_view(device, idx, heap->addr, inc_size, format, D3D12_SRV_DIMENSION_TEXTURE2D, texture_resource);
		}
	};

	res_texture *textures = ctx->dx_memory_arena.get_array(this->textures);

	for(u32 i = 0 ; i < this->textures.count; ++i) {
		switch(textures[i].type) {
			case res_texture::texture2d: {
				resource_and_view *r_n_v = &ctx->dx_memory_arena.get_array(ctx->resources_and_views)[textures[i].info.texture2d.res_and_view_idx];
				u32 width = GET_WIDTH(textures[i].info.texture2d.width_and_height);
				u32 height = GET_HEIGHT(textures[i].info.texture2d.width_and_height);
				finalize_texture_2D(textures[i].info.texture2d.heap_idx, cmd_list, heap, width, height, r_n_v->addr, DXGI_FORMAT_R8G8B8A8_UNORM, &textures[i]);
			}; break;

			default: { printf("Unlucky, its not done yet!");} break;
		}
	}

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


inline func compute_pipeline::create_root_sig (ID3D12Device2 *device, memory_arena *arena) -> compute_pipeline {
	CD3DX12_DESCRIPTOR_RANGE1 ranges[this->number_of_resources];
	auto buffers 	= arena->get_array(this->buffers);
	auto textures 	= arena->get_array(this->textures);
	auto rtv_s 		= arena->get_array(this->rtv_s);

	u32 current_resource = 0;
	for(u32 i = 0; i < this->buffers.count; ++i) {
		switch(buffers[i].type) {
			case res_buffer::TYPE::constant_buffer: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, buffers[i].info.cbuffer.register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			}; break;

			case res_buffer::TYPE::buffer2d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, buffers[i].info.buffer2d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;

			case res_buffer::TYPE::buffer1d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, buffers[i].info.buffer1d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;
		}
	}

	for(u32 i = 0; i < this->textures.count; ++i) {
		switch(textures[i].type) {
			case res_texture::TYPE::texture2d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, textures[i].info.texture2d.register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			}; break;

			case res_texture::TYPE::texture1d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, textures[i].info.texture1d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;
		}
	}

	for(u32 i = 0; i < this->rtv_s.count; ++i) {
		switch(rtv_s[i].type) {
			case res_render_target::TYPE::render_target2d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, rtv_s[i].info.render_target2d.register_index, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;

			case res_render_target::TYPE::render_target1d: {
				ranges[current_resource++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, rtv_s[i].info.render_target1d.register_index, 0 ,D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
			}; break;
		}
	}
		
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
	return *this;
}

inline func compute_pipeline::bind_texture (res_texture texture, memory_arena arena) -> compute_pipeline {
	res_texture *array = arena.get_array(this->textures);
	assert(this->textures.count <= this->textures.capacity);
	array[this->textures.count++] = texture;
	++this->number_of_resources;
	return *this;
}

inline func compute_pipeline::bind_rndr_target (res_render_target rt, memory_arena arena) -> compute_pipeline {
	res_render_target *array = arena.get_array(this->rtv_s);
	assert(this->rtv_s.count <= this->rtv_s.capacity);
	array[this->rtv_s.count++] = rt;
	++this->number_of_resources;
	return *this;
}

inline func compute_pipeline::bind_buffer (res_buffer buffer, memory_arena arena) -> compute_pipeline {
	res_buffer *array = arena.get_array(this->buffers);
	assert(this->buffers.count <= this->buffers.capacity);
	array[this->buffers.count++] = buffer;
	++this->number_of_resources;
	return *this;
}

inline func compute_pipeline::bind_sampler (res_sampler 	sampler, memory_arena arena) -> compute_pipeline {
	return *this;
}

inline func compute_pipeline::bind_shader (ID3DBlob* shader) 	-> compute_pipeline {
	this->shader_blob = shader;
	return *this;
}

inline func compute_pipeline::finalize (dx_context *ctx, ID3D12Device2* device, descriptor_heap *heap) -> compute_pipeline {
	return *this;
}


inline func res_texture::create_texture_2d(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width ,u32 height, u8* texture_data, u32 register_index) -> res_texture {
	res_texture result = {};
	result.type = res_texture::texture2d;
	result.format = res_texture::texture_u8rgba_norm;
	result.info.texture2d.texture_data = texture_data;
	result.info.texture2d.register_index = register_index;
	result.info.texture2d.width_and_height = SET_WIDTH_HEIGHT(width, height);
	result.info.texture2d.heap_idx = heap->next_resource_idx++;
	result.info.texture2d.res_and_view_idx = r_n_v->count++;

	resource_and_view *resource_and_view = arena.load_ptr_by_idx(r_n_v->ptr, result.info.texture2d.res_and_view_idx);

	// Describe and create a Texture2D.
    D3D12_RESOURCE_DESC texture_desc = {};
    texture_desc.MipLevels = 1;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    texture_desc.DepthOrArraySize = 1;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	auto addr1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	
	// NOTE(DH): Create texture
	ThrowIfFailed(device->CreateCommittedResource(
        &addr1,
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource_and_view->addr)));
	
	resource_and_view->addr->SetName(L"TEXTURE_2D_");

	return result;
}

inline func res_texture::create_texture_1d(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 width, u8* texture_data, u32 register_index) -> res_texture {
	res_texture result = {};
	result.type = res_texture::texture1d;
	result.info.texture1d.texture_data = texture_data;
	result.info.texture1d.register_index = register_index;
	result.info.texture1d.width = width;
	result.info.texture1d.heap_idx = heap->next_resource_idx++;
	result.info.texture1d.res_and_view_idx = r_n_v->count++; // TODO(DH): This will be setupped in finalize function!

	// Describe and create a Texture2D.
    D3D12_RESOURCE_DESC texture_desc = {};
    texture_desc.MipLevels = 1;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.Width = width;
    texture_desc.Height = 1;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    texture_desc.DepthOrArraySize = 1;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;

	// NOTE(DH): Here we create heap properties with texture data to be holded
	auto addr1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	
	// NOTE(DH): Create staging buffer
	ThrowIfFailed(device->CreateCommittedResource(
        &addr1,
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&result.info.texture1d.staging_buff)));

		return result;
}

inline func res_texture::recreate_texture_2d(ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height,	u8* texture_data) -> res_texture {

}

inline func res_texture::recreate_texture_1d(ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u8* texture_data) -> res_texture {

}

inline func res_render_target::create_rt_2d(ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height, u32 register_idx) ->res_render_target {
	res_render_target result = {};
	result.type = res_render_target::render_target2d;
	result.info.render_target2d.register_index = register_idx;
	result.info.render_target2d.width = width;
	result.info.render_target2d.height = height;
	result.info.render_target2d.heap_idx = heap->next_resource_idx++;

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	result.info.render_target2d.res_and_view_idx = r_n_v->count;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, result.info.render_target2d.width, result.info.render_target2d.height, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, result.info.render_target2d.heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET2D");
	return result;
}

inline func res_render_target::create_rt_1d(ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 register_idx) ->res_render_target {
	res_render_target result = {};
	result.type = res_render_target::render_target1d;
	result.info.render_target1d.register_index = register_idx;
	result.info.render_target1d.width = width;
	result.info.render_target1d.heap_idx = heap->next_resource_idx++;

	D3D12_TEXTURE_LAYOUT 		texture_layout 			= D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS 		resource_flags 			= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES 		resource_states 		= D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION 	resource_dimension 		= D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	D3D12_UAV_DIMENSION 		uav_dimension 			= D3D12_UAV_DIMENSION_TEXTURE1D;
	D3D12_HEAP_TYPE				heap_type 				= D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS 			heap_flags 				= D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT 				allocation_format 		= DXGI_FORMAT_R8G8B8A8_UNORM;

	result.info.render_target1d.res_and_view_idx = r_n_v->count;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * result.info.render_target1d.width, 1, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, result.info.render_target1d.heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET1D");
	return result;
}

inline func res_render_target::recreate_rt_2d(ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width, u32 height) -> res_render_target {
	this->info.render_target2d.width = width;
	this->info.render_target2d.height = height;

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, this->info.render_target2d.res_and_view_idx);

	res_n_view->addr->Release();

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, this->info.render_target2d.width, this->info.render_target2d.height, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, this->info.render_target2d.heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET2D");
	return *this;
}

inline func res_render_target::recreate_rt_1d(ID3D12Device2* device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 width) -> res_render_target {
	this->info.render_target1d.width = width;

	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, this->info.render_target1d.res_and_view_idx);

	res_n_view->addr->Release();

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * this->info.render_target1d.width, 1, allocation_format);
	res_n_view->view = create_uav_descriptor_view(device, this->info.render_target1d.heap_idx, heap->addr, heap->descriptor_size, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"RENDER_TARGET1D");
	return *this;
}

inline func res_buffer::create_cbuffer(ID3D12Device2 *device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 register_index) -> res_buffer {
	res_buffer result = {};
	result.type = res_buffer::constant_buffer;

	u32 size_of_cbuffer = sizeof(c_buffer); // NOTE(DH): IMPORTANT, this is always will be 256 bytes!
	result.info.cbuffer.register_index = register_index;
	result.info.cbuffer.res_and_view_idx = r_n_v->count;
	result.info.cbuffer.heap_idx = heap->next_resource_idx++;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);
	
	DXGI_FORMAT 				format 		= DXGI_FORMAT_UNKNOWN;
	D3D12_HEAP_TYPE 			heap_type 	= D3D12_HEAP_TYPE_UPLOAD;
	D3D12_HEAP_FLAGS 			heap_flags 	= D3D12_HEAP_FLAG_NONE;
	D3D12_TEXTURE_LAYOUT 		layout 		= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS 		flags 		= D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES 		states 		= D3D12_RESOURCE_STATE_GENERIC_READ;
	D3D12_RESOURCE_DIMENSION 	dim 		= D3D12_RESOURCE_DIMENSION_BUFFER;

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, layout, flags, states, dim, size_of_cbuffer, 1, format);
	res_n_view->view = get_cpu_handle_at(result.info.cbuffer.heap_idx, heap->addr, heap->descriptor_size);
	res_n_view->addr->SetName(L"C_BUFFER");
	// NOTE(DH): IMPORTANT, Always needs to be 256 bytes!
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {.BufferLocation = res_n_view->addr->GetGPUVirtualAddress(), .SizeInBytes = size_of_cbuffer};
	device->CreateConstantBufferView(&cbv_desc, res_n_view->view);
	// Map and initialize the constant buffer. We don't unmap this until the
	// app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(res_n_view->addr->Map(0, &read_range, (void**)(&result.info.cbuffer.mapped_view)));

	return result;
}

inline func res_buffer::create_buffer_2d(ID3D12Device2 *device, memory_arena arena, descriptor_heap *heap, arena_array<resource_and_view> *r_n_v, u32 count_x, u32 count_y, u32 register_index) -> res_buffer {
	res_buffer result = {};
	result.type = res_buffer::buffer2d;
	result.info.buffer2d.res_and_view_idx = r_n_v->count;
	result.info.buffer2d.register_index = register_index;
	result.info.buffer2d.count_x = count_x;
	result.info.buffer2d.count_y = count_y;
	result.info.buffer2d.heap_idx = heap->next_resource_idx++;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;
	
	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * result.info.buffer2d.count_x * result.info.buffer2d.count_y, 1, allocation_format);
	res_n_view->view = create_uav_u32_buffer_descriptor_view(device, result.info.buffer2d.heap_idx, heap->addr, heap->descriptor_size, result.info.buffer2d.count_x * result.info.buffer2d.count_y, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER2D");

	return result;
}

inline func res_buffer::create_buffer_1d (ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count, u32 register_index) -> res_buffer {
	res_buffer result = {};
	result.type = res_buffer::buffer1d;
	result.info.buffer1d.res_and_view_idx = r_n_v->count;
	result.info.buffer1d.register_index = register_index;
	result.info.buffer1d.count = count;
	result.info.buffer1d.heap_idx = heap->next_resource_idx++;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;
	
	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * result.info.buffer1d.count, 1, allocation_format);
	res_n_view->view = create_uav_u32_buffer_descriptor_view(device, result.info.buffer1d.heap_idx, heap->addr, heap->descriptor_size, result.info.buffer1d.count, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER1D");

	return result;
}

inline func res_buffer::recreate_buffer_2d	(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count_x, u32 count_y) -> res_buffer {
	this->info.buffer2d.count_x = count_x;
	this->info.buffer2d.count_y = count_y;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, r_n_v->count++);
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;
	
	res_n_view->addr->Release();

	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * this->info.buffer2d.count_x * this->info.buffer2d.count_y, 1, allocation_format);
	res_n_view->view = create_uav_u32_buffer_descriptor_view(device, this->info.buffer2d.heap_idx, heap->addr, heap->descriptor_size, this->info.buffer2d.count_x * this->info.buffer2d.count_y, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER2D");

	return *this;
}

inline func res_buffer::recreate_buffer_1d	(ID3D12Device2* device, memory_arena arena, descriptor_heap* heap, arena_array<resource_and_view> *r_n_v, u32 count) -> res_buffer {
	this->info.buffer1d.count = count;

	resource_and_view* res_n_view = arena.load_ptr_by_idx(r_n_v->ptr, this->info.buffer1d.res_and_view_idx);
	
	D3D12_TEXTURE_LAYOUT texture_layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DIMENSION resource_dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D3D12_UAV_DIMENSION uav_dimension = D3D12_UAV_DIMENSION_BUFFER;
	D3D12_HEAP_TYPE	heap_type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS;
	DXGI_FORMAT allocation_format = DXGI_FORMAT_UNKNOWN;

	res_n_view->addr->Release();
	
	res_n_view->addr = allocate_data_on_gpu(device, heap_type, heap_flags, texture_layout, resource_flags, resource_states, resource_dimension, sizeof(u32) * this->info.buffer1d.count, 1, allocation_format);
	res_n_view->view = create_uav_u32_buffer_descriptor_view(device, this->info.buffer1d.res_and_view_idx, heap->addr, heap->descriptor_size, this->info.buffer1d.count, allocation_format, uav_dimension, res_n_view->addr);
	res_n_view->addr->SetName(L"BUFFER1D");

	return *this;
}

void update(dx_context *ctx)
{
	ctx->time_elapsed += ctx->dt_for_frame * ctx->speed_multiplier;
	ctx->time_elapsed = fmod(ctx->time_elapsed, TIME_MAX);

	ctx->common_cbuff_data.offset.z = ctx->viewport.Width;
	ctx->common_cbuff_data.offset.w = ctx->viewport.Height;

	rendered_entity *entity = ctx->dx_memory_arena.get_array<rendered_entity>(ctx->entities_array);
	float speed = 0.005f;
	float offset_bounds = 1.25f;

	ctx->common_cbuff_data.offset.y += speed;
	if(ctx->common_cbuff_data.offset.y > offset_bounds)
		ctx->common_cbuff_data.offset.y = -offset_bounds;

	ctx->common_cbuff_data.offset.x = ctx->time_elapsed;
	ctx->common_cbuff_data.mouse_pos = V4(ImGui::GetMousePos().x, ImGui::GetMousePos().y, 1.0, 1.0);

	render_pass			pass 		= ctx->dx_memory_arena.load_by_idx<render_pass>(ctx->compute_stage.render_passes.ptr, 0);
	compute_pipeline 	pipeline 	= ctx->dx_memory_arena.load(pass.curr_pipeline_c);
	res_buffer*			buffers		= ctx->dx_memory_arena.get_array(pipeline.buffers);

	for(u32 i = 0; i < ctx->compute_stage.render_passes.count; ++i) {
		for(u32 idx = 0; idx < pipeline.buffers.count; ++idx) {
			switch(buffers[idx].type)
			{
				case res_buffer::constant_buffer: {
					memcpy(buffers[idx].info.cbuffer.mapped_view, &ctx->common_cbuff_data, sizeof(ctx->common_cbuff_data));
					//return;
				}; break;

				default: {}; break;
			}
		}
	}

	// TODO(DH): Move this to ^
	// memcpy(entity->cb_data_begin, &ctx->common_cbuff_data, sizeof(ctx->common_cbuff_data));
}

void recompile_shader(dx_context *ctx, rendering_stage rndr_stage)
{
	rendering_stage stage = ctx->compute_stage; // NOTE(DH): This needs to be compute stage :)
	// ctx->compute_stage.rndr_pass_01.prev_pipeline = rndr_stage.rndr_pass_01.curr_pipeline;
	// ctx->compute_stage.rndr_pass_01.curr_pipeline = create_compute_pipeline(ctx->g_device, "Splat",&ctx->dx_memory_arena, ctx->compute_stage.rndr_pass_01.curr_pipeline.root_signature);

	// ctx->compute_stage.rndr_pass_02.prev_pipeline = rndr_stage.rndr_pass_02.curr_pipeline;
	// ctx->compute_stage.rndr_pass_02.curr_pipeline = create_compute_pipeline(ctx->g_device, "CSMain",&ctx->dx_memory_arena, ctx->compute_stage.rndr_pass_02.curr_pipeline.root_signature);
}

void imgui_draw_canvas(dx_context *ctx)
{
	bool enabled = true;
	if(ImGui::Begin("Node editor", &enabled, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
		// ImGui::SetCursorPos(ImVec2(0, 0));
        static ImVector<ImVec2> points;
        static ImVec2 scrolling(0.0f, 0.0f);
        static ImVec2 child_moving(0.0f, 0.0f);
        static bool opt_enable_context_menu = true;
        static bool adding_line = false;
		ImGuiIO& io = ImGui::GetIO();

		ImGui::Text("This is canvas position: %f, %f", ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
		

        // Typically you would use a BeginChild()/EndChild() pair to benefit from a clipping region + own scrolling.
        // Here we demonstrate that this can be replaced by simple offsetting + custom drawing + PushClipRect/PopClipRect() calls.
        // To use a child window instead we could use, e.g:
        //      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));      // Disable padding
        //      ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));  // Set a background color
        //      ImGui::BeginChild("canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove);
        //      ImGui::PopStyleColor();
        //      ImGui::PopStyleVar();
        //      [...]
        //      ImGui::EndChild();

        // Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
        if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

        // Draw border and background color
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

        // This will catch our interactions
        ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool is_hovered = ImGui::IsItemHovered(); // Hovered
        const bool is_active = ImGui::IsItemActive();   // Held
        const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y); // Lock scrolled origin
        const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

        // Pan (we use a zero mouse threshold when there's no context menu)
        // You may decide to make that threshold dynamic based on whether the mouse is hovering something etc.
        const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
        if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
        {
            scrolling.x += io.MouseDelta.x;
            scrolling.y += io.MouseDelta.y;
        }

        // Context menu (under default mouse threshold)
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        if (opt_enable_context_menu && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
            ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
        if (ImGui::BeginPopup("context"))
        {
            if (adding_line)
                points.resize(points.size() - 2);
            adding_line = false;
            if (ImGui::MenuItem("Remove one", NULL, false, points.Size > 0)) { points.resize(points.size() - 2); }
            if (ImGui::MenuItem("Remove all", NULL, false, points.Size > 0)) { points.clear(); }
            ImGui::EndPopup();
        }

        // Draw grid + all lines in the canvas
		draw_list->PushClipRect(canvas_p0, canvas_p1, true);
        {
            const float GRID_STEP = 128.0f;
            for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x; x += GRID_STEP)
                draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
            for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y; y += GRID_STEP)
                draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(200, 200, 200, 40));
        }

		draw_list->PopClipRect();

		ImVec2 node_size = ImVec2(300, 200);

		ImGui::SetNextWindowPos(canvas_p0 + child_moving + scrolling);
		ImGui::PushClipRect(canvas_p0 + ImVec2(1,1), canvas_p1 - ImVec2(1,1), false);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 0, 0, 255));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::InvisibleButton("Node", node_size, ImGuiButtonFlags_MouseButtonLeft);
		ImGui::BeginChild("canvas", node_size, ImGuiChildFlags_None, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);
		if (ImGui::IsItemActive())
        {
            child_moving.x += io.MouseDelta.x;
            child_moving.y += io.MouseDelta.y;
        }
		ImGui::Button("Click me just like that!");
		ImGui::Text("Generic text");
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopClipRect();
    }
	ImGui::End();
}

void imgui_generate_commands(dx_context *ctx)
{
	{
		bool active_tool = true;
		ImGui::Begin("Core Menu", &ctx->g_is_menu_active, ImGuiWindowFlags_MenuBar);
		if(ImGui::BeginMenuBar())
		{
			if(ImGui::BeginMenu("File"))
			{
				if(ImGui::MenuItem("Open", "Ctrl+O")) {}
				if(ImGui::MenuItem("Save", "Ctrl+S")) {}
				if(ImGui::MenuItem("Exit", "Ctrl+W")) { ctx->g_is_quitting = true;}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		ImGui::Text("This is some useful text.");
		ImGui::Text("Memory used by render backend: %u of %u kb", ctx->dx_memory_arena.used / Kilobytes(1), ctx->dx_memory_arena.size / Kilobytes(1));
		ImGui::Text("Time Elapsed: %f ", ctx->time_elapsed);
		ImGui::SliderFloat("Speed multiplier", &ctx->speed_multiplier, 0.1f, 100.0f);
		if (ImGui::Button("Button")) 
			printf("Luca Abelle");

		if(ImGui::Button("Recompile Shader"))
			ctx->g_recompile_shader = true;

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 255, 255));
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::PopStyleColor();
		ImGui::End();
	}
}

void set_render_target
(
	ComPtr<ID3D12GraphicsCommandList> cmd_list,
	ComPtr<ID3D12DescriptorHeap> dsc_heap,
	u32 rtv_num,
	bool single_handle_to_rtv_range,
	u32 frame_idx,
	u32 dsc_size
)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(dsc_heap->GetCPUDescriptorHandleForHeapStart(), frame_idx, dsc_size);
    cmd_list->OMSetRenderTargets(rtv_num, &rtvHandle, single_handle_to_rtv_range, nullptr);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE get_rtv_descriptor_handle(ComPtr<ID3D12DescriptorHeap> dsc_heap, u32 frame_idx, u32 dsc_size)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(dsc_heap->GetCPUDescriptorHandleForHeapStart(), frame_idx, dsc_size);
}

void record_reset_cmd_allocator(ComPtr<ID3D12CommandAllocator> cmd_alloc)
{
	cmd_alloc->Reset();
}

ID3D12CommandAllocator* get_command_allocator(u32 frame_idx, ID3D12CommandAllocator** cmd_alloc_array)
{
	return cmd_alloc_array[frame_idx];
}

CD3DX12_RESOURCE_BARRIER barrier_transition(ID3D12Resource * p_resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
	return CD3DX12_RESOURCE_BARRIER::Transition(p_resource, state_before, state_after);
}

void record_dsc_heap(ComPtr<ID3D12GraphicsCommandList> cmd_list, ID3D12DescriptorHeap* ppHeaps[], u32 descriptors_count)
{
	cmd_list->SetDescriptorHeaps(descriptors_count, ppHeaps);
}

void record_graphics_root_dsc_table(u32 root_param_idx, ComPtr<ID3D12GraphicsCommandList> cmd_list, ComPtr<ID3D12DescriptorHeap> dsc_heap)
{
	cmd_list->SetGraphicsRootDescriptorTable(root_param_idx, dsc_heap->GetGPUDescriptorHandleForHeapStart());
}

void record_graphics_root_dsc_table(u32 root_param_idx, ComPtr<ID3D12GraphicsCommandList> cmd_list, CD3DX12_GPU_DESCRIPTOR_HANDLE dsc_handle)
{
	cmd_list->SetGraphicsRootDescriptorTable(root_param_idx, dsc_handle);
}

void record_reset_cmd_list(ID3D12GraphicsCommandList* cmd_list, ID3D12CommandAllocator *cmd_alloc, ID3D12PipelineState *pipeline_state)
{
	cmd_list->Reset(cmd_alloc, pipeline_state);
}

void record_resource_barrier(u32 num_of_barriers, CD3DX12_RESOURCE_BARRIER transition, ComPtr<ID3D12GraphicsCommandList> cmd_list)
{
	cmd_list->ResourceBarrier(num_of_barriers, &transition);
}

void record_imgui_cmd_list(ComPtr<ID3D12GraphicsCommandList> cmd_list)
{
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list.Get());
}

void record_graphics_root_signature(ComPtr<ID3D12GraphicsCommandList> cmd_list, ComPtr<ID3D12RootSignature> root_signature)
{
	cmd_list->SetGraphicsRootSignature(root_signature.Get());
}

void record_compute_root_signature(ComPtr<ID3D12GraphicsCommandList> cmd_list, ComPtr<ID3D12RootSignature> root_signature)
{
	cmd_list->SetComputeRootSignature(root_signature.Get());
}

void record_viewports(u32 num_of_viewports, D3D12_VIEWPORT viewport, ComPtr<ID3D12GraphicsCommandList> cmd_list)
{
	cmd_list->RSSetViewports(num_of_viewports, &viewport);
}

void record_scissors(u32 num_of_scissors, D3D12_RECT scissor_rect, ComPtr<ID3D12GraphicsCommandList> cmd_list)
{
	cmd_list->RSSetScissorRects(num_of_scissors, &scissor_rect);
}

IDXGISwapChain4* get_current_swapchain(dx_context *context)
{
	return context->g_swap_chain;
}

void present(dx_context *context, IDXGISwapChain4* swap_chain)
{
	//Presenting the frame
	UINT syncInterval = context->g_vsync ? 1 : 0;
	UINT presentFlags = context->g_tearing_supported && !context->g_vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(swap_chain->Present(syncInterval, presentFlags));
}

ID3D12GraphicsCommandList* generate_imgui_command_buffer(dx_context *context)
{
	//NOTE(DH): Here is the IMGUI commands generated
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();

	imgui_draw_canvas(context);
	imgui_generate_commands(context);
	
	ImGui::Render();
	ImGui::UpdatePlatformWindows();

	auto imgui_cmd_allocator = get_command_allocator(context->g_frame_index, context->g_imgui_command_allocators);

	//NOTE(DH): Reset command allocators for the next frame
	record_reset_cmd_allocator(imgui_cmd_allocator);

	//NOTE(DH): Reset command lists for the next frame
	record_reset_cmd_list(context->g_imgui_command_list, imgui_cmd_allocator, context->g_pipeline_state);

	//NOTE(DH): IMGUI set descriptor heap
	{
		ID3D12DescriptorHeap* ppHeaps[] = { context->g_imgui_descriptor_heap};
		record_dsc_heap(context->g_imgui_command_list, ppHeaps, _countof(ppHeaps));
	}

	// Indicate that the back buffer will be used as a render target.
	auto transition = barrier_transition(context->g_back_buffers[context->g_frame_index],D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	record_resource_barrier(1, transition, context->g_imgui_command_list);

	// NOTE(DH): Set render target for IMGUI
	set_render_target(context->g_imgui_command_list, context->g_rtv_descriptor_heap, 1, FALSE, context->g_frame_index, context->g_rtv_descriptor_size);

	//NOTE(DH): Here is the IMGUI commands packed into it's own command list
	record_imgui_cmd_list(context->g_imgui_command_list);

	// Indicate that the back buffer will now be used to present.
	transition = barrier_transition(
		context->g_back_buffers[context->g_frame_index], 
		D3D12_RESOURCE_STATE_RENDER_TARGET, 
		D3D12_RESOURCE_STATE_PRESENT
	);

	record_resource_barrier(1,  transition, context->g_imgui_command_list);

	return context->g_imgui_command_list;
}

ID3D12GraphicsCommandList* generate_command_buffer(dx_context *context)
{
	auto cmd_allocator = get_command_allocator(context->g_frame_index, context->g_command_allocators);
	auto srv_dsc_heap = context->graphics_stage_heap;
	// auto srv_dsc_heap = context->g_srv_descriptor_heap;

	rendering_stage stage = context->graphic_stage;
	render_pass only_pass = context->dx_memory_arena.load_by_idx(stage.render_passes.ptr, 0);
	graphic_pipeline g_p = context->dx_memory_arena.load(only_pass.curr_pipeline_g);

	//NOTE(DH): Reset command allocators for the next frame
	record_reset_cmd_allocator(cmd_allocator);

	//NOTE(DH): Reset command lists for the next frame
	record_reset_cmd_list(context->g_command_list, cmd_allocator, g_p.state);
	context->g_command_list->SetName(L"GRAPHICS COMMAND LIST");

	//TODO(DH): Need to create normal system for entities, when choosen wrong entity program is crashes!
	// rendered_entity entity = context->dx_memory_arena.load_by_idx<rendered_entity>(context->entities_array.ptr,0); //NOTE(DH): Be carefull, because this is too loousy for now

	// record_graphics_root_signature(context->g_command_list, context->g_root_signature);

	context->g_command_list->SetGraphicsRootSignature(g_p.root_signature);
	context->g_command_list->SetPipelineState(g_p.state);

	// NOTE(DH): DX12 set descriptor heap and graphics root dsc table
	// {
	// 	CD3DX12_GPU_DESCRIPTOR_HANDLE desc_handle(srv_dsc_heap->GetGPUDescriptorHandleForHeapStart());
	// 	u32 cbv_srv_size = context->g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// 	desc_handle.Offset(1, cbv_srv_size);

	// 	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap};
	// 	record_dsc_heap(context->g_command_list, ppHeaps, _countof(ppHeaps));
	// 	record_graphics_root_dsc_table(0,context->g_command_list, srv_dsc_heap);
	// 	record_graphics_root_dsc_table(1,context->g_command_list, desc_handle);
	// }
	// ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap};
	// record_dsc_heap(context->g_command_list, ppHeaps, _countof(ppHeaps));

	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap.addr};
	record_dsc_heap(context->g_command_list, ppHeaps, _countof(ppHeaps));

	auto buffers 	= context->dx_memory_arena.get_array(g_p.buffers);
	auto textures 	= context->dx_memory_arena.get_array(g_p.textures);
	auto rtv_s 		= context->dx_memory_arena.get_array(g_p.rtv_s);

	u32 current_resource = 0;
	for(u32 i = 0; i < g_p.buffers.count; ++i) {
		switch(buffers[i].type) {
			case res_buffer::TYPE::constant_buffer: {
				context->g_command_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(buffers[i].info.cbuffer.heap_idx, 1, context->g_device, srv_dsc_heap.addr));
				++current_resource;
			}; break;

			case res_buffer::TYPE::buffer2d: {
				context->g_command_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(buffers[i].info.buffer2d.heap_idx, 1, context->g_device, srv_dsc_heap.addr));
				++current_resource;
			}; break;

			case res_buffer::TYPE::buffer1d: {
				context->g_command_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(buffers[i].info.buffer1d.heap_idx, 1, context->g_device, srv_dsc_heap.addr));
				++current_resource;
			}; break;
		}
	}

	for(u32 i = 0; i < g_p.textures.count; ++i) {
		switch(textures[i].type) {
			case res_texture::TYPE::texture2d: {
				context->g_command_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(textures[i].info.texture2d.heap_idx, 1, context->g_device, srv_dsc_heap.addr));
				++current_resource;
			}; break;

			case res_texture::TYPE::texture1d: {
				context->g_command_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(textures[i].info.texture1d.heap_idx, 1, context->g_device, srv_dsc_heap.addr));
				++current_resource;
			}; break;
		}
	}

	for(u32 i = 0; i < g_p.rtv_s.count; ++i) {
		switch(rtv_s[i].type) {
			case res_render_target::TYPE::render_target2d: {
				context->g_command_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(rtv_s[i].info.render_target2d.heap_idx, 1, context->g_device, srv_dsc_heap.addr));
				++current_resource;
			}; break;

			case res_render_target::TYPE::render_target1d: {
				context->g_command_list->SetGraphicsRootDescriptorTable(current_resource, get_uav_cbv_srv_gpu_handle(rtv_s[i].info.render_target1d.heap_idx, 1, context->g_device, srv_dsc_heap.addr));
				++current_resource;
			}; break;
		}
	}

	// for(u32 i = 0; i < 2; ++i)
	// {
	// 	context->g_command_list->SetGraphicsRootDescriptorTable(i, get_uav_cbv_srv_gpu_handle(i, 1, context->g_device, srv_dsc_heap));
	// }

	record_viewports(1, context->viewport, context->g_command_list);
	record_scissors(1, context->scissor_rect, context->g_command_list);

    // Indicate that the back buffer will be used as a render target.
	auto transition = barrier_transition(context->g_back_buffers[context->g_frame_index],D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	record_resource_barrier(1, transition, context->g_command_list);

	set_render_target(context->g_command_list, context->g_rtv_descriptor_heap, 1, FALSE, context->g_frame_index, context->g_rtv_descriptor_size);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    context->g_command_list->ClearRenderTargetView
	(
		get_rtv_descriptor_handle(context->g_rtv_descriptor_heap, context->g_frame_index, context->g_rtv_descriptor_size), 
		clearColor, 
		0, 
		nullptr
	);
	
	// Execute bundle
	// context->g_command_list->ExecuteBundle(entity.bundle);

    // Indicate that the back buffer will now be used to present.
	transition = barrier_transition(context->g_back_buffers[context->g_frame_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    record_resource_barrier(1, transition, context->g_command_list);

	//Finally executing the filled Command List into the Command Queue
	return context->g_command_list;
}

void render(dx_context *context, ID3D12GraphicsCommandList* command_list)
{
	ThrowIfFailed(command_list->Close());
	ID3D12CommandList* const cmd_lists[] = {command_list};

	//Finally executing the filled Command List into the Command Queue
	context->g_command_queue->ExecuteCommandLists(1, cmd_lists);
}

// NOTE(DH): Compute pipeline {

// NOTE(DH): Allocate and increment count
func allocate_descriptor_heap(ID3D12Device2* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, u32 count) -> descriptor_heap
{
	descriptor_heap heap = {};
	heap.addr = create_descriptor_heap(device, heap_type, flags, count);
	heap.descriptor_size = device->GetDescriptorHandleIncrementSize(heap_type);

	return heap;
}

ID3D12Resource *allocate_data_on_gpu(ID3D12Device2* device, D3D12_HEAP_TYPE heap_type, D3D12_HEAP_FLAGS heap_flags, D3D12_TEXTURE_LAYOUT layout, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES states, D3D12_RESOURCE_DIMENSION dim, u64 width, u64 height, DXGI_FORMAT format)
{
	ID3D12Resource *resource;
	D3D12_RESOURCE_DESC description = {};
	description.Flags = flags;
	description.Dimension = dim;
	description.Width = width;
	description.Height = height;
	description.Format = format;
	description.Layout = layout;
	description.MipLevels = 1;
	description.DepthOrArraySize = 1;
	description.SampleDesc.Count = 1;

	CD3DX12_HEAP_PROPERTIES default_heap = CD3DX12_HEAP_PROPERTIES(heap_type);
	device->CreateCommittedResource(&default_heap, heap_flags, &description, states, nullptr,IID_PPV_ARGS(&resource));

	return resource;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE create_uav_descriptor_view(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE view = get_cpu_handle_at(index, desc_heap, desc_size);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.Format = format;
	uav_desc.ViewDimension = dim;

	device->CreateUnorderedAccessView(p_resource, nullptr, &uav_desc, view);
	return view;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE create_srv_descriptor_view(ID3D12Device2 *device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, DXGI_FORMAT format, D3D12_SRV_DIMENSION dim, ID3D12Resource *p_resource)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE view = get_cpu_handle_at(index, desc_heap, desc_size);
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = format;
    srv_desc.ViewDimension = dim;
    srv_desc.Texture2D.MipLevels = 1;
	
	device->CreateShaderResourceView(p_resource, &srv_desc, view);
	return view;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE create_uav_u32_buffer_descriptor_view(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, u32 num_of_elements, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE view = get_cpu_handle_at(index, desc_heap, desc_size);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.Format = format;
	uav_desc.ViewDimension = dim;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = num_of_elements;
	uav_desc.Buffer.StructureByteStride = sizeof(u32);
	uav_desc.Buffer.CounterOffsetInBytes = 0;
	uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(p_resource, nullptr, &uav_desc, view);
	return view;
}

func create_graphic_pipeline(ID3D12Device2* device, memory_arena *arena, ID3D12RootSignature *root_sig) -> graphic_pipeline
{
	// pipeline result = {};
	// result.ty = tmplate.ty;

	// //compile shaders
	// auto final_path = get_shader_code_path<WCHAR>(arena->get_ptr<WCHAR>(tmplate.shader_path), arena);
	
	// // NOTE(DH): You can also compile with just plain code text without file!
	// // result.shader_blob = compile_shader(shader_text, sizeof(shader_text), nullptr, nullptr, "CSMain", "cs_5_0", 0, 0);

	// switch(tmplate.ty) {
	// 	case pipeline::compute: {
	// 		LPCSTR entry_name = arena->get_ptr<char>(tmplate.compute_part.entry_name);
	// 		LPCSTR target_name = arena->get_ptr<char>(tmplate.compute_part.version_name);
	// 		result.compute_part.shader_blob = compile_shader_from_file(final_path, nullptr, nullptr, entry_name, target_name, 0, 0); // TODO(DH): Add compile flags, see for example in your code!

	// 		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
	// 		desc.pRootSignature = root_sig;
	// 		desc.CS = CD3DX12_SHADER_BYTECODE(result.compute_part.shader_blob);
	// 		ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&result.state)));

	// 		// Assign root signature
	// 		result.root_signature = root_sig;
	// 	}; break;

	// 	// NOTE(DH): We need to remember that pixel shader called after vertex shader and
	// 	// result of vertex shader is passed to pixel shader input. Here we assign and build how this
	// 	// input should look like!
	// 	case pipeline::graphics: {
			 // Describe and create the graphics pipeline state object (PSO).
			// D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			// psoDesc.InputLayout = { input_element_descs, _countof(input_element_descs) };
			// psoDesc.pRootSignature = root_sig;
			// psoDesc.VS = CD3DX12_SHADER_BYTECODE(result.vertex_shader);
			// psoDesc.PS = CD3DX12_SHADER_BYTECODE(result.pixel_shader);
			// psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			// psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			// psoDesc.DepthStencilState.DepthEnable = FALSE;
			// psoDesc.DepthStencilState.StencilEnable = FALSE;
			// psoDesc.SampleMask = UINT_MAX;
			// psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			// psoDesc.NumRenderTargets = 1;
			// psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			// psoDesc.SampleDesc.Count = 1;
			// ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&result.state)));
	// 	}; break;
	// }

	// return result;
}

func compile_shader(ID3D12Device2 *device, WCHAR *shader_path, LPCSTR entry_name, LPCSTR version_name) -> ID3DBlob* {
	//compile shaders
	WCHAR *final_path = get_shader_code_path<WCHAR>(shader_path);
	WCHAR path_buffer[512];
	_snwprintf_s(path_buffer, sizeof(path_buffer), L"%s", final_path);
	
	// NOTE(DH): You can also compile with just plain code text without file!
	// result.shader_blob = compile_shader(shader_text, sizeof(shader_text), nullptr, nullptr, "CSMain", "cs_5_0", 0, 0);

	return compile_shader_from_file(path_buffer, nullptr, nullptr, entry_name, version_name, 0, 0); // TODO(DH): Add compile flags, see for example in your code!
}

void clear_render_target(dx_context ctx, ID3D12GraphicsCommandList *command_list, float clear_color[4])
{
	record_resource_barrier(1, barrier_transition(ctx.g_back_buffers[ctx.g_frame_index],D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),command_list);

	// Record commands.
    command_list->ClearRenderTargetView
	(
		get_rtv_descriptor_handle(ctx.g_rtv_descriptor_heap, ctx.g_frame_index, ctx.g_rtv_descriptor_size),
		clear_color,
		0, 
		nullptr
	);

	record_resource_barrier(1, barrier_transition(ctx.g_back_buffers[ctx.g_frame_index],D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT),command_list);
}

void record_compute_stage(dx_context context, ComPtr<ID3D12Device2> device, rendering_stage stage, ID3D12GraphicsCommandList* command_list)
{
	// 1.) Bind root signature and pipeline
	
	// render_pass *passes = context.dx_memory_arena.get_array(stage.render_passes);
	// compute_pipeline pipeline_first = context.dx_memory_arena.load_by_idx(passes[0].curr_pipeline_c, 0);
	// compute_pipeline pipeline_second = context.dx_memory_arena.load_by_idx(passes[0].curr_pipeline_c, 1);

	// command_list->SetComputeRootSignature	(pipeline_first.root_signature);
	// command_list->SetPipelineState			(pipeline_first.state);

	// // 2.) Bind resources needed for pipeline
	// ID3D12DescriptorHeap* ppHeaps[] = { context.compute_stage_heap.addr};
	// command_list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	// for(u32 i = 0; i < stage.resources_array.count; ++i)
	// {
	// 	command_list->SetComputeRootDescriptorTable(i, get_uav_cbv_srv_gpu_handle(i, 1, device.Get(), context.compute_stage_heap.addr));
	// }

	// resource *back_buffer = context.dx_memory_arena.load_ptr_by_idx<resource>(stage.resources_array.ptr, 0);

	// // 3.) Dispatch compute shader
	// u32 width 		= back_buffer->info.render_target2d.width;
	// u32 height 		= back_buffer->info.render_target2d.height;
	// u32 dispatchX 	= (width / 8) + 1;
	// u32 dispatchY 	= (height / 8) + 1;

	// command_list->Dispatch(32, 32, 12);

	// command_list->SetComputeRootSignature(stage.rndr_pass_02.curr_pipeline.root_signature);
	// command_list->SetPipelineState(stage.rndr_pass_02.curr_pipeline.state);
	// command_list->Dispatch(dispatchX, dispatchY, 1);

	// // 4.) Copy result from back buffer to screen
	// ID3D12Resource* screen_buffer = context.g_back_buffers[context.g_frame_index];

	// record_resource_barrier(1,  barrier_transition(screen_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST), command_list);
	// auto texture = context.dx_memory_arena.load_ptr_by_idx<resource_and_view>(context.resources_and_views.ptr, back_buffer->info.render_target2d.res_and_view_idx);
	// command_list->CopyResource(screen_buffer, texture->addr);
	// record_resource_barrier(1,  barrier_transition(screen_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT), command_list);
}

ID3D12GraphicsCommandList* generate_compute_command_buffer(dx_context *ctx)
{
	// auto cmd_allocator = get_command_allocator(ctx->g_frame_index, ctx->g_command_allocators);
	// auto srv_dsc_heap = ctx->g_srv_descriptor_heap;

	// //NOTE(DH): Reset command allocators for the next frame
	// record_reset_cmd_allocator(cmd_allocator);

	// //NOTE(DH): Reset command lists for the next frame
	// rendering_stage compute_stage = ctx->dx_memory_arena.load_by_idx<rendering_stage>(ctx->rendering_stages.ptr, 0);
	// record_reset_cmd_list(ctx->g_compute_command_list, cmd_allocator, compute_stage.rndr_pass_01.curr_pipeline.state);

	// record_compute_stage(*ctx, ctx->g_device, compute_stage, ctx->g_compute_command_list);

	// //Finally executing the filled Command List into the Command Queue
	// return ctx->g_compute_command_list;
}

// NOTE(DH): Compute pipeline }

// NOTE(DH): Here we allocate all our descriptor heaps for different pipelines
func allocate_descriptor_heaps(memory_arena *arena, u32 count) -> arena_array<descriptor_heap> {
	printf("Allocate for %s count: %u...\n", __func__, count);
	return arena->push_array<descriptor_heap>(count);
}

func allocate_resources_and_views(memory_arena *arena, u32 max_resources_count) -> arena_array<resource_and_view> {
	printf("Allocate for %s count: %u...\n", __func__, max_resources_count);
	return arena->push_array<resource_and_view>(max_resources_count);
}

func allocate_rendering_stages(memory_arena *arena, u32 max_count) -> arena_array<rendering_stage> {
	printf("Allocate for %s count: %u...\n", __func__, max_count);
	return arena->push_array<rendering_stage>(max_count);
}


//NOTE(DH): Define min and max back like in windows.h
#if !defined(min)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#if !defined(max)
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif