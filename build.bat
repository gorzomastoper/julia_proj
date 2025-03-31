@echo off
IF NOT EXIST .\bin mkdir .\bin
clang .\src\app.cpp -o .\bin\%1.exe -std=c++20 -l C:\VulkanSDK\1.4.309.0\Lib\vulkan-1.lib -l uuid.lib -l user32.lib -l gdi32 -l d3d12.lib -l dxguid.lib -l dxgi.lib -l d3dcompiler.lib -l dxguid.lib -l Winmm.lib -l ./libs/imgui.lib -l ./libs/dx_backend.lib -l ./libs/vk_backend.lib -I .\src\directx -I .\src\imgui-docking -I .\src\imgui\backends -I C:\VulkanSDK\1.4.309.0\include -D %2 -D VIRT_ALLOCATION -g -O0 -mavx
REM -fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-return=runtime