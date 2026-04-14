#include "Game.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstdint>

#include "Camera.h"
#include "Collision.h"
#include "Graphics.h"

namespace {

constexpr wchar_t kWindmillPath[] = L"Assets/models/model_windmill.x";
constexpr wchar_t kJeepObjPathA[] = L"Assets/models/replacements/model_jeep/model_jeep.obj";
constexpr wchar_t kJeepObjPathB[] = L"Assets/models/replacements/model_jeep.obj";
constexpr wchar_t kJeepObjPathC[] = L"Assets/models/model_jeep.obj";
constexpr wchar_t kJeepXPath[] = L"Assets/models/model_jeep.x";
constexpr wchar_t kCrateObjPathA[] = L"Assets/models/replacements/model_crate/model_crate.obj";
constexpr wchar_t kCrateObjPathB[] = L"Assets/models/replacements/model_crate.obj";
constexpr wchar_t kCrateObjPathC[] = L"Assets/models/model_crate.obj";
constexpr wchar_t kBoulderObjPathA[] = L"Assets/models/replacements/model_boulder/model_boulder.obj";
constexpr wchar_t kBoulderObjPathB[] = L"Assets/models/replacements/model_boulder.obj";
constexpr wchar_t kBoulderObjPathC[] = L"Assets/models/model_boulder.obj";
constexpr wchar_t kWindmillObjPathA[] = L"Assets/models/replacements/model_windmill/model_windmill.obj";
constexpr wchar_t kWindmillObjPathB[] = L"Assets/models/replacements/model_windmill.obj";
constexpr wchar_t kWindmillObjPathC[] = L"Assets/models/model_windmill.obj";

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

struct HudVertex {
    float x, y, z, rhw;
    DWORD color;
};

constexpr DWORD kHudFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

void DrawQuad2D(IDirect3DDevice9* device, float x0, float y0, float x1, float y1, DWORD color) {
    if (!device) {
        return;
    }
    const float px0 = x0 - 0.5f;
    const float py0 = y0 - 0.5f;
    const float px1 = x1 - 0.5f;
    const float py1 = y1 - 0.5f;
    const HudVertex verts[4] = {
        {px0, py0, 0.0f, 1.0f, color},
        {px1, py0, 0.0f, 1.0f, color},
        {px0, py1, 0.0f, 1.0f, color},
        {px1, py1, 0.0f, 1.0f, color},
    };
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(HudVertex));
}

void DrawLine2D(IDirect3DDevice9* device, float x0, float y0, float x1, float y1, DWORD color) {
    if (!device) {
        return;
    }
    const HudVertex verts[2] = {
        {x0 - 0.5f, y0 - 0.5f, 0.0f, 1.0f, color},
        {x1 - 0.5f, y1 - 0.5f, 0.0f, 1.0f, color},
    };
    device->DrawPrimitiveUP(D3DPT_LINELIST, 1, verts, sizeof(HudVertex));
}

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

    // Legacy .x props can include broken helper/shadow geometry; prefer OBJ replacements only.
    const auto jeep_candidates = std::array<const wchar_t*, 4>{kJeepObjPathA, kJeepObjPathB,
                                                                kJeepObjPathC, kJeepXPath};
    const StaticModelLoadResult jeep_load = LoadStaticModelCandidates(device, &jeep_, jeep_candidates);
    if (FAILED(jeep_load.hr) || !jeep_.IsLoaded()) {
        const HRESULT fallback_hr =
            jeep_.CreateBox(device, 4.6f, 1.8f, 2.4f, L"Assets/terrain/tex_stone.jpg");
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

    // Legacy .x props can include broken helper/shadow geometry; prefer OBJ replacements only.
    const auto crate_candidates =
        std::array<const wchar_t*, 3>{kCrateObjPathA, kCrateObjPathB, kCrateObjPathC};
    const StaticModelLoadResult crate_load = LoadStaticModelCandidates(device, &crate_, crate_candidates);
    if (FAILED(crate_load.hr) || !crate_.IsLoaded()) {
        const HRESULT fallback_hr =
            crate_.CreateBox(device, 4.4f, 4.4f, 4.4f, L"Assets/terrain/tex_crate.png");
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

    // Legacy .x props can include broken helper/shadow geometry; prefer OBJ replacements only.
    const auto boulder_candidates = std::array<const wchar_t*, 3>{kBoulderObjPathA, kBoulderObjPathB,
                                                                   kBoulderObjPathC};
    const StaticModelLoadResult boulder_load =
        LoadStaticModelCandidates(device, &boulder_mesh_, boulder_candidates);
    if (FAILED(boulder_load.hr) || !boulder_mesh_.IsLoaded()) {
        const HRESULT fallback_hr = boulder_mesh_.CreateSphere(device, boulder_radius_, 20, 14,
                                                               L"Assets/terrain/tex_rock.jpg");
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

    windmill_is_static_mesh_ = false;
    windmill_loaded_ = false;
    windmill_has_animation_ = false;
    windmill_anim_paused_ = false;

    const std::filesystem::path windmill_path = ResolveAssetPath(kWindmillPath);
    if (!windmill_path.empty()) {
        const HRESULT wh = windmill_.Load(device, windmill_path.c_str());
        if (SUCCEEDED(wh) && windmill_.IsLoaded()) {
            windmill_loaded_ = true;
            windmill_has_animation_ = windmill_.HasAnimation();
        }
    }

    if (!windmill_loaded_) {
        const auto windmill_candidates =
            std::array<const wchar_t*, 4>{kWindmillObjPathA, kWindmillObjPathB, kWindmillObjPathC,
                                          kWindmillPath};
        const StaticModelLoadResult wm_static =
            LoadStaticModelCandidates(device, &windmill_static_, windmill_candidates);
        if (SUCCEEDED(wm_static.hr) && windmill_static_.IsLoaded()) {
            windmill_loaded_ = true;
            windmill_is_static_mesh_ = true;
            windmill_has_animation_ = false;
        }
    }

    if (FAILED(D3DXCreateFontW(device, 20, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI",
                               &font_))) {
        font_ = nullptr;
    }

    crate_pos_.y = terrain_.SampleHeight(crate_pos_.x, crate_pos_.z);
    windmill_pos_.y = terrain_.SampleHeight(windmill_pos_.x, windmill_pos_.z);
    boulder_pos_.y = terrain_.SampleHeight(boulder_pos_.x, boulder_pos_.z) + boulder_radius_;

    const D3DXVECTOR3 start(8.f, 0.f, -18.f);
    D3DXVECTOR3 vs = start;
    vs.y = terrain_.SampleHeight(vs.x, vs.z) + 1.1f;
    vehicle_.Reset(vs, 0.4f);
    boulder_vel_ = D3DXVECTOR3(0.f, 0.f, 0.f);
    sim_time_ = 0.0;
    fps_smoothed_ = 0.f;
    wants_quit_ = false;
    return true;
}

void Game::Shutdown() {
    if (font_) {
        font_->Release();
        font_ = nullptr;
    }
    windmill_.Release();
    windmill_static_.Unload();
    windmill_loaded_ = false;
    windmill_is_static_mesh_ = false;
    windmill_has_animation_ = false;
    windmill_anim_paused_ = false;
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

    sim_time_ += static_cast<double>(dt);
    if (dt > 1e-6f) {
        const float instant_fps = 1.f / dt;
        fps_smoothed_ =
            (fps_smoothed_ <= 0.f) ? instant_fps : (fps_smoothed_ * 0.9f + instant_fps * 0.1f);
    }
    const float sim_time_f = static_cast<float>(sim_time_);
    const float day_t = std::fmod(sim_time_f * 0.12f, 6.2831853f);
    const float sun_h = std::sinf(day_t);
    const float sun_x = std::cosf(day_t * 0.7f);
    const float sun_z = std::sinf(day_t * 0.7f);
    D3DXVECTOR3 dir(sun_x, -0.55f - sun_h * 0.85f, sun_z);
    D3DXVec3Normalize(&sun_dir_, &dir);

    const float blend = std::clamp(0.5f + 0.5f * sun_h, 0.f, 1.f);
    LerpColor(&clear_color_, 1.f - blend, D3DCOLOR_XRGB(12, 18, 42), D3DCOLOR_XRGB(135, 190, 235));
    graphics.SetClearColor(clear_color_);

    const float amb_day = 0.30f;
    // Keep ground readable at night; clear-color night sky can still look brighter than terrain
    // without a reasonable ambient floor.
    const float amb_night = 0.17f;
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
    wchar_t line[768];
    swprintf_s(line, L"MAEG4060 sandbox | ~%.0f FPS | speed %.1f | pos (%.1f, %.1f, %.1f)",
               fps_smoothed_, vs.speed, vs.position.x, vs.position.y, vs.position.z);
    RECT r{8, 8, client_w - 8, (std::min)(36, bottom_pad)};
    font_->DrawTextW(nullptr, line, -1, &r, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 255, 255, 255));
    const wchar_t* windmill_state = L"off";
    if (windmill_loaded_) {
        if (windmill_has_animation_) {
            windmill_state = windmill_anim_paused_ ? L"paused" : L"anim";
        } else if (windmill_is_static_mesh_) {
            windmill_state = windmill_anim_paused_ ? L"paused" : L"spin";
        } else {
            windmill_state = L"static";
        }
    }
    const int64_t total_tenths = static_cast<int64_t>(sim_time_ * 10.0 + 0.5);
    const int sim_minutes = static_cast<int>(total_tenths / 600);
    const int sim_seconds = static_cast<int>((total_tenths / 10) % 60);
    const int sim_tenths = static_cast<int>(total_tenths % 10);
    swprintf_s(line, L"sim t=%02d:%02d.%01d | windmill=%s", sim_minutes, sim_seconds, sim_tenths,
               windmill_state);
    RECT r2{8, 32, client_w - 8, (std::min)(56, bottom_pad)};
    font_->DrawTextW(nullptr, line, -1, &r2, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 220, 240, 255));

    const wchar_t* terrain_src = terrain_.UsingHeightmap() ? L"heightmap image" : L"procedural";
    swprintf_s(line, L"Terrain: %s | grass + rock + stone + mountain splats | crate texture on props",
               terrain_src);
    RECT r3{8, 56, client_w - 8, (std::min)(80, bottom_pad)};
    font_->DrawTextW(nullptr, line, -1, &r3, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 210, 230, 255));

    const wchar_t* wasd_overlay =
        L"W = forward      S = reverse\n"
        L"A = steer left   D = steer right\n"
        L"Space = brake    Esc = quit    M = windmill\n"
        L"(Gamepad: steer / triggers)";
    const int overlay_h = 92;
    RECT rw{(std::max)(8, client_w - 460), (std::max)(8, client_h - overlay_h - 12), client_w - 14,
            client_h - 10};
    const UINT wasd_fmt = DT_RIGHT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK;
    RECT rw_shadow = rw;
    rw_shadow.left += 2;
    rw_shadow.right += 2;
    rw_shadow.top += 2;
    rw_shadow.bottom += 2;
    font_->DrawTextW(nullptr, wasd_overlay, -1, &rw_shadow, wasd_fmt, D3DCOLOR_ARGB(240, 0, 0, 0));
    font_->DrawTextW(nullptr, wasd_overlay, -1, &rw, wasd_fmt, D3DCOLOR_ARGB(255, 255, 245, 160));

    DrawMiniMap(device, client_w, client_h);
}

void Game::DrawMiniMap(IDirect3DDevice9* device, int client_w, int client_h) {
    if (!device || client_w < 240 || client_h < 220) {
        return;
    }

    const float map_size = 170.0f;
    const float margin = 14.0f;
    const float map_left = static_cast<float>(client_w) - map_size - margin;
    const float map_top = margin;
    const float map_right = map_left + map_size;
    const float map_bottom = map_top + map_size;

    DWORD prev_fvf = 0;
    device->GetFVF(&prev_fvf);
    DWORD prev_lighting = FALSE;
    DWORD prev_z = FALSE;
    DWORD prev_alpha = FALSE;
    DWORD prev_src = D3DBLEND_ONE;
    DWORD prev_dst = D3DBLEND_ZERO;
    device->GetRenderState(D3DRS_LIGHTING, &prev_lighting);
    device->GetRenderState(D3DRS_ZENABLE, &prev_z);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &prev_alpha);
    device->GetRenderState(D3DRS_SRCBLEND, &prev_src);
    device->GetRenderState(D3DRS_DESTBLEND, &prev_dst);

    device->SetTexture(0, nullptr);
    device->SetFVF(kHudFvf);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    DrawQuad2D(device, map_left, map_top, map_right, map_bottom, D3DCOLOR_ARGB(130, 12, 18, 24));
    DrawLine2D(device, map_left, map_top, map_right, map_top, D3DCOLOR_ARGB(220, 180, 210, 240));
    DrawLine2D(device, map_right, map_top, map_right, map_bottom, D3DCOLOR_ARGB(220, 180, 210, 240));
    DrawLine2D(device, map_right, map_bottom, map_left, map_bottom, D3DCOLOR_ARGB(220, 180, 210, 240));
    DrawLine2D(device, map_left, map_bottom, map_left, map_top, D3DCOLOR_ARGB(220, 180, 210, 240));

    const float half_w = terrain_.HalfWidth();
    const float half_d = terrain_.HalfDepth();
    const auto world_to_map = [&](const D3DXVECTOR3& p, float* out_x, float* out_y) {
        const float u = std::clamp((p.x + half_w) / (2.0f * half_w), 0.0f, 1.0f);
        const float v = std::clamp((p.z + half_d) / (2.0f * half_d), 0.0f, 1.0f);
        *out_x = map_left + u * map_size;
        *out_y = map_top + v * map_size;
    };

    auto draw_marker = [&](const D3DXVECTOR3& pos, float radius, DWORD color) {
        float mx = 0.0f;
        float my = 0.0f;
        world_to_map(pos, &mx, &my);
        DrawQuad2D(device, mx - radius, my - radius, mx + radius, my + radius, color);
    };

    draw_marker(crate_pos_, 2.5f, D3DCOLOR_ARGB(230, 255, 210, 80));
    draw_marker(boulder_pos_, 2.8f, D3DCOLOR_ARGB(230, 255, 120, 120));
    draw_marker(windmill_pos_, 2.5f, D3DCOLOR_ARGB(230, 120, 220, 255));

    const VehicleState& vs = vehicle_.State();
    float vx = 0.0f;
    float vy = 0.0f;
    world_to_map(vs.position, &vx, &vy);
    DrawQuad2D(device, vx - 3.0f, vy - 3.0f, vx + 3.0f, vy + 3.0f, D3DCOLOR_ARGB(245, 80, 255, 120));
    const float dir_len = 9.0f;
    DrawLine2D(device, vx, vy, vx + std::sinf(vs.yaw) * dir_len, vy + std::cosf(vs.yaw) * dir_len,
               D3DCOLOR_ARGB(245, 80, 255, 120));

    device->SetRenderState(D3DRS_SRCBLEND, prev_src);
    device->SetRenderState(D3DRS_DESTBLEND, prev_dst);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, prev_alpha);
    device->SetRenderState(D3DRS_ZENABLE, prev_z);
    device->SetRenderState(D3DRS_LIGHTING, prev_lighting);
    device->SetFVF(prev_fvf);

    if (font_) {
        RECT label{
            static_cast<LONG>(map_left),
            static_cast<LONG>(map_bottom + 4.0f),
            static_cast<LONG>(map_right),
            static_cast<LONG>(map_bottom + 26.0f),
        };
        font_->DrawTextW(nullptr, L"Mini map", -1, &label, DT_LEFT | DT_TOP,
                         D3DCOLOR_ARGB(230, 200, 230, 255));

        const float legend_y = map_bottom + 22.0f;
        DrawQuad2D(device, map_left, legend_y + 2.0f, map_left + 7.0f, legend_y + 9.0f,
                   D3DCOLOR_ARGB(245, 80, 255, 120));
        DrawQuad2D(device, map_left + 48.0f, legend_y + 2.0f, map_left + 55.0f, legend_y + 9.0f,
                   D3DCOLOR_ARGB(230, 255, 210, 80));
        DrawQuad2D(device, map_left + 98.0f, legend_y + 2.0f, map_left + 105.0f, legend_y + 9.0f,
                   D3DCOLOR_ARGB(230, 255, 120, 120));
        DrawQuad2D(device, map_left + 150.0f, legend_y + 2.0f, map_left + 157.0f, legend_y + 9.0f,
                   D3DCOLOR_ARGB(230, 120, 220, 255));

        RECT legend{
            static_cast<LONG>(map_left + 9.0f),
            static_cast<LONG>(map_bottom + 20.0f),
            static_cast<LONG>(map_right + 84.0f),
            static_cast<LONG>(map_bottom + 42.0f),
        };
        font_->DrawTextW(nullptr, L"You  Crate  Boulder  Windmill", -1, &legend, DT_LEFT | DT_TOP,
                         D3DCOLOR_ARGB(225, 220, 230, 245));
    }
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
        if (windmill_is_static_mesh_) {
            const float angle = windmill_anim_paused_ ? 0.f : static_cast<float>(sim_time_) * 0.65f;
            D3DXMATRIX rot;
            D3DXMATRIX tr;
            D3DXMATRIX wm;
            D3DXMatrixRotationY(&rot, angle);
            D3DXMatrixTranslation(&tr, windmill_pos_.x, windmill_pos_.y, windmill_pos_.z);
            D3DXMatrixMultiply(&wm, &rot, &tr);
            device->SetTransform(D3DTS_WORLD, &wm);
            windmill_static_.Draw(device);
        } else {
            D3DXMATRIX wm;
            D3DXMatrixTranslation(&wm, windmill_pos_.x, windmill_pos_.y, windmill_pos_.z);
            windmill_.Draw(device, &wm);
        }
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
    D3DXMatrixTranslation(&tr, boulder_pos_.x, boulder_pos_.y, boulder_pos_.z);
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
