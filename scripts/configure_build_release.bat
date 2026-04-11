@echo off
setlocal
set ROOT=%~dp0..
cd /d "%ROOT%"

REM If you previously configured with another CMake generator, delete the whole "build"
REM folder (or remove CMakeCache.txt and CMakeFiles) before running this script.
REM First configure without DXSDK_DIR may take several minutes (NuGet D3DX download + extract).
REM If MSVC reports C1060 (compiler out of heap), retry with fewer jobs, e.g.:
REM   cmake --build build -j 1

if not defined DXSDK_DIR (
  if exist "C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)" (
    set "DXSDK_DIR=C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)"
  )
)

call "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

%CMAKE% -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl || exit /b 1
%CMAKE% --build build || exit /b 1

echo Build OK: %ROOT%\build\maeg4060_stage2.exe
echo Next: scripts\package_game_build.bat
