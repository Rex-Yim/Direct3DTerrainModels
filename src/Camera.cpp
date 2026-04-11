#include "Camera.h"

#include <cmath>

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

void Camera::SetChase(const D3DXVECTOR3& target, float yaw, float distance, float height,
                      float look_ahead) {
    const D3DXVECTOR3 fwd(std::sinf(yaw), 0.f, std::cosf(yaw));
    D3DXVECTOR3 back = fwd * (-distance);
    eye_ = target + back + D3DXVECTOR3(0.f, height, 0.f);
    at_ = target + fwd * look_ahead + D3DXVECTOR3(0.f, height * 0.25f, 0.f);
    RebuildView();
}
