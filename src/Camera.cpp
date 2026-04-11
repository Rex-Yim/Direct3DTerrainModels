#include "Camera.h"

#include <d3dx9.h>

Camera::Camera()
    : eye_(0.f, 18.f, -42.f)
    , at_(0.f, 0.f, 0.f)
    , up_(0.f, 1.f, 0.f)
    , fov_y_(D3DX_PI / 4.f)
    , z_near_(1.f)
    , z_far_(5000.f) {
    RebuildView();
    SetAspect(16.f, 9.f);
}

void Camera::RebuildView() {
    D3DXMatrixLookAtLH(&view_, &eye_, &at_, &up_);
}

void Camera::SetAspect(float width, float height) {
    const float aspect = (height > 0.f) ? (width / height) : (16.f / 9.f);
    D3DXMatrixPerspectiveFovLH(&proj_, fov_y_, aspect, z_near_, z_far_);
}

void Camera::ApplyViewProj(IDirect3DDevice9* device) const {
    if (!device) {
        return;
    }
    device->SetTransform(D3DTS_VIEW, &view_);
    device->SetTransform(D3DTS_PROJECTION, &proj_);
}
