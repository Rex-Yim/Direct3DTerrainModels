/**
 * @file main.cpp
 * @brief Application entry: Win32 window creation and message pump.
 *
 * Rendering uses DirectX 9 with the fixed-function pipeline (see Graphics.cpp).
 * Math and transforms elsewhere should use left-handed conventions (e.g. D3DX*LH).
 */

#include <windows.h>

#include <d3dx9.h>

#include "Camera.h"
#include "Graphics.h"
#include "ModelLoader.h"

namespace {

// Registered window class name (Unicode build: wchar_t strings).
constexpr wchar_t kWindowClassName[] = L"PhysicsSandboxWindow";

constexpr int kInitialClientWidth = 1280;
constexpr int kInitialClientHeight = 720;

// Repository layout: mesh lives under Assets/models/. (If you prefer Assets/model_jeep.x, copy or symlink there.)
constexpr wchar_t kJeepModelPath[] = L"Assets/models/model_jeep.x";

// Graphics is resized from WM_SIZE; we keep a pointer for the window procedure.
Graphics* g_graphics = nullptr;

/**
 * @brief One directional light + modest ambient so FFP-lit meshes are visible.
 */
void ApplyDirectionalLight(IDirect3DDevice9* device) {
    if (!device) {
        return;
    }

    device->SetRenderState(D3DRS_LIGHTING, TRUE);
    device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_COLORVALUE(0.22f, 0.24f, 0.28f, 1.f));

    D3DLIGHT9 light = {};
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse.r = light.Diffuse.g = light.Diffuse.b = 1.f;
    light.Diffuse.a = 1.f;
    // Direction (world space) in which light propagates — slightly from above and to the side.
    light.Direction.x = -0.35f;
    light.Direction.y = -0.85f;
    light.Direction.z = 0.25f;
    D3DXVec3Normalize(reinterpret_cast<D3DXVECTOR3*>(&light.Direction),
                      reinterpret_cast<const D3DXVECTOR3*>(&light.Direction));

    device->SetLight(0, &light);
    device->LightEnable(0, TRUE);
}

/**
 * Standard Win32 window procedure: handles lifetime, sizing, and default behavior.
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        // Signal WM_QUIT so the message loop exits.
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        // Resize buffers in place so MANAGED .x meshes from D3DXLoadMeshFromX stay valid.
        if (g_graphics && wParam != SIZE_MINIMIZED) {
            const int client_w = LOWORD(lParam);
            const int client_h = HIWORD(lParam);
            if (client_w > 0 && client_h > 0) {
                g_graphics->Resize(client_w, client_h);
            }
        }
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

}  // namespace

/**
 * Windows subsystem entry (Unicode). Creates the main window and runs the message loop.
 *
 * Idle time (no pending messages) is used to advance the simulation / present frames.
 */
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*prev_instance*/, PWSTR /*cmd_line*/,
                    int show_command) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    // Client size -> required window rectangle including non-client chrome.
    RECT window_rect = {0, 0, kInitialClientWidth, kInitialClientHeight};
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"3D Physics Sandbox — DirectX 9 (Fixed Function)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd) {
        return 1;
    }

    Graphics graphics;
    g_graphics = &graphics;
    if (!graphics.Initialize(hwnd, kInitialClientWidth, kInitialClientHeight)) {
        g_graphics = nullptr;
        return 1;
    }

    Camera camera;
    ModelLoader jeep;
    if (FAILED(jeep.LoadMeshFromX(graphics.Device(), kJeepModelPath)) || !jeep.IsLoaded()) {
        g_graphics = nullptr;
        graphics.Shutdown();
        MessageBoxW(hwnd, L"Failed to load jeep .x mesh. Check working directory and path.", L"Model load error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    // Message pump: process Windows messages; when the queue is empty, run one frame.
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            IDirect3DDevice9* device = graphics.Device();
            graphics.BeginFrame();

            if (graphics.IsSceneActive()) {
                camera.SetAspect(static_cast<float>(graphics.ClientWidth()),
                                   static_cast<float>(graphics.ClientHeight()));
                camera.ApplyViewProj(device);

                D3DXMATRIX world;
                D3DXMatrixIdentity(&world);
                device->SetTransform(D3DTS_WORLD, &world);

                device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
                device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

                ApplyDirectionalLight(device);
                jeep.Draw(device);
            }

            graphics.EndFrame();
        }
    }

    g_graphics = nullptr;
    graphics.Shutdown();
    return static_cast<int>(msg.wParam);
}
