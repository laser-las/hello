@echo off
setlocal

where cl.exe >nul 2>nul
if %errorlevel%==0 goto msvc

echo cl.exe not found, falling back to CMake.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1
echo.
echo built build\halcyon.exe
exit /b 0

:msvc
cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /DUNICODE /D_UNICODE ^
   src\main.cpp src\common.cpp src\mem.cpp src\config.cpp src\game.cpp ^
   src\overlay.cpp src\aim.cpp src\esp.cpp ^
   /Fe:halcyon.exe /link user32.lib gdi32.lib gdiplus.lib winmm.lib
if errorlevel 1 exit /b 1
echo.
echo built halcyon.exe
exit /b 0
