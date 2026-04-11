#pragma once

#include <d3dx9math.h>

#include <d3d9.h>

/**
 * @brief Left-handed view and projection using D3DXMatrixLookAtLH / D3DXMatrixPerspectiveFovLH.
 *
 * Default placement: slightly above the XZ plane, looking toward the origin.
 */
class Camera {
public:
    Camera();

    /** Rebuild projection from viewport aspect ratio (width / height). */
    void SetAspect(float width, float height);

    /** Apply D3DTS_VIEW and D3DTS_PROJECTION on the fixed-function pipeline. */
    void ApplyViewProj(IDirect3DDevice9* device) const;

    /** Third-person camera behind vehicle heading. */
    void SetChase(const D3DXVECTOR3& target, float yaw, float distance, float height,
                  float look_ahead);

    const D3DXMATRIX& View() const { return view_; }
    const D3DXMATRIX& Projection() const { return proj_; }

private:
    void RebuildView();

    D3DXVECTOR3 eye_;
    D3DXVECTOR3 at_;
    D3DXVECTOR3 up_;

    float fov_y_;
    float z_near_;
    float z_far_;

    D3DXMATRIX view_;
    D3DXMATRIX proj_;
};
