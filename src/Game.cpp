#include "Game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Camera.h"
#include "Collision.h"
#include "Graphics.h"

namespace {

constexpr wchar_t kJeepPath[] = L"Assets/models/model_jeep.x";
constexpr wchar_t kCratePath[] = L"Assets/models/model_crate.x";
constexpr wchar_t kBoulderPath[] = L"Assets/models/model_boulder.x";
constexpr wchar_t kWindmillPath[] = L"Assets/models/model_windmill.x";

void LerpColor(DWORD* out, float t, DWORD a, DWORD b) {
    t = std::clamp(t, 0.f, 1.f);
    const int ar = (a >> 16) & 0xFF;
    const int ag = (a >> 8) & 0xFF;
    const int ab = a & 0xFF;
    const int br = (b >> 16) & 0xFF;
    const int bg = (b >> 8) & 0xFF;
    const int bb = b & 0xFF;
    const int r = static_cast<int>(static_cast<float>(ar) + (br - ar) * t);
    const int g = static_cast<int>(static_cast<float>(ag) + (bg - ag) * t);
    const int bl = static_cast<int>(static_cast<float>(ab) + (bb - ab) * t);
    *out = D3DCOLOR_XRGB(r, g, bl);
}

}  // namespace

bool Game::Initialize(HWND hwnd, IDirect3DDevice9* device) {
    Shutdown();
    hwnd_ = hwnd;
    if (!device) {
        return false;
    }

    if (!terrain_.Initialize(device)) {
        return false;
    }

    if (FAILED(jeep_.LoadMeshFromX(device, kJeepPath)) || !jeep_.IsLoaded()) {
        return false;
    }
    if (FAILED(crate_.LoadMeshFromX(device, kCratePath)) || !crate_.IsLoaded()) {
        return false;
    }
    if (FAILED(boulder_mesh_.LoadMeshFromX(device, kBoulderPath)) || !boulder_mesh_.IsLoaded()) {
        return false;
    }

    const HRESULT wh = windmill_.Load(device, kWindmillPath);
    windmill_loaded_ = SUCCEEDED(wh);

    if (FAILED(D3DXCreateFontW(device, 20, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI",
                               &font_))) {
        font_ = nullptr;
    }

    crate_pos_.y = terrain_.SampleHeight(crate_pos_.x, crate_pos_.z);
    windmill_pos_.y = terrain_.SampleHeight(windmill_pos_.x, windmill_pos_.z);
    boulder_pos_.x = 28.f;
    boulder_pos_.z = -22.f;
    boulder_pos_.y = terrain_.SampleHeight(boulder_pos_.x, boulder_pos_.z) + boulder_radius_;

    const D3DXVECTOR3 start(8.f, 0.f, -18.f);
    D3DXVECTOR3 vs = start;
    vs.y = terrain_.SampleHeight(vs.x, vs.z) + 1.1f;
    vehicle_.Reset(vs, 0.4f);
    boulder_vel_ = D3DXVECTOR3(0.f, 0.f, 0.f);
    sim_time_ = 0.f;
    wants_quit_ = false;
    return true;
}

void Game::Shutdown() {
    if (font_) {
        font_->Release();
        font_ = nullptr;
    }
    windmill_.Release();
    boulder_mesh_.Unload();
    crate_.Unload();
    jeep_.Unload();
    terrain_.Shutdown();
    hwnd_ = nullptr;
}

void Game::OnLostDevice() {
    if (font_) {
        font_->OnLostDevice();
    }
}

void Game::OnResetDevice(IDirect3DDevice9* device) {
    if (font_) {
        font_->OnResetDevice();
    }
    if (!font_ && device) {
        D3DXCreateFontW(device, 20, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI", &font_);
    }
}

void Game::Update(float dt, Graphics& graphics) {
    input_.Update();
    if (input_.State().quit) {
        wants_quit_ = true;
        PostQuitMessage(0);
    }

    sim_time_ += dt;
    const float day_t = std::fmod(sim_time_ * 0.12f, 6.2831853f);
    const float sun_h = std::sinf(day_t);
    const float sun_x = std::cosf(day_t * 0.7f);
    const float sun_z = std::sinf(day_t * 0.7f);
    D3DXVECTOR3 dir(sun_x, -0.55f - sun_h * 0.85f, sun_z);
    D3DXVec3Normalize(&sun_dir_, &dir);

    const float blend = std::clamp(0.5f + 0.5f * sun_h, 0.f, 1.f);
    LerpColor(&clear_color_, 1.f - blend, D3DCOLOR_XRGB(12, 18, 42), D3DCOLOR_XRGB(135, 190, 235));
    graphics.SetClearColor(clear_color_);

    const float amb_day = 0.26f;
    const float amb_night = 0.08f;
    const float amb = amb_night + (amb_day - amb_night) * blend;
    ambient_ = D3DCOLOR_COLORVALUE(amb * 0.9f, amb, amb * 1.05f, 1.f);

    vehicle_.Update(dt, input_.State().throttle, input_.State().steer, input_.State().brake,
                    terrain_);

    constexpr float kGravity = 28.f;
    boulder_vel_.y -= kGravity * dt;
    boulder_pos_ += boulder_vel_ * dt;
    const float ground =
        terrain_.SampleHeight(boulder_pos_.x, boulder_pos_.z) + boulder_radius_;
    if (boulder_pos_.y < ground) {
        boulder_pos_.y = ground;
        boulder_vel_.y *= -0.25f;
    }
    boulder_pos_.x = std::clamp(boulder_pos_.x, -terrain_.HalfWidth() + 3.f,
                                terrain_.HalfWidth() - 3.f);
    boulder_pos_.z = std::clamp(boulder_pos_.z, -terrain_.HalfDepth() + 3.f,
                                terrain_.HalfDepth() - 3.f);

    ResolveCollisions();

    if (windmill_loaded_) {
        windmill_.AdvanceTime(dt);
    }
}

void Game::ResolveCollisions() {
    D3DXVECTOR3 jc;
    D3DXVECTOR3 je;
    vehicle_.GetCollisionCenterExtents(&jc, &je);
    Aabb jeep_aabb{jc, je};

    Aabb crate_aabb;
    crate_aabb.center = crate_pos_ + D3DXVECTOR3(0.f, 1.2f, 0.f);
    crate_aabb.half = D3DXVECTOR3(2.2f, 2.2f, 2.2f);

    if (AabbVsAabbResolveFirstAxis(crate_aabb, &jeep_aabb)) {
        vehicle_.SetFromHullCenter(jeep_aabb.center, terrain_);
    }

    Aabb jeep_push = jeep_aabb;
    vehicle_.GetCollisionCenterExtents(&jeep_push.center, &jeep_push.half);
    Sphere boulder{boulder_pos_, boulder_radius_};
    if (SphereVsAabbPushSphere(jeep_push, &boulder)) {
        boulder_pos_ = boulder.center;
        D3DXVECTOR3 rel = boulder.center - jeep_push.center;
        rel.y = 0.f;
        if (D3DXVec3LengthSq(&rel) > 1e-4f) {
            D3DXVec3Normalize(&rel, &rel);
            boulder_vel_ += rel * 3.f;
        }
    }

    if (SphereVsAabbPushSphere(crate_aabb, &boulder)) {
        boulder_pos_ = boulder.center;
    }

    D3DXVECTOR3 to_b = boulder_pos_ - jeep_aabb.center;
    to_b.y = 0.f;
    const float dist = D3DXVec3Length(&to_b);
    const float min_d = boulder_radius_ + 2.5f;
    if (dist < min_d && dist > 1e-4f) {
        D3DXVec3Normalize(&to_b, &to_b);
        const D3DXVECTOR3 push = to_b * (min_d - dist);
        VehicleState st = vehicle_.State();
        D3DXVECTOR3 hc;
        D3DXVECTOR3 he;
        vehicle_.GetCollisionCenterExtents(&hc, &he);
        hc.x += push.x;
        hc.z += push.z;
        vehicle_.SetFromHullCenter(hc, terrain_);
    }
}

void Game::ApplyLighting(IDirect3DDevice9* device) {
    if (!device) {
        return;
    }
    device->SetRenderState(D3DRS_LIGHTING, TRUE);
    device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    device->SetRenderState(D3DRS_AMBIENT, ambient_);

    D3DLIGHT9 light = {};
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse.r = light.Diffuse.g = light.Diffuse.b = 1.f;
    light.Diffuse.a = 1.f;
    light.Direction.x = sun_dir_.x;
    light.Direction.y = sun_dir_.y;
    light.Direction.z = sun_dir_.z;
    D3DXVec3Normalize(reinterpret_cast<D3DXVECTOR3*>(&light.Direction),
                      reinterpret_cast<const D3DXVECTOR3*>(&light.Direction));

    device->SetLight(0, &light);
    device->LightEnable(0, TRUE);
}

void Game::DrawHud(IDirect3DDevice9* device, int client_w, int client_h) {
    if (!font_ || !device) {
        return;
    }
    const VehicleState& vs = vehicle_.State();
    wchar_t line[512];
    swprintf_s(line, L"MAEG4060 sandbox | WASD Space Esc | speed %.1f | pos (%.1f, %.1f, %.1f)",
               vs.speed, vs.position.x, vs.position.y, vs.position.z);
    RECT r{8, 8, client_w - 8, 80};
    font_->DrawTextW(nullptr, line, -1, &r, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 255, 255, 255));
    swprintf_s(line, L"sim t=%.1fs | windmill=%s", sim_time_, windmill_loaded_ ? L"on" : L"off");
    RECT r2{8, 36, client_w - 8, 120};
    font_->DrawTextW(nullptr, line, -1, &r2, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 220, 240, 255));
}

void Game::Render(IDirect3DDevice9* device, Camera& camera, int client_w, int client_h,
                  bool scene_active) {
    if (!device || !scene_active) {
        return;
    }

    camera.SetAspect(static_cast<float>(client_w), static_cast<float>(client_h));
    const VehicleState& vs = vehicle_.State();
    camera.SetChase(vs.position, vs.yaw, 32.f, 11.f, 6.f);
    camera.ApplyViewProj(device);

    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

    ApplyLighting(device);

    D3DXMATRIX id;
    D3DXMatrixIdentity(&id);
    device->SetTransform(D3DTS_WORLD, &id);
    terrain_.Draw(device);

    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    if (windmill_loaded_) {
        D3DXMATRIX wm;
        D3DXMatrixTranslation(&wm, windmill_pos_.x, windmill_pos_.y, windmill_pos_.z);
        windmill_.Draw(device, &wm);
    }

    D3DXMATRIX wc;
    D3DXMatrixTranslation(&wc, crate_pos_.x, crate_pos_.y, crate_pos_.z);
    device->SetTransform(D3DTS_WORLD, &wc);
    crate_.Draw(device);

    D3DXMATRIX wb;
    const float s = (boulder_radius_ / 2.2f);
    D3DXMATRIX sc;
    D3DXMatrixScaling(&sc, s, s, s);
    D3DXMATRIX tr;
    D3DXMatrixTranslation(&tr, boulder_pos_.x, boulder_pos_.y - boulder_radius_ * 0.2f,
                          boulder_pos_.z);
    D3DXMatrixMultiply(&wb, &sc, &tr);
    device->SetTransform(D3DTS_WORLD, &wb);
    boulder_mesh_.Draw(device);

    D3DXMATRIX wj;
    vehicle_.BuildWorldMatrix(terrain_, &wj);
    device->SetTransform(D3DTS_WORLD, &wj);
    jeep_.Draw(device);

    device->SetTransform(D3DTS_WORLD, &id);
    DrawHud(device, client_w, client_h);
}
