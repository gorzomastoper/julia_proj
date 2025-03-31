@echo off
IF NOT EXIST .\libs mkdir .\libs
clang .\src\imgui\imgui_libs.cpp -c -Wall -o .\libs\imgui.lib -std=c++20 -I .\src\imgui -D DEBUG -g -O0
clang .\src\dx_backend.cpp -c -Wall -o .\libs\dx_backend.lib -std=c++20 -I .\src\directx -I .\src\imgui -D DEBUG -g -O0
REM -fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-return=runtime