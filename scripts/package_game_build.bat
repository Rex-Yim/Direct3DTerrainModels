@echo off
setlocal
set ROOT=%~dp0..
cd /d "%ROOT%"

set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if exist "%ROOT%\build\CMakeCache.txt" (
  echo Running cmake --install to "%ROOT%\Game_Build" ...
  %CMAKE% --install build --prefix "%ROOT%\Game_Build"
  if errorlevel 1 exit /b 1
  echo Done: %ROOT%\Game_Build
  exit /b 0
)

REM Fallback: manual copy (Visual Studio multi-config layout)
set SRC=%ROOT%\build\Release
if not exist "%SRC%\maeg4060_stage2.exe" set SRC=%ROOT%\build\RelWithDebInfo
if not exist "%SRC%\maeg4060_stage2.exe" (
  if exist "%ROOT%\build\maeg4060_stage2.exe" set SRC=%ROOT%\build
)
if not exist "%SRC%\maeg4060_stage2.exe" (
  echo No installable build found. Configure and build first, or ensure build\CMakeCache.txt exists.
  exit /b 1
)
set DST=%ROOT%\Game_Build
mkdir "%DST%" 2>nul
copy /Y "%SRC%\maeg4060_stage2.exe" "%DST%\"
if exist "%SRC%\D3DX9_43.dll" copy /Y "%SRC%\D3DX9_43.dll" "%DST%\"
if exist "%ROOT%\build\Microsoft.DXSDK.D3DX\build\native\release\bin\x64\D3DX9_43.dll" (
  copy /Y "%ROOT%\build\Microsoft.DXSDK.D3DX\build\native\release\bin\x64\D3DX9_43.dll" "%DST%\"
)
xcopy /E /I /Y "%ROOT%\Assets" "%DST%\Assets"
echo Done: %DST%
