#include "d3dx12_root_signature.h"
#include "dxgiformat.h"
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
T* create_command_list(dx_context *ctx, D3D12_COMMAND_LIST_TYPE cmdListType, ID3D12PipelineState *pInitialState, bool close)
{
	T* cmdList;
	ThrowIfFailed(ctx->g_device->CreateCommandList(0, cmdListType, ctx->g_command_allocators[ctx->g_frame_index], pInitialState, IID_PPV_ARGS(&cmdList)));

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
	auto render_pass_01 = context->mem_arena.load_by_idx<render_pass>				(context->compute_stage.render_passes.ptr, 0);
	auto pipeline 		= context->mem_arena.get_ptr								(render_pass_01.curr_pipeline_c);

	pipeline->resize(context, &context->compute_stage_heap, &context->mem_arena, context->resources_and_views, width, height, pipeline->bindings);

	// mat4 model_matrix = translation_matrix(V3(-1.0, -1.0, 0.0));
	mat4 model_matrix = translation_matrix(V3(0.0, 0.0, 0.0));
	mat4 view_matrix = look_at_matrix(V3(0.0f, 0.0f, 1.0f), V3(0, 0, 0), V3(0, 1, 0));
	context->common_cbuff_data.MVP_matrix = create_ortho_matrix(5.0f, context->aspect_ratio, 1.0f, -1.0, -1.0, 1.0f, 0.0f, 0.0f) * view_matrix * model_matrix;
	// context->common_cbuff_data.MVP_matrix = create_ortho_matrix(1.0f, 0.0f, g_ClientWidth, g_ClientHeight, 0.0f, 0.0f, 0.0f) * view_matrix * model_matrix;

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
#if DEBUG
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ID3DBlob *result;
	ID3DBlob *error;
	//ThrowIfFailed(D3DCompileFromFile(filename_path, pDefines, pInclude, pEntrypoint, pTarget, flags1, flags2, &result, &error));
	auto hr = D3DCompileFromFile(filename_path, pDefines, pInclude, pEntrypoint, pTarget, compileFlags, flags2, &result, &error);
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

// {
// 	// Create and record the bundle.
// 	{
// 		result.bundle = create_command_list<ID3D12GraphicsCommandList> (
// 			context.g_device,
// 			context.g_bundle_allocators[context.g_frame_index],
// 			D3D12_COMMAND_LIST_TYPE_BUNDLE,
// 			context.g_pipeline_state,
// 			false
// 		);
		
// 		result.bundle->SetGraphicsRootSignature(context.g_root_signature);
// 		result.bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
// 		result.bundle->IASetVertexBuffers(0, 1, &result.vertex_buffer_view);
// 		result.bundle->IASetIndexBuffer(&result.index_buffer_view);
// 		result.bundle->DrawIndexedInstanced(circle_indices.size(), 1, 0, 0, 0);
// 		ThrowIfFailed(result.bundle->Close());
// 	}
// }

dx_context init_dx(HWND hwnd)
{
	dx_context result = {};
	result.g_hwnd = hwnd;
	printf("Initializing DX12...\n");

	printf("Initialize memory...\n");
	result.mem_arena = initialize_arena(Megabytes(128));

	printf("Allocate resources from mem arena...\n");
	// TODO(DH): When nodes will be ready to use, rework this to be dynamic
	// This will create resource for translator, it will be created later
	// Here is the max resource count for all stages and pipelines
	result.resources_and_views = allocate_resources_and_views(&result.mem_arena, 2048);

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

	result.common_cbuff_data.MVP_matrix = create_ortho_matrix(1.0f, 1.0f, 0, g_ClientWidth, g_ClientHeight, 0, 0.1f, 1000.0f);

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

	result.g_command_list = create_command_list<ID3D12GraphicsCommandList>(&result, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	result.g_imgui_command_list = create_command_list<ID3D12GraphicsCommandList>(&result, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);
	result.g_compute_command_list = create_command_list<ID3D12GraphicsCommandList>(&result, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, true);

	//Create fence objects
	result.g_fence = create_fence(result.g_device);
	result.g_fence_event = create_fence_event_handle();

	// 1.) Create graphics rendering stage
	{
		
	}

	// 2.) Create compute rendering stage
	{
		result.compute_stage_heap = allocate_descriptor_heap(result.g_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 32);

		WCHAR filename[] = L"example_03.hlsl";
		ID3DBlob* shader_for_pass_01 = compile_shader(result.g_device, filename, "Splat", "cs_5_0");
		ID3DBlob* shader_for_pass_02 = compile_shader(result.g_device, filename, "CSMain", "cs_5_0");

		render_target2d render_target = render_target2d::create (result.g_device, result.mem_arena, &result.compute_stage_heap, &result.resources_and_views, result.viewport.Width, result.viewport.Height, 0);
		buffer_1d	hist_buffer = buffer_1d::create (result.g_device, result.mem_arena, &result.compute_stage_heap, &result.resources_and_views, result.viewport.Width * result.viewport.Height, sizeof(u32), nullptr, 1);
		buffer_cbuf	cbuff = buffer_cbuf::create (result.g_device, result.mem_arena, &result.compute_stage_heap, &result.resources_and_views, (u8*)&result.common_cbuff_data, 0);

		auto binds = mk_bindings()
			.bind_buffer<true, false, true>(result.mem_arena.push_data(render_target))
			.bind_buffer<true, false, false>(result.mem_arena.push_data(hist_buffer))
			.bind_buffer<false,true, false>(result.mem_arena.push_data(cbuff));

		auto binds_ar_ptr = result.mem_arena.push_data(binds);
		auto binds_ptr = *(arena_ptr<void>*)(&binds_ar_ptr);

		auto resize = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, u32 width, u32 height, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = ctx->mem_arena.get_ptr(bnds_ptr);
			Resize<decltype(binds)::BUF_TS_U>::resize(bnds->data, ctx->g_device, ctx->mem_arena, heap, &resources_and_views, width, height); 
		};

		auto generate_binding_table = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = ctx->mem_arena.get_ptr(bnds_ptr);
			GCBC<decltype(binds)::BUF_TS_U>::bind_root_sig_table(bnds->data, 0, cmd_list, ctx->g_device, heap->addr, &ctx->mem_arena);
		};

		auto update = [](dx_context *ctx, descriptor_heap* heap, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = ctx->mem_arena.get_ptr(bnds_ptr);
			Update<decltype(binds)::BUF_TS_U>::update(bnds->data, ctx, &ctx->mem_arena, resources_and_views, cmd_list);
		};

		auto copy_to_render_target = [](dx_context *ctx, memory_arena *arena, arena_array<resource_and_view> resources_and_views, ID3D12GraphicsCommandList* cmd_list, arena_ptr<void> bindings) {
			arena_ptr<decltype(binds)> bnds_ptr = *(arena_ptr<decltype(binds)>*)&bindings;
			auto bnds = ctx->mem_arena.get_ptr(bnds_ptr);
			CTRT<decltype(binds)::BUF_TS_U>::copy_to_render_target(bnds->data, ctx, &ctx->mem_arena, resources_and_views, cmd_list);
		};

		compute_pipeline compute_pipeline_pass_01 = 
		compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target)
			.bind_shader		(shader_for_pass_01)
			.create_root_sig	(binds, result.g_device, &result.mem_arena)
			.finalize			(binds, &result, &result.mem_arena, result.resources_and_views, result.g_device, &result.compute_stage_heap);

		compute_pipeline compute_pipeline_pass_02 = 
		compute_pipeline::init__(binds_ptr, resize, generate_binding_table, update, copy_to_render_target)
			.bind_shader		(shader_for_pass_02)
			.create_root_sig	(binds, result.g_device, &result.mem_arena)
			.finalize			(binds, &result, &result.mem_arena, result.resources_and_views, result.g_device, &result.compute_stage_heap);

		result.compute_stage = 
		rendering_stage::init__	(&result.mem_arena, 2)
			.bind_compute_pass	(compute_pipeline_pass_01, &result.mem_arena)
			.bind_compute_pass	(compute_pipeline_pass_02, &result.mem_arena);
	}

	//All should be initialised, show the window
	result.g_is_initialized = true;

	return result;
}

void update(dx_context *ctx)
{
	ctx->time_elapsed += ctx->dt_for_frame * ctx->speed_multiplier;
	ctx->time_elapsed = fmod(ctx->time_elapsed, TIME_MAX);

	ctx->common_cbuff_data.offset.z = ctx->viewport.Width;
	ctx->common_cbuff_data.offset.w = ctx->viewport.Height;

	float speed = 0.005f;
	float offset_bounds = 1.25f;

	ctx->common_cbuff_data.offset.y += speed;
	if(ctx->common_cbuff_data.offset.y > offset_bounds)
		ctx->common_cbuff_data.offset.y = -offset_bounds;

	ctx->common_cbuff_data.offset.x = ctx->time_elapsed;
	//ctx->common_cbuff_data.mouse_pos = V4(ImGui::GetMousePos().x, ImGui::GetMousePos().y, 1.0, 1.0);

	render_pass			pass 		= ctx->mem_arena.load_by_idx<render_pass>(ctx->compute_stage.render_passes.ptr, 0);
	compute_pipeline 	pipeline 	= ctx->mem_arena.load(pass.curr_pipeline_c);

	pipeline.update(ctx, &ctx->compute_stage_heap, &ctx->mem_arena, ctx->resources_and_views, ctx->g_compute_command_list, pipeline.bindings);
}

void recompile_shader(dx_context *ctx, rendering_stage rndr_stage)
{
	rendering_stage stage = ctx->compute_stage; // NOTE(DH): This needs to be compute stage :)
	// ctx->compute_stage.rndr_pass_01.prev_pipeline = rndr_stage.rndr_pass_01.curr_pipeline;
	// ctx->compute_stage.rndr_pass_01.curr_pipeline = create_compute_pipeline(ctx->g_device, "Splat",&ctx->mem_arena, ctx->compute_stage.rndr_pass_01.curr_pipeline.root_signature);

	// ctx->compute_stage.rndr_pass_02.prev_pipeline = rndr_stage.rndr_pass_02.curr_pipeline;
	// ctx->compute_stage.rndr_pass_02.curr_pipeline = create_compute_pipeline(ctx->g_device, "CSMain",&ctx->mem_arena, ctx->compute_stage.rndr_pass_02.curr_pipeline.root_signature);
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

void set_render_target
(
	ID3D12GraphicsCommandList* cmd_list,
	ID3D12DescriptorHeap* dsc_heap,
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

func start_imgui_frame () -> void {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

func finish_imgui_frame () -> void {
	ImGui::Render();
	ImGui::UpdatePlatformWindows();
}

ID3D12GraphicsCommandList* generate_imgui_command_buffer(dx_context *context)
{
	//NOTE(DH): Here is the IMGUI commands generated

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

CD3DX12_CPU_DESCRIPTOR_HANDLE create_uav_buffer_descriptor_view(ID3D12Device2* device, u32 index, ID3D12DescriptorHeap *desc_heap, u32 desc_size, u32 num_of_elements, u32 size_of_one_elem, DXGI_FORMAT format, D3D12_UAV_DIMENSION dim, ID3D12Resource *p_resource)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE view = get_cpu_handle_at(index, desc_heap, desc_size);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
	uav_desc.Format = format;
	uav_desc.ViewDimension = dim;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = num_of_elements;
	uav_desc.Buffer.StructureByteStride = size_of_one_elem;
	uav_desc.Buffer.CounterOffsetInBytes = 0;
	uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(p_resource, nullptr, &uav_desc, view);
	return view;
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

ID3D12GraphicsCommandList* generate_compute_command_buffer(dx_context *ctx, u32 width, u32 height)
{
	auto cmd_allocator = get_command_allocator(ctx->g_frame_index, ctx->g_command_allocators);
	auto srv_dsc_heap = ctx->compute_stage_heap;

	rendering_stage 	stage 		= ctx->compute_stage;
	render_pass 		rndr_pass_1	= ctx->mem_arena.load_by_idx(stage.render_passes.ptr, 0);
	render_pass 		rndr_pass_2	= ctx->mem_arena.load_by_idx(stage.render_passes.ptr, 1);
	compute_pipeline	c_p_1 		= ctx->mem_arena.load(rndr_pass_1.curr_pipeline_c);
	compute_pipeline	c_p_2 		= ctx->mem_arena.load(rndr_pass_2.curr_pipeline_c);

	//NOTE(DH): Reset command allocators for the next frame
	record_reset_cmd_allocator(cmd_allocator);

	//NOTE(DH): Reset command lists for the next frame
	record_reset_cmd_list(ctx->g_compute_command_list, cmd_allocator, c_p_1.state);
	ctx->g_compute_command_list->SetName(L"COMPUTE COMMAND LIST");

	// NOTE(DH): Set root signature and pipeline state
	ctx->g_compute_command_list->SetComputeRootSignature(c_p_1.root_signature);
	ctx->g_compute_command_list->SetPipelineState(c_p_1.state);

	ID3D12DescriptorHeap* ppHeaps[] = { srv_dsc_heap.addr};
	record_dsc_heap(ctx->g_compute_command_list, ppHeaps, _countof(ppHeaps));

	c_p_1.generate_binding_table(ctx, &srv_dsc_heap, &ctx->mem_arena, ctx->g_compute_command_list, c_p_1.bindings);
	ctx->g_compute_command_list->Dispatch(32,32,12);

	ctx->g_compute_command_list->SetPipelineState(c_p_2.state);

	u32 dispatch_X = (width / 8) + 1;
	u32 dispatch_y = (height / 8) + 1;

	ctx->g_compute_command_list->Dispatch(dispatch_X, dispatch_y, 1);

	// 4.) Copy result from back buffer to screen
	c_p_1.copy_to_screen_rt(ctx, &ctx->mem_arena, ctx->resources_and_views, ctx->g_compute_command_list, c_p_1.bindings);

	//Finally executing the filled Command List into the Command Queue
	return ctx->g_compute_command_list;
}

// NOTE(DH): Compute pipeline }

// NOTE(DH): Here we allocate all our descriptor heaps for different pipelines
func allocate_descriptor_heaps(memory_arena *arena, u32 count) -> arena_array<descriptor_heap> {
	printf("Allocate for %s count: %u...\n", __func__, count);
	return arena->alloc_array<descriptor_heap>(count);
}

func allocate_resources_and_views(memory_arena *arena, u32 max_resources_count) -> arena_array<resource_and_view> {
	printf("Allocate for %s count: %u...\n", __func__, max_resources_count);
	return arena->alloc_array<resource_and_view>(max_resources_count);
}

func allocate_rendering_stages(memory_arena *arena, u32 max_count) -> arena_array<rendering_stage> {
	printf("Allocate for %s count: %u...\n", __func__, max_count);
	return arena->alloc_array<rendering_stage>(max_count);
}

func record_copy_buffer (ID3D12Resource *p_dst, ID3D12Resource *p_src, ID3D12GraphicsCommandList *command_list, u32 size) -> void {
	auto addr4 = CD3DX12_RESOURCE_BARRIER::Transition(p_dst, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER/* | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
	command_list->CopyBufferRegion(p_dst, 0, p_src, 0, size);
	command_list->ResourceBarrier(1, &addr4);
}

//NOTE(DH): Define min and max back like in windows.h
#if !defined(min)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#if !defined(max)
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif