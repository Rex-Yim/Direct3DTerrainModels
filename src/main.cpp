/**
 * @file main.cpp
 * @brief Win32 entry: window, message pump, fixed timestep, Game orchestration.
 */

#include <windows.h>

#include <algorithm>
#include <string>

#include <d3dx9.h>

#include "Camera.h"
#include "Game.h"
#include "Graphics.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"PhysicsSandboxWindow";

constexpr int kInitialClientWidth = 1280;
constexpr int kInitialClientHeight = 720;
constexpr float kMaxFps = 60.0f;
constexpr float kMinFrameTimeSec = 1.0f / kMaxFps;

Graphics* g_graphics = nullptr;
Game* g_game = nullptr;
bool g_game_device_needs_reset = false;

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
                if (g_game && !g_graphics->IsDeviceLost() && !g_game_device_needs_reset) {
                    g_game->OnLostDevice();
                    g_game_device_needs_reset = true;
                }
                const bool resized = g_graphics->Resize(client_w, client_h);
                if (resized && g_game && !g_graphics->IsDeviceLost()) {
                    g_game->OnResetDevice(g_graphics->Device());
                    g_game_device_needs_reset = false;
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
        MessageBoxW(hwnd,
                    L"Failed to create the Direct3D 9 device.\n\n"
                    L"Make sure the required DirectX runtime DLLs are next to the executable "
                    L"and that the app is running in a session with graphics acceleration.",
                    L"Direct3D initialization failed",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    Game game;
    g_game = &game;
    if (!game.Initialize(hwnd, graphics.Device())) {
        g_game = nullptr;
        g_graphics = nullptr;
        graphics.Shutdown();
        const std::wstring message = game.InitError().empty()
            ? std::wstring(L"Game initialization failed.")
            : std::wstring(L"Game initialization failed:\n\n") + game.InitError();
        MessageBoxW(hwnd, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
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
            if (graphics.IsDeviceLost()) {
                if (!g_game_device_needs_reset) {
                    game.OnLostDevice();
                    g_game_device_needs_reset = true;
                }
                if (!graphics.RecoverDevice()) {
                    Sleep(50);
                    continue;
                }
                game.OnResetDevice(graphics.Device());
                g_game_device_needs_reset = false;
            }

            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            float dt =
                static_cast<float>(now.QuadPart - prev.QuadPart) / static_cast<float>(freq.QuadPart);
            if (dt < kMinFrameTimeSec) {
                const float remaining_sec = kMinFrameTimeSec - dt;
                const DWORD sleep_ms = static_cast<DWORD>(remaining_sec * 1000.0f);
                if (sleep_ms > 0) {
                    Sleep(sleep_ms);
                }
                QueryPerformanceCounter(&now);
                dt = static_cast<float>(now.QuadPart - prev.QuadPart) /
                     static_cast<float>(freq.QuadPart);
            }
            prev = now;
            dt = (std::min)(dt, 0.05f);

            game.Update(dt, graphics);

            graphics.BeginFrame();
            if (graphics.IsDeviceLost()) {
                continue;
            }
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
