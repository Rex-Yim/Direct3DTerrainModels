#pragma once

#include <windows.h>

#include <string>

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9math.h>

#include "AnimatedModel.h"
#include "Input.h"
#include "ModelLoader.h"
#include "Terrain.h"
#include "Vehicle.h"

class Camera;
class Graphics;  // Update() only needs declaration

/**
 * @brief Simulation + rendering: terrain, vehicle, props, windmill, day/night, HUD.
 */
class Game {
public:
    bool Initialize(HWND hwnd, IDirect3DDevice9* device);
    void Shutdown();

    void OnLostDevice();
    void OnResetDevice(IDirect3DDevice9* device);

    void Update(float dt, Graphics& graphics);
    void Render(IDirect3DDevice9* device, Camera& camera, int client_w, int client_h,
                bool scene_active);

    const std::wstring& InitError() const { return init_error_; }
    bool WantsQuit() const { return wants_quit_; }

private:
    void ApplyLighting(IDirect3DDevice9* device);
    void DrawHud(IDirect3DDevice9* device, int client_w, int client_h);
    void DrawMiniMap(IDirect3DDevice9* device, int client_w, int client_h);
    void ResolveCollisions();

    HWND hwnd_ = nullptr;
    bool wants_quit_ = false;

    Input input_;
    Terrain terrain_;
    Vehicle vehicle_;
    ModelLoader jeep_;
    D3DXMATRIX jeep_local_correction_{};
    ModelLoader crate_;
    ModelLoader boulder_mesh_;
    D3DXMATRIX boulder_local_correction_{};
    HierarchyModel windmill_;
    /** Used when D3DXLoadMeshHierarchyFromX fails but the same asset loads as a plain mesh. */
    ModelLoader windmill_static_;
    D3DXMATRIX windmill_local_correction_{};
    ID3DXMesh* windmill_static_base_mesh_ = nullptr;
    ID3DXMesh* windmill_static_blade_mesh_ = nullptr;
    D3DXVECTOR3 windmill_blade_pivot_local_{0.f, 0.f, 0.f};
    bool windmill_loaded_ = false;
    bool windmill_is_static_mesh_ = false;
    bool windmill_has_animation_ = false;

    /** Near vehicle spawn (~8, -18) so props are visible without a long drive. */
    D3DXVECTOR3 crate_pos_{12.f, 0.f, -8.f};
    D3DXVECTOR3 crate_vel_{0.f, 0.f, 0.f};
    D3DXVECTOR3 boulder_pos_{22.f, 0.f, -26.f};
    D3DXVECTOR3 boulder_vel_{0.f, 0.f, 0.f};
    float boulder_radius_ = 2.2f;

    D3DXVECTOR3 windmill_pos_{72.f, 0.f, -58.f};

    double sim_time_ = 0.0;
    /** Exponential moving average of frame rate for HUD (updated in Update). */
    float fps_smoothed_ = 0.f;
    D3DCOLOR clear_color_ = D3DCOLOR_XRGB(135, 206, 235);
    D3DXVECTOR3 sun_dir_{-0.35f, -0.85f, 0.25f};
    DWORD ambient_ = D3DCOLOR_COLORVALUE(0.22f, 0.24f, 0.28f, 1.f);

    ID3DXFont* font_ = nullptr;
    std::wstring init_error_;
};
