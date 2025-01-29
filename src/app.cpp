#ifndef UNICODE
#define UNICODE
#include <combaseapi.h>
#include <memoryapi.h>
#include <propsys.h>
#include <synchapi.h>
#include <vcruntime_string.h>
#include <winbase.h>
#include <windef.h>
#include <winuser.h>
#endif

#include <stdio.h>
#include <cstdint>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "util/types.h"
#include "util/memory_management.h"
#include "dx_backend.h"

//NOTE(DH): ImGUI implementation import {
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui-docking/imgui.h"
#include "imgui-docking/backends/imgui_impl_dx12.h"
#include "imgui-docking/backends/imgui_impl_win32.h"
//NOTE(DH): ImGUI implementation import }

#if DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#if 1
#define USE_DX12
#endif

#ifdef VIRT_ALLOCATION
void* allocate_memory(void* base, size_t size) { return VirtualAlloc(base, size, MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);}
#else
void* allocate_memory(void* base, size_t size) { return malloc(size);}
#endif

//TODO(DH): This is temporary stuff, vaporize immidietly when normal memory arena will be created
// Simple free list based allocator
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap*       Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    ImVector<int>               FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        IM_ASSERT(Heap == nullptr && FreeIndices.empty());
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices.push_back(n);
    }
    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        IM_ASSERT(FreeIndices.Size > 0);
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
    {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(cpu_idx == gpu_idx);
        FreeIndices.push_back(cpu_idx);
    }
} g_pd3dSrvDescHeapAlloc;

static dx_context directx_context;

struct {
    u32 width;
    u32 height;

    i32 mouse_x;
    i32 mouse_y;
    f32 mouse_wheel_delta;

    bool lmb_down;
} main_window_info = {0};

//NOTE(DH): This is needed for fullscreen functionality
static WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};

func toggle_fullscreen(HWND Window)
{
    // NOTE(Denis): Follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353

    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            SetWindowLong(Window, GWL_STYLE,Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

bool quiting = false;
global_variable bool global_pause = false;
global_variable f64 GlobalPerfCountFrequency;
global_variable f32 last_time;

inline LARGE_INTEGER
win32_get_wall_clock()
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline f32
win32_get_seconds_elapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	f32 Result = ((f32)(End.QuadPart - Start.QuadPart) / 
					 (f32)GlobalPerfCountFrequency);
	
	return(Result);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK main_window_callback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
 	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

#ifdef USE_DX12
	if(directx_context.g_is_initialized)
#endif
	{
		switch (uMsg) {
			case WM_QUIT: {
				quiting = true;
				PostQuitMessage(0);
			} return 0;
			case WM_DESTROY: {
				quiting = true;
				PostQuitMessage(0);
			} return 0;

#ifdef USE_DX12
			case WM_PAINT: {
					// if(!global_pause) 
					{
						f32 current_time = GetCurrentTime();
						f32 delta_time = current_time - last_time;
						last_time = current_time;

						directx_context.dt_for_frame = delta_time * 0.001;
						update(&directx_context);

						// auto triangle_cmd_list = generate_command_buffer(&directx_context);
						auto compute_cmd_list = generate_compute_command_buffer(&directx_context);
						auto imgui_cmd_list = generate_imgui_command_buffer(&directx_context);

						float clear_color[] = { 0.0f, 0.2f, 0.4f, 1.0f };
						// clear_render_target(directx_context, compute_cmd_list, clear_color);
						render(&directx_context, compute_cmd_list);
						// render(&directx_context, triangle_cmd_list);
						render(&directx_context, imgui_cmd_list);

						// Do not wait for frame, just move to the next one
						auto render_target = get_current_swapchain(&directx_context);
						present(&directx_context, render_target);
						move_to_next_frame(&directx_context);

						// Recompile shaders if needed
						if(directx_context.g_recompile_shader)
						{
							rendering_stage compute_stage = directx_context.dx_memory_arena.load_by_idx<rendering_stage>(directx_context.rendering_stages.ptr, 0);
							recompile_shader(&directx_context, compute_stage);
							directx_context.g_recompile_shader = false;
						}
					}
					quiting = directx_context.g_is_quitting;
				} return 0;
#endif

			case WM_SIZE: {
				RECT ClientRect;
				GetClientRect(hwnd, &ClientRect);
				main_window_info.height = (uint16_t)(ClientRect.bottom - ClientRect.top);
				main_window_info.width = (uint16_t)(ClientRect.right - ClientRect.left);
				main_window_info.mouse_wheel_delta = 0;
#ifdef USE_DX12
				if(wParam == SIZE_MINIMIZED) {
					global_pause = true;
				} else if (wParam == SIZE_RESTORED){
					global_pause = false;
					resize(&directx_context, main_window_info.width, main_window_info.height);
				}
#else
				do_redraw(hwnd);
#endif
			}; break;
			case WM_KEYDOWN: {
				u32 key_code = (u32)wParam;
				switch (key_code) {
					case 'Q': {quiting = true; PostQuitMessage(0);} break;
					case 'F': {toggle_fullscreen(directx_context.g_hwnd);} break;
					// case VK_SPACE: 
					// {
					// 	WINDOWPLACEMENT placement;
					// 	GetWindowPlacement(hwnd, &placement);
					// 	placement.rcNormalPosition.;
					// } break;
					default: {
						bool AltKeyWasDown = ((lParam & (1 << 29)) != 0);
						if((key_code == VK_F4) && AltKeyWasDown) { quiting = true; }
						if((key_code == VK_RETURN) && AltKeyWasDown) { toggle_fullscreen(hwnd); }
					}
				}
			} break;
			// case WM_MOUSEMOVE: {}; break;
			case WM_MOUSEWHEEL: {
				main_window_info.mouse_wheel_delta = GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f;// / (120.0f * 500.0f);
				printf("Mouse wheel delta: %f\n", main_window_info.mouse_wheel_delta);
			} break;
		}
		// printf("window size = %u, %u\n", main_window_info.height, main_window_info.width);
	}

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// #include "ui_buffer_backend.cpp"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

	// NOTE(DH): For dt time and taget frame rate things!
	LARGE_INTEGER perf_count_frequency_result;
	QueryPerformanceFrequency(&perf_count_frequency_result);
	GlobalPerfCountFrequency = perf_count_frequency_result.QuadPart;

#ifdef USE_DX12
#if DEBUG //SHOW_CONSOLE
	//Always enable the debug layer before doing anything DX12 related
	//so all possible errors generated while creating DX12 objects
	//are caught by the debug layter.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));

        debugInterface->EnableDebugLayer();

#endif // _DEBUG
#endif // _USE_DX12

    #if DEBUG //SHOW_CONSOLE
        AllocConsole();
        FILE *fpstdin = stdin, *fpstdout = stdout, *fpstderr = stderr;

        freopen_s(&fpstdin, "CONIN$", "r", stdin);
        freopen_s(&fpstdout, "CONOUT$", "w", stdout);
        freopen_s(&fpstderr, "CONOUT$", "w", stderr);
    #endif

    // ui_button("hello")(mk_empty_ui_context());
    // asdd("helll2");
    // let of1 = bk_state.components_data.write<u64>(11);
    // let of2 = bk_state.components_data.write<u32>(22);
    // let of3 = bk_state.components_data.write<u64>(33);
    // let of4 = bk_state.components_data.write<u32>(44);
    // let of5 = bk_state.components_data.write<u32>(55);
    // let of6 = bk_state.components_data.write<u8>(66);
    // let of7 = bk_state.components_data.write<u64>(77);

    // printf("of1 = %llu\n", *bk_state.components_data.get_ptr<u64>(of1));
    // printf("of2 = %u\n", *bk_state.components_data.get_ptr<u32>(of2));
    // printf("of3 = %llu\n", *bk_state.components_data.get_ptr<u64>(of3));
    // printf("of4 = %u\n", *bk_state.components_data.get_ptr<u32>(of4));
    // printf("of5 = %u\n", *bk_state.components_data.get_ptr<u32>(of5));
    // printf("of6 = %u\n", *bk_state.components_data.get_ptr<u8>(of6));
    // printf("of7 = %llu\n", *bk_state.components_data.get_ptr<u64>(of7));

	//NOTE(DH): Enumerate audio devices - START
	LPWSTR device_id_name;
	IMMDeviceCollection *audio_collection;
	IMMDevice *devices[10];
	IPropertyStore *p_props = NULL;
	auto node = CoInitialize(NULL);
	IMMDeviceEnumerator *pEnumerator = NULL;
	node = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
	node = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &audio_collection);

	u32 num_devices = 0;
	audio_collection->GetCount(&num_devices);

	for(u32 i = 0; i < num_devices; ++i)
	{
		audio_collection->Item(i, &devices[i]);
		devices[i]->GetId(&device_id_name);
		devices[i]->OpenPropertyStore(STGM_READ, &p_props);

		PROPVARIANT prop_name;
		PropVariantInit(&prop_name);
		node = p_props->GetValue(PKEY_Device_FriendlyName, &prop_name);
	}
	//NOTE(DH): Enumerate audio devices - END

    const wchar_t CLASS_NAME[]  = L"Sample Window Class";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = main_window_callback;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    // wc.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.style = CS_OWNDC;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    // wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Monica",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

	directx_context.g_hwnd = hwnd;

	//NOTE(DH): Full directx initialization
	directx_context = init_dx(directx_context.g_hwnd);

	//NOTE(DH): Initialize IMGUI {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// NOTE(DH): This is example how to load font and get cyrillic glyphs
	//io.Fonts->AddFontFromFileTTF(".\\..\\data\\fonts\\PTSans-Regular.ttf", 15.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());

	ImGui::StyleColorsLight();
	//ImGui::StyleColorsDark();
	//ImGui::GetStyle().ScaleAllSizes(2.0f);

	ImGui_ImplWin32_Init(hwnd);

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = directx_context.g_device;
	init_info.CommandQueue = directx_context.g_command_queue;
	init_info.NumFramesInFlight = g_NumFrames;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	init_info.SrvDescriptorHeap = directx_context.g_imgui_descriptor_heap;

	g_pd3dSrvDescHeapAlloc.Create(init_info.Device, init_info.SrvDescriptorHeap);

	init_info.SrvDescriptorAllocFn = 
		[](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { printf("ALLOC"); return g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
	init_info.SrvDescriptorFreeFn = 
		[](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { printf("FREE"); return g_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };

	ImGui_ImplDX12_Init(&init_info);
	
	//NOTE(DH): Initialize IMGUI }

    ShowWindow(hwnd, nCmdShow);

    POINT mpos = {};
    GetCursorPos(&mpos);
    ScreenToClient(hwnd,&mpos);
    main_window_info.mouse_x = mpos.x;
    main_window_info.mouse_y = mpos.y;

	LARGE_INTEGER last_counter = win32_get_wall_clock();

	// NOTE(DH): Get and set window capability
	i32 monitor_refresh_hz = 60;
	HDC RefreshDC = GetDC(hwnd);

#if 1
			int win32_refresh_rate = GetDeviceCaps(RefreshDC,VREFRESH);
#else
			int win32_refresh_rate = monitor_refresh_hz;
#endif
	ReleaseDC(hwnd, RefreshDC);
	if(win32_refresh_rate > 1)
	{
		monitor_refresh_hz = win32_refresh_rate;
	}
	f32 game_update_hz = (f32)monitor_refresh_hz;
	f32 target_seconds_per_frame = 1.0f / (f32)game_update_hz;
	last_time = GetCurrentTime(); // TODO(DH): Implement it like in DENGINE!!!

   // while (quiting == false) {
        // POINT mpos = {};
        // GetCursorPos(&mpos);
        // ScreenToClient(hwnd,&mpos);
        // main_window_info.mouse_x = mpos.x;
        // main_window_info.mouse_y = mpos.y;

		// NOTE(DH): Set fixed delta time
		directx_context.dt_for_frame = target_seconds_per_frame;

        MSG msg = {};

#ifdef USE_DX12
		while (msg.message != WM_QUIT && quiting == false) {
			if(PeekMessage(&msg,0,0,0,PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
#else
        while (PeekMessage(&msg,0,0,0,PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif


#ifdef USE_DX12

#else
   		if (let res = generate_one(map, map_size_x, map_size_y, ng_list, free_ng_ls, best_ng_idx)) {
            std::tie(ng_list, free_ng_ls, best_ng_idx) = *res;
        }
        map_to_img(map, map_img_buf, map_size_x, map_size_y);

        do_redraw(hwnd);
#endif

    //}

	//Make sure command queue has finished all in-flight commands before closing
	flush(directx_context);

	VirtualFree(directx_context.dx_memory_arena.base, directx_context.dx_memory_arena.size, MEM_RELEASE);

	//Closing event handle
	CloseHandle(directx_context.g_fence_event);

    return 0;
}