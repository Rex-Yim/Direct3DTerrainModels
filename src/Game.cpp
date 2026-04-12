#include "Game.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "Camera.h"
#include "Collision.h"
#include "Graphics.h"

namespace {

constexpr wchar_t kJeepPath[] = L"Assets/models/model_jeep.x";
constexpr wchar_t kCratePath[] = L"Assets/models/model_crate.x";
constexpr wchar_t kBoulderPath[] = L"Assets/models/model_boulder.x";
constexpr wchar_t kWindmillPath[] = L"Assets/models/model_windmill.x";
constexpr wchar_t kJeepObjPathA[] = L"Assets/models/replacements/model_jeep/model_jeep.obj";
constexpr wchar_t kJeepObjPathB[] = L"Assets/models/replacements/model_jeep.obj";
constexpr wchar_t kJeepObjPathC[] = L"Assets/models/model_jeep.obj";
constexpr wchar_t kCrateObjPathA[] = L"Assets/models/replacements/model_crate/model_crate.obj";
constexpr wchar_t kCrateObjPathB[] = L"Assets/models/replacements/model_crate.obj";
constexpr wchar_t kCrateObjPathC[] = L"Assets/models/model_crate.obj";
constexpr wchar_t kBoulderObjPathA[] = L"Assets/models/replacements/model_boulder/model_boulder.obj";
constexpr wchar_t kBoulderObjPathB[] = L"Assets/models/replacements/model_boulder.obj";
constexpr wchar_t kBoulderObjPathC[] = L"Assets/models/model_boulder.obj";

std::filesystem::path ModuleDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (len >= buffer.size()) {
        buffer.resize(buffer.size() * 2, L'\0');
        len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(len);
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path ResolveAssetPath(const wchar_t* relative_path) {
    if (!relative_path || !relative_path[0]) {
        return {};
    }

    const std::filesystem::path rel(relative_path);
    const std::filesystem::path module_dir = ModuleDirectory();
    const std::filesystem::path cwd = std::filesystem::current_path();
    const std::filesystem::path candidates[] = {
        cwd / rel,
        module_dir / rel,
        module_dir.parent_path() / rel,
        module_dir.parent_path().parent_path() / rel,
    };

    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

std::wstring HrHex(HRESULT hr) {
    wchar_t buffer[32];
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(hr));
    return buffer;
}

struct StaticModelLoadResult {
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    std::filesystem::path resolved_path;
    std::wstring attempts_log;
};

template <size_t N>
StaticModelLoadResult LoadStaticModelCandidates(IDirect3DDevice9* device,
                                                ModelLoader* loader,
                                                const std::array<const wchar_t*, N>& candidates) {
    StaticModelLoadResult result;
    if (!device || !loader) {
        result.hr = E_INVALIDARG;
        result.attempts_log = L"<invalid args>";
        return result;
    }

    for (const wchar_t* candidate : candidates) {
        if (!candidate || !candidate[0]) {
            continue;
        }
        result.attempts_log += candidate;
        result.attempts_log += L" => ";

        const std::filesystem::path resolved = ResolveAssetPath(candidate);
        if (resolved.empty()) {
            result.attempts_log += L"not found\n";
            result.hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            continue;
        }

        const HRESULT hr = loader->LoadFromFile(device, resolved.c_str());
        result.hr = hr;
        if (SUCCEEDED(hr) && loader->IsLoaded()) {
            result.resolved_path = resolved;
            result.attempts_log += L"ok\n";
            return result;
        }

        result.attempts_log += L"failed (";
        result.attempts_log += HrHex(hr);
        result.attempts_log += L")\n";
    }

    return result;
}

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
    init_error_.clear();
    if (!device) {
        init_error_ = L"Direct3D device was not created.";
        return false;
    }

    if (!terrain_.Initialize(device)) {
        init_error_ = L"Failed to create terrain resources.";
        return false;
    }

    const auto jeep_candidates =
        std::array<const wchar_t*, 4>{kJeepObjPathA, kJeepObjPathB, kJeepObjPathC, kJeepPath};
    const StaticModelLoadResult jeep_load = LoadStaticModelCandidates(device, &jeep_, jeep_candidates);
    if (FAILED(jeep_load.hr) || !jeep_.IsLoaded()) {
        const HRESULT fallback_hr = jeep_.CreateBox(device, 4.6f, 1.8f, 2.4f);
        if (FAILED(fallback_hr) || !jeep_.IsLoaded()) {
            init_error_ = L"Jeep model failed.\nResolved path: ";
            init_error_ += jeep_load.resolved_path.empty() ? std::wstring(L"<none>") : jeep_load.resolved_path.native();
            init_error_ += L"\nLoad hr: ";
            init_error_ += HrHex(jeep_load.hr);
            init_error_ += L"\nFallback hr: ";
            init_error_ += HrHex(fallback_hr);
            init_error_ += L"\nAttempts:\n";
            init_error_ += jeep_load.attempts_log;
            return false;
        }
    }

    const auto crate_candidates =
        std::array<const wchar_t*, 4>{kCrateObjPathA, kCrateObjPathB, kCrateObjPathC, kCratePath};
    const StaticModelLoadResult crate_load = LoadStaticModelCandidates(device, &crate_, crate_candidates);
    if (FAILED(crate_load.hr) || !crate_.IsLoaded()) {
        const HRESULT fallback_hr = crate_.CreateBox(device, 4.4f, 4.4f, 4.4f);
        if (FAILED(fallback_hr) || !crate_.IsLoaded()) {
            init_error_ = L"Crate model failed.\nResolved path: ";
            init_error_ += crate_load.resolved_path.empty() ? std::wstring(L"<none>") : crate_load.resolved_path.native();
            init_error_ += L"\nLoad hr: ";
            init_error_ += HrHex(crate_load.hr);
            init_error_ += L"\nFallback hr: ";
            init_error_ += HrHex(fallback_hr);
            init_error_ += L"\nAttempts:\n";
            init_error_ += crate_load.attempts_log;
            return false;
        }
    }

    const auto boulder_candidates = std::array<const wchar_t*, 4>{kBoulderObjPathA, kBoulderObjPathB,
                                                                   kBoulderObjPathC, kBoulderPath};
    const StaticModelLoadResult boulder_load =
        LoadStaticModelCandidates(device, &boulder_mesh_, boulder_candidates);
    if (FAILED(boulder_load.hr) || !boulder_mesh_.IsLoaded()) {
        const HRESULT fallback_hr = boulder_mesh_.CreateSphere(device, boulder_radius_, 20, 14);
        if (FAILED(fallback_hr) || !boulder_mesh_.IsLoaded()) {
            init_error_ = L"Boulder model failed.\nResolved path: ";
            init_error_ += boulder_load.resolved_path.empty() ? std::wstring(L"<none>") : boulder_load.resolved_path.native();
            init_error_ += L"\nLoad hr: ";
            init_error_ += HrHex(boulder_load.hr);
            init_error_ += L"\nFallback hr: ";
            init_error_ += HrHex(fallback_hr);
            init_error_ += L"\nAttempts:\n";
            init_error_ += boulder_load.attempts_log;
            return false;
        }
    }

    const std::filesystem::path windmill_path = ResolveAssetPath(kWindmillPath);
    const HRESULT wh = windmill_path.empty() ? E_FAIL : windmill_.Load(device, windmill_path.c_str());
    windmill_loaded_ = !windmill_path.empty() && SUCCEEDED(wh);
    windmill_has_animation_ = windmill_loaded_ && windmill_.HasAnimation();
    windmill_anim_paused_ = false;

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
    windmill_loaded_ = false;
    windmill_has_animation_ = false;
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
    if (input_.State().windmill_toggle) {
        windmill_anim_paused_ = !windmill_anim_paused_;
    }
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

    if (windmill_loaded_ && windmill_has_animation_ && !windmill_anim_paused_) {
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
    const int bottom_pad = (std::max)(12, client_h - 8);
    const VehicleState& vs = vehicle_.State();
    wchar_t line[512];
    swprintf_s(line, L"MAEG4060 sandbox | WASD Space Esc M | speed %.1f | pos (%.1f, %.1f, %.1f)",
               vs.speed, vs.position.x, vs.position.y, vs.position.z);
    RECT r{8, 8, client_w - 8, (std::min)(80, bottom_pad)};
    font_->DrawTextW(nullptr, line, -1, &r, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 255, 255, 255));
    const wchar_t* windmill_state = L"off";
    if (windmill_loaded_) {
        if (windmill_has_animation_) {
            windmill_state = windmill_anim_paused_ ? L"paused" : L"anim";
        } else {
            windmill_state = L"static";
        }
    }
    swprintf_s(line, L"sim t=%.1fs | windmill=%s", sim_time_, windmill_state);
    RECT r2{8, 36, client_w - 8, (std::min)(120, bottom_pad)};
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
