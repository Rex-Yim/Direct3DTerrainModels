#include "Vehicle.h"

#include "Terrain.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kMaxSpeed = 42.f;
constexpr float kAccel = 28.f;
constexpr float kBrakeDecel = 55.f;
constexpr float kRolling = 6.f;
constexpr float kMaxSteerRate = 1.35f;

}  // namespace

void Vehicle::Reset(const D3DXVECTOR3& start_world, float start_yaw) {
    s_.position = start_world;
    s_.yaw = start_yaw;
    s_.speed = 0.f;
}

void Vehicle::SetFromHullCenter(const D3DXVECTOR3& hull_center, Terrain& terrain) {
    s_.position = hull_center;
    s_.position.y -= 0.35f;
    s_.position.y = terrain.SampleHeight(s_.position.x, s_.position.z) + 1.1f;
}

void Vehicle::Update(float dt, float throttle, float steer, float brake, Terrain& terrain) {
    dt = std::clamp(dt, 0.f, 0.1f);

    const float forward_force = throttle * kAccel;
    const float brake_force = brake * kBrakeDecel;
    const float roll = std::copysign(kRolling, s_.speed);

    float dv = forward_force * dt;
    if (brake_force > 0.01f) {
        const float dir = (std::fabs(s_.speed) < 0.01f) ? throttle : std::copysign(1.f, s_.speed);
        dv -= dir * brake_force * dt;
    }
    dv -= roll * dt;

    s_.speed += dv;
    if (std::fabs(s_.speed) < 0.02f && std::fabs(throttle) < 0.05f && brake < 0.05f) {
        s_.speed = 0.f;
    }
    s_.speed = std::clamp(s_.speed, -kMaxSpeed * 0.35f, kMaxSpeed);

    const float speed_factor = std::clamp(std::fabs(s_.speed) / kMaxSpeed, 0.f, 1.f);
    s_.yaw += steer * kMaxSteerRate * (0.35f + 0.65f * speed_factor) * dt;

    D3DXVECTOR3 fwd(std::sinf(s_.yaw), 0.f, std::cosf(s_.yaw));
    s_.position += fwd * (s_.speed * dt);

    // Prevent leaving the heightfield (avoids driving into the void beyond the terrain edge).
    constexpr float kEdgePad = 0.5f;
    s_.position.x = std::clamp(s_.position.x, -terrain.HalfWidth() + kEdgePad,
                               terrain.HalfWidth() - kEdgePad);
    s_.position.z = std::clamp(s_.position.z, -terrain.HalfDepth() + kEdgePad,
                               terrain.HalfDepth() - kEdgePad);

    const float ground = terrain.SampleHeight(s_.position.x, s_.position.z);
    s_.position.y = ground + 1.1f;
}

void Vehicle::BuildWorldMatrix(Terrain& terrain, D3DXMATRIX* out_world) const {
    D3DXVECTOR3 n;
    terrain.SampleNormal(s_.position.x, s_.position.z, &n);

    // Model forward is opposite of our simulation forward; flip to match chase camera / movement.
    D3DXVECTOR3 fwd(-std::sinf(s_.yaw), 0.f, -std::cosf(s_.yaw));
    D3DXVECTOR3 right;
    D3DXVec3Cross(&right, &fwd, &n);
    if (D3DXVec3LengthSq(&right) < 1e-8f) {
        right = D3DXVECTOR3(1.f, 0.f, 0.f);
    }
    D3DXVec3Normalize(&right, &right);

    D3DXVECTOR3 fwd2;
    D3DXVec3Cross(&fwd2, &n, &right);
    D3DXVec3Normalize(&fwd2, &fwd2);

    D3DXMATRIX rot;
    rot._11 = right.x;
    rot._12 = right.y;
    rot._13 = right.z;
    rot._14 = 0.f;
    rot._21 = n.x;
    rot._22 = n.y;
    rot._23 = n.z;
    rot._24 = 0.f;
    rot._31 = fwd2.x;
    rot._32 = fwd2.y;
    rot._33 = fwd2.z;
    rot._34 = 0.f;
    rot._41 = 0.f;
    rot._42 = 0.f;
    rot._43 = 0.f;
    rot._44 = 1.f;

    D3DXMATRIX tr;
    D3DXMatrixTranslation(&tr, s_.position.x, s_.position.y, s_.position.z);
    D3DXMatrixMultiply(out_world, &rot, &tr);
}

void Vehicle::GetCollisionCenterExtents(D3DXVECTOR3* out_center, D3DXVECTOR3* out_half_extents) const {
    *out_center = s_.position;
    out_center->y += 0.35f;
    *out_half_extents = D3DXVECTOR3(1.2f, 0.85f, 2.4f);
}
