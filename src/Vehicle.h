#pragma once

#include <d3d9.h>
#include <d3dx9math.h>

class Terrain;

struct VehicleState {
    D3DXVECTOR3 position{0.f, 0.f, 0.f};
    float yaw = 0.f;
    float speed = 0.f;
};

/**
 * @brief Simple longitudinal dynamics + steering; pose aligned to terrain normal.
 */
class Vehicle {
public:
    void Reset(const D3DXVECTOR3& start_world, float start_yaw);

    /** After AABB resolution: move hull center in world space; keeps speed and yaw. */
    void SetFromHullCenter(const D3DXVECTOR3& hull_center, Terrain& terrain);

    void Update(float dt, float throttle, float steer, float brake, Terrain& terrain);

    const VehicleState& State() const { return s_; }

    void BuildWorldMatrix(Terrain& terrain, D3DXMATRIX* out_world) const;

    /** Rough axis-aligned hull in world space for collision (half-extents in local XZ, height). */
    void GetCollisionCenterExtents(D3DXVECTOR3* out_center, D3DXVECTOR3* out_half_extents) const;

private:
    VehicleState s_{};
};
