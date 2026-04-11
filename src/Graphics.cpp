#include "Graphics.h"

Graphics::~Graphics() {
    Shutdown();
}

bool Graphics::CreatePresentParameters(D3DPRESENT_PARAMETERS* out_pp) const {
    if (!out_pp || !hwnd_) {
        return false;
    }
    D3DPRESENT_PARAMETERS& pp = *out_pp;
    pp = {};
    pp.BackBufferWidth = static_cast<UINT>(width_);
    pp.BackBufferHeight = static_cast<UINT>(height_);
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.MultiSampleQuality = 0;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd_;
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    return true;
}

bool Graphics::Initialize(HWND hwnd, int width, int height) {
    width_ = width;
    height_ = height;
    hwnd_ = hwnd;
    scene_active_ = false;

    Shutdown();

    d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d_) {
        return false;
    }

    D3DPRESENT_PARAMETERS pp = {};
    if (!CreatePresentParameters(&pp)) {
        return false;
    }

    const DWORD behavior = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    HRESULT hr = d3d_->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hwnd_,
        behavior,
        &pp,
        &d3d_device_);

    if (FAILED(hr)) {
        hr = d3d_->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            hwnd_,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp,
            &d3d_device_);
    }

    if (FAILED(hr) || !d3d_device_) {
        if (d3d_device_) {
            d3d_device_->Release();
            d3d_device_ = nullptr;
        }
        if (d3d_) {
            d3d_->Release();
            d3d_ = nullptr;
        }
        return false;
    }

    return true;
}

bool Graphics::Resize(int width, int height) {
    if (!d3d_device_ || !hwnd_) {
        return false;
    }
    width_ = width;
    height_ = height;
    scene_active_ = false;

    D3DPRESENT_PARAMETERS pp = {};
    if (!CreatePresentParameters(&pp)) {
        return false;
    }

    HRESULT hr = d3d_device_->Reset(&pp);
    return SUCCEEDED(hr);
}

void Graphics::Shutdown() {
    scene_active_ = false;
    if (d3d_device_) {
        d3d_device_->Release();
        d3d_device_ = nullptr;
    }
    if (d3d_) {
        d3d_->Release();
        d3d_ = nullptr;
    }
}

void Graphics::BeginFrame() {
    scene_active_ = false;
    if (!d3d_device_) {
        return;
    }

    HRESULT hr = d3d_device_->Clear(
        0,
        nullptr,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        clear_color_,
        1.0f,
        0);
    if (FAILED(hr)) {
        return;
    }

    scene_active_ = SUCCEEDED(d3d_device_->BeginScene());
}

void Graphics::EndFrame() {
    if (!d3d_device_) {
        return;
    }
    if (scene_active_) {
        d3d_device_->EndScene();
        scene_active_ = false;
    }
    d3d_device_->Present(nullptr, nullptr, nullptr, nullptr);
}
