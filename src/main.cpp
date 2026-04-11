/**
 * @file main.cpp
 * @brief Win32 entry: window, message pump, fixed timestep, Game orchestration.
 */

#include <windows.h>

#include <algorithm>

#include <d3dx9.h>

#include "Camera.h"
#include "Game.h"
#include "Graphics.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"PhysicsSandboxWindow";

constexpr int kInitialClientWidth = 1280;
constexpr int kInitialClientHeight = 720;

Graphics* g_graphics = nullptr;
Game* g_game = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (g_graphics && wParam != SIZE_MINIMIZED) {
            const int client_w = LOWORD(lParam);
            const int client_h = HIWORD(lParam);
            if (client_w > 0 && client_h > 0) {
                IDirect3DDevice9* dev = g_graphics->Device();
                if (g_game && dev) {
                    g_game->OnLostDevice();
                }
                g_graphics->Resize(client_w, client_h);
                if (g_game && dev) {
                    g_game->OnResetDevice(dev);
                }
            }
        }
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

}  // namespace

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

    RECT window_rect = {0, 0, kInitialClientWidth, kInitialClientHeight};
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"MAEG4060 — Off-road terrain sandbox (DirectX 9)",
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

    Game game;
    g_game = &game;
    if (!game.Initialize(hwnd, graphics.Device())) {
        g_game = nullptr;
        g_graphics = nullptr;
        graphics.Shutdown();
        MessageBoxW(hwnd, L"Game initialization failed (assets or D3D).", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    Camera camera;

    LARGE_INTEGER freq{};
    LARGE_INTEGER prev{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            float dt = static_cast<float>(now.QuadPart - prev.QuadPart) / static_cast<float>(freq.QuadPart);
            prev = now;
            dt = (std::min)(dt, 0.05f);

            game.Update(dt, graphics);

            graphics.BeginFrame();
            if (graphics.IsSceneActive()) {
                game.Render(graphics.Device(), camera, graphics.ClientWidth(), graphics.ClientHeight(),
                            true);
            }
            graphics.EndFrame();
        }
    }

    g_game = nullptr;
    game.Shutdown();

    g_graphics = nullptr;
    graphics.Shutdown();
    return static_cast<int>(msg.wParam);
}
