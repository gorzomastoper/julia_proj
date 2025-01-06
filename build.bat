@echo off
clang .\src\app.cpp -o .\bin\%1.exe -std=c++20 -l user32.lib -l gdi32 -l d3d12.lib -l dxguid.lib -l dxgi.lib -l d3dcompiler.lib -l dxguid.lib -l ./libs/imgui.lib -l ./libs/dx_backend.lib -I .\src\directx -I .\src\imgui-docking -I .\src\imgui\backends -D DEBUG -g -O0 
REM -fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-return=runtime