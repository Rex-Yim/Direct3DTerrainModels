#include "Graphics.h"

namespace {

bool IsDeviceResetError(HRESULT hr) {
    return hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET;
}

}  // namespace

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
    device_lost_ = false;

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
    if (device_lost_) {
        return true;
    }

    return ResetDevice();
}

bool Graphics::RecoverDevice() {
    if (!d3d_device_) {
        return false;
    }
    if (!device_lost_) {
        return true;
    }

    const HRESULT hr = d3d_device_->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) {
        return false;
    }
    if (hr == D3DERR_DEVICENOTRESET) {
        return ResetDevice();
    }
    if (SUCCEEDED(hr)) {
        device_lost_ = false;
        return true;
    }
    return false;
}

bool Graphics::ResetDevice() {
    D3DPRESENT_PARAMETERS pp = {};
    if (!CreatePresentParameters(&pp)) {
        return false;
    }

    const HRESULT hr = d3d_device_->Reset(&pp);
    if (SUCCEEDED(hr)) {
        device_lost_ = false;
        return true;
    }
    if (IsDeviceResetError(hr)) {
        MarkDeviceLost();
    }
    return false;
}

void Graphics::MarkDeviceLost() {
    scene_active_ = false;
    device_lost_ = true;
}

void Graphics::Shutdown() {
    scene_active_ = false;
    device_lost_ = false;
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
    if (!d3d_device_ || device_lost_) {
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
        if (IsDeviceResetError(hr)) {
            MarkDeviceLost();
        }
        return;
    }

    hr = d3d_device_->BeginScene();
    if (FAILED(hr)) {
        if (IsDeviceResetError(hr)) {
            MarkDeviceLost();
        }
        return;
    }
    scene_active_ = true;
}

void Graphics::EndFrame() {
    if (!d3d_device_ || device_lost_) {
        return;
    }
    if (scene_active_) {
        const HRESULT end_hr = d3d_device_->EndScene();
        scene_active_ = false;
        if (FAILED(end_hr)) {
            if (IsDeviceResetError(end_hr)) {
                MarkDeviceLost();
            }
            return;
        }
    }
    const HRESULT hr = d3d_device_->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(hr) && IsDeviceResetError(hr)) {
        MarkDeviceLost();
    }
}
