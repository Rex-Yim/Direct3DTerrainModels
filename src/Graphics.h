#pragma once

#include <windows.h>

#include <d3d9.h>

/**
 * @brief DirectX 9 fixed-function setup: IDirect3D9 factory and IDirect3DDevice9.
 *
 * BeginFrame clears and starts the scene; draw from main (or other systems), then EndFrame
 * ends the scene and presents.
 */
class Graphics {
public:
    Graphics() = default;
    ~Graphics();

    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    bool Initialize(HWND hwnd, int width, int height);
    /** Resize swap chain / buffers without releasing the device (MANAGED resources stay valid). */
    bool Resize(int width, int height);
    void Shutdown();

    /** Clear + BeginScene. Pair every call with EndFrame(). */
    void BeginFrame();
    /** EndScene (if BeginScene succeeded) + Present. */
    void EndFrame();

    IDirect3D9* D3D() const { return d3d_; }
    IDirect3DDevice9* Device() const { return d3d_device_; }
    int ClientWidth() const { return width_; }
    int ClientHeight() const { return height_; }
    /** True if the last BeginFrame() opened a scene (BeginScene succeeded). */
    bool IsSceneActive() const { return scene_active_; }

private:
    bool CreatePresentParameters(D3DPRESENT_PARAMETERS* out_pp) const;

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    IDirect3D9* d3d_ = nullptr;
    IDirect3DDevice9* d3d_device_ = nullptr;
    bool scene_active_ = false;
};
