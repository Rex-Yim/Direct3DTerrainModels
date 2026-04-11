/**
 * @file d3d_device_main.cpp
 * @brief Window + IDirect3DDevice9: initialize Graphics, handle resize, idle Present loop.
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "Graphics.h"

namespace {

constexpr wchar_t kClassName[] = L"Maeg4060D3dDevice";

constexpr int kClientWidth = 1280;
constexpr int kClientHeight = 720;

Graphics* g_graphics = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        if (g_graphics && wParam != SIZE_MINIMIZED) {
            const int cw = LOWORD(lParam);
            const int ch = HIWORD(lParam);
            if (cw > 0 && ch > 0) {
                g_graphics->Resize(cw, ch);
            }
        }
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*prev*/, PWSTR /*cmd_line*/, int show) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    RECT r = {0, 0, kClientWidth, kClientHeight};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"MAEG4060 — D3D9 device (clear only)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        r.right - r.left,
        r.bottom - r.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd) {
        return 1;
    }

    Graphics graphics;
    g_graphics = &graphics;
    if (!graphics.Initialize(hwnd, kClientWidth, kClientHeight)) {
        g_graphics = nullptr;
        MessageBoxW(hwnd, L"Direct3DCreate9 or CreateDevice failed.", L"D3D9", MB_OK | MB_ICONERROR);
        return 1;
    }

    graphics.SetClearColor(D3DCOLOR_XRGB(45, 52, 64));

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg = {};
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_graphics = nullptr;
                graphics.Shutdown();
                return static_cast<int>(msg.wParam);
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        graphics.BeginFrame();
        if (graphics.IsSceneActive()) {
            // No draw calls yet — back buffer is only the clear color.
        }
        graphics.EndFrame();
    }
}
