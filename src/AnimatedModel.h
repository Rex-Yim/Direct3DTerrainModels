#pragma once

#include <d3d9.h>
#include <d3dx9.h>

/**
 * @brief Loads a skinned / hierarchical .x scene with optional keyframe animation.
 */
class HierarchyModel {
public:
    HierarchyModel() = default;
    ~HierarchyModel();

    HierarchyModel(const HierarchyModel&) = delete;
    HierarchyModel& operator=(const HierarchyModel&) = delete;

    HRESULT Load(IDirect3DDevice9* device, const wchar_t* x_path);
    void Release();

    void AdvanceTime(float dt_seconds);
    void Draw(IDirect3DDevice9* device, const D3DXMATRIX* root_world);

    bool IsLoaded() const { return root_frame_ != nullptr; }
    bool HasAnimation() const { return has_animation_; }

private:
    IDirect3DDevice9* device_ = nullptr;
    LPD3DXFRAME root_frame_ = nullptr;
    LPD3DXANIMATIONCONTROLLER anim_controller_ = nullptr;
    bool has_animation_ = false;
};
