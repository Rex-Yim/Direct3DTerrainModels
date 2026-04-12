cmake_minimum_required(VERSION 3.20)
project(maeg4060_stage2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Legacy June 2010 SDK (DXSDK_DIR), or Microsoft.DXSDK.D3DX NuGet headers/libs (no global installer).
set(_d3dx_nuget_ver "9.29.952.8")
set(_d3dx_nuget_root "${CMAKE_BINARY_DIR}/Microsoft.DXSDK.D3DX")
if(DEFINED ENV{DXSDK_DIR})
    set(D3DX_INCLUDE_DIR "$ENV{DXSDK_DIR}/Include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(D3DX_LIB_DIR "$ENV{DXSDK_DIR}/Lib/x64")
    else()
        set(D3DX_LIB_DIR "$ENV{DXSDK_DIR}/Lib/x86")
    endif()
    set(D3DX_INSTALL_DLL "")
    message(STATUS "Using DirectX SDK (June 2010) from DXSDK_DIR: $ENV{DXSDK_DIR}")
else()
    set(_d3dx_stamp "${_d3dx_nuget_root}/.extracted_${_d3dx_nuget_ver}")
    if(NOT EXISTS "${_d3dx_stamp}")
        set(_nupkg "${CMAKE_BINARY_DIR}/Microsoft.DXSDK.D3DX.${_d3dx_nuget_ver}.nupkg")
        message(STATUS "DXSDK_DIR not set; downloading Microsoft.DXSDK.D3DX ${_d3dx_nuget_ver}...")
        file(
            DOWNLOAD "https://www.nuget.org/api/v2/package/Microsoft.DXSDK.D3DX/${_d3dx_nuget_ver}"
            "${_nupkg}"
            SHOW_PROGRESS
            TLS_VERIFY ON)
        file(MAKE_DIRECTORY "${_d3dx_nuget_root}")
        file(ARCHIVE_EXTRACT INPUT "${_nupkg}" DESTINATION "${_d3dx_nuget_root}")
        file(WRITE "${_d3dx_stamp}" "")
    endif()
    set(D3DX_INCLUDE_DIR "${_d3dx_nuget_root}/build/native/include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(D3DX_LIB_DIR "${_d3dx_nuget_root}/build/native/release/lib/x64")
        set(D3DX_INSTALL_DLL "${_d3dx_nuget_root}/build/native/release/bin/x64/D3DX9_43.dll")
    else()
        set(D3DX_LIB_DIR "${_d3dx_nuget_root}/build/native/release/lib/x86")
        set(D3DX_INSTALL_DLL "${_d3dx_nuget_root}/build/native/release/bin/x86/D3DX9_43.dll")
    endif()
    message(STATUS "Using NuGet D3DX include: ${D3DX_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${D3DX_INCLUDE_DIR}/d3dx9.h")
    message(FATAL_ERROR
        "D3DX headers not found at '${D3DX_INCLUDE_DIR}/d3dx9.h'. "
        "If DXSDK_DIR is set, it must be the June 2010 DirectX SDK root (containing Include). "
        "Otherwise CMake downloads Microsoft.DXSDK.D3DX into the build tree; ensure configure "
        "finished (needs network on first run), or delete the build directory and reconfigure.")
endif()

add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/AnimatedModel.cpp
    src/AnimatedModel.h
    src/Camera.cpp
    src/Camera.h
    src/Collision.cpp
    src/Collision.h
    src/Game.cpp
    src/Game.h
    src/Graphics.cpp
    src/Graphics.h
    src/Input.cpp
    src/Input.h
    src/ModelLoader.cpp
    src/ModelLoader.h
    src/Terrain.cpp
    src/Terrain.h
    src/Vehicle.cpp
    src/Vehicle.h
)

target_include_directories(${PROJECT_NAME} PRIVATE "${D3DX_INCLUDE_DIR}")
target_link_directories(${PROJECT_NAME} PRIVATE "${D3DX_LIB_DIR}")

target_link_libraries(${PROJECT_NAME} PRIVATE d3d9 d3dx9 xinput9_1_0)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
)

if(D3DX_INSTALL_DLL)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${D3DX_INSTALL_DLL}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

# Step 0: Win32 only (no D3D) — useful for verifying toolchain and message loop.
add_executable(empty_window WIN32 src/empty_window_main.cpp)
set_target_properties(empty_window PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
if(MSVC)
    target_compile_options(empty_window PRIVATE /W4)
endif()
target_compile_definitions(empty_window PRIVATE UNICODE _UNICODE)

# Step 1: Win32 + IDirect3DDevice9 (reuses Graphics for CreateDevice / Present).
add_executable(d3d_device WIN32
    src/d3d_device_main.cpp
    src/Graphics.cpp
    src/Graphics.h
)
target_link_libraries(d3d_device PRIVATE d3d9)
set_target_properties(d3d_device PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
if(MSVC)
    target_compile_options(d3d_device PRIVATE /W4)
endif()
target_compile_definitions(d3d_device PRIVATE UNICODE _UNICODE)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Assets" DESTINATION .)
if(D3DX_INSTALL_DLL)
    install(FILES "${D3DX_INSTALL_DLL}" DESTINATION .)
endif()
