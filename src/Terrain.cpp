#include "Terrain.h"

#include <windows.h>

#include <d3dx9.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {

struct TerrainVertex {
    D3DXVECTOR3 pos;
    D3DXVECTOR3 nml;
    DWORD diffuse;
    float u, v;
};

constexpr DWORD kTerrainFvf = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;

float Saturate(float x) {
    return std::clamp(x, 0.f, 1.f);
}

float ProceduralHeight(float wx, float wz) {
    const float nx = wx * 0.018f;
    const float nz = wz * 0.018f;
    float h = 6.f * std::sinf(nx) * std::cosf(nz);
    h += 12.f * std::sinf(nx * 0.28f) * std::sinf(nz * 0.31f);
    h += 0.00018f * (wx * wx + wz * wz);
    h += 3.f * std::sinf((wx + wz) * 0.012f);
    return h;
}

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

std::filesystem::path ResolveTerrainAssetPath(const wchar_t* relative_path) {
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

template <size_t N>
bool TryLoadTextureCandidates(IDirect3DDevice9* device,
                              const wchar_t* const (&candidates)[N],
                              IDirect3DTexture9** out_texture) {
    if (!device || !out_texture) {
        return false;
    }

    *out_texture = nullptr;
    for (const wchar_t* candidate : candidates) {
        const std::filesystem::path resolved = ResolveTerrainAssetPath(candidate);
        if (resolved.empty()) {
            continue;
        }

        IDirect3DTexture9* texture = nullptr;
        if (SUCCEEDED(D3DXCreateTextureFromFileW(device, resolved.c_str(), &texture)) && texture) {
            *out_texture = texture;
            return true;
        }
    }
    return false;
}

HRESULT FillSolidNoiseTexture(IDirect3DDevice9* device, IDirect3DTexture9** out, DWORD base_rgb,
                              bool noise) {
    IDirect3DTexture9* tex = nullptr;
    constexpr int kSize = 128;
    if (FAILED(device->CreateTexture(kSize, kSize, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex,
                                     nullptr))) {
        return E_FAIL;
    }

    D3DLOCKED_RECT lr{};
    if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) {
        tex->Release();
        return E_FAIL;
    }

    const DWORD br = base_rgb & 0xFF;
    const DWORD bg = (base_rgb >> 8) & 0xFF;
    const DWORD bb = (base_rgb >> 16) & 0xFF;
    auto* dst = static_cast<DWORD*>(lr.pBits);
    const int pitch = lr.Pitch / static_cast<int>(sizeof(DWORD));

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            float n = 0.f;
            if (noise) {
                n = 0.15f * std::sinf(static_cast<float>(x * 13 + y * 7));
            }
            const int rr = static_cast<int>(std::clamp(static_cast<float>(br) * (1.f + n), 0.f, 255.f));
            const int gg = static_cast<int>(std::clamp(static_cast<float>(bg) * (1.f + n), 0.f, 255.f));
            const int bbb = static_cast<int>(std::clamp(static_cast<float>(bb) * (1.f + n), 0.f, 255.f));
            dst[y * pitch + x] = D3DCOLOR_XRGB(rr, gg, bbb);
        }
    }

    tex->UnlockRect(0);
    *out = tex;
    return S_OK;
}

}  // namespace

Terrain::~Terrain() {
    Shutdown();
}

void Terrain::Shutdown() {
    if (ib_) {
        ib_->Release();
        ib_ = nullptr;
    }
    if (vb_) {
        vb_->Release();
        vb_ = nullptr;
    }
    if (grass_tex_) {
        grass_tex_->Release();
        grass_tex_ = nullptr;
    }
    if (rock_tex_) {
        rock_tex_->Release();
        rock_tex_ = nullptr;
    }
    heights_.clear();
    hm_res_x_ = hm_res_z_ = 0;
    num_indices_ = 0;
    device_ = nullptr;
}

bool Terrain::BuildHeightData() {
    hm_res_x_ = grid_cells_x_ + 1;
    hm_res_z_ = grid_cells_z_ + 1;
    heights_.resize(static_cast<size_t>(hm_res_x_) * static_cast<size_t>(hm_res_z_));
    for (int iz = 0; iz < hm_res_z_; ++iz) {
        const float tz = static_cast<float>(iz) / static_cast<float>(grid_cells_z_);
        const float wz = -half_depth_ + tz * (2.f * half_depth_);
        for (int ix = 0; ix < hm_res_x_; ++ix) {
            const float tx = static_cast<float>(ix) / static_cast<float>(grid_cells_x_);
            const float wx = -half_width_ + tx * (2.f * half_width_);
            heights_[static_cast<size_t>(iz) * static_cast<size_t>(hm_res_x_) +
                     static_cast<size_t>(ix)] = ProceduralHeight(wx, wz);
        }
    }
    return true;
}

float Terrain::HeightAtGrid(int ix, int iz) const {
    ix = std::clamp(ix, 0, hm_res_x_ - 1);
    iz = std::clamp(iz, 0, hm_res_z_ - 1);
    return heights_[static_cast<size_t>(iz) * static_cast<size_t>(hm_res_x_) + static_cast<size_t>(ix)];
}

float Terrain::SampleHeight(float world_x, float world_z) const {
    if (heights_.empty()) {
        return 0.f;
    }
    const float fx =
        ((world_x + half_width_) / (2.f * half_width_)) * static_cast<float>(hm_res_x_ - 1);
    const float fz =
        ((world_z + half_depth_) / (2.f * half_depth_)) * static_cast<float>(hm_res_z_ - 1);
    const float tcx = std::clamp(fx, 0.f, static_cast<float>(hm_res_x_ - 1));
    const float tcz = std::clamp(fz, 0.f, static_cast<float>(hm_res_z_ - 1));

    const int x0 = static_cast<int>(std::floor(tcx));
    const int z0 = static_cast<int>(std::floor(tcz));
    const int x1 = (std::min)(x0 + 1, hm_res_x_ - 1);
    const int z1 = (std::min)(z0 + 1, hm_res_z_ - 1);
    const float tx = tcx - static_cast<float>(x0);
    const float tz = tcz - static_cast<float>(z0);

    const float h00 = HeightAtGrid(x0, z0);
    const float h10 = HeightAtGrid(x1, z0);
    const float h01 = HeightAtGrid(x0, z1);
    const float h11 = HeightAtGrid(x1, z1);
    const float h0 = h00 * (1.f - tx) + h10 * tx;
    const float h1 = h01 * (1.f - tx) + h11 * tx;
    return h0 * (1.f - tz) + h1 * tz;
}

void Terrain::SampleNormal(float world_x, float world_z, D3DXVECTOR3* out_normal) const {
    constexpr float e = 0.75f;
    const float hxp = SampleHeight(world_x + e, world_z);
    const float hxm = SampleHeight(world_x - e, world_z);
    const float hzp = SampleHeight(world_x, world_z + e);
    const float hzm = SampleHeight(world_x, world_z - e);
    D3DXVECTOR3 n(-(hxp - hxm) / (2.f * e), 1.f, -(hzp - hzm) / (2.f * e));
    D3DXVec3Normalize(out_normal, &n);
}

bool Terrain::CreateTextures(IDirect3DDevice9* device) {
    const wchar_t* grass_candidates[] = {
        L"Assets/terrain/tex_grass.jpg",
        L"Assets/terrain/tex_grass.png",
    };
    const wchar_t* rock_candidates[] = {
        L"Assets/terrain/tex_rock.jpg",
        L"Assets/terrain/tex_rock.png",
        L"Assets/terrain/tex_mountain.png",
        L"Assets/terrain/tex_stone.jpg",
        L"Assets/terrain/tex_stone.png",
    };

    if (!TryLoadTextureCandidates(device, grass_candidates, &grass_tex_) &&
        FAILED(FillSolidNoiseTexture(device, &grass_tex_, D3DCOLOR_XRGB(55, 120, 45), true))) {
        return false;
    }
    if (!TryLoadTextureCandidates(device, rock_candidates, &rock_tex_) &&
        FAILED(FillSolidNoiseTexture(device, &rock_tex_, D3DCOLOR_XRGB(110, 105, 98), true))) {
        return false;
    }
    return true;
}

bool Terrain::CreateMesh(IDirect3DDevice9* device) {
    const int vx = hm_res_x_;
    const int vz = hm_res_z_;
    const UINT num_verts = static_cast<UINT>(vx * vz);
    const UINT num_tris = static_cast<UINT>((vx - 1) * (vz - 1) * 2);
    num_indices_ = num_tris * 3;

    if (FAILED(device->CreateVertexBuffer(
            num_verts * sizeof(TerrainVertex), 0, kTerrainFvf, D3DPOOL_MANAGED, &vb_, nullptr))) {
        return false;
    }
    if (FAILED(
            device->CreateIndexBuffer(num_indices_ * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED,
                                      &ib_, nullptr))) {
        return false;
    }

    TerrainVertex* v = nullptr;
    if (FAILED(vb_->Lock(0, 0, reinterpret_cast<void**>(&v), 0))) {
        return false;
    }

    const D3DXVECTOR3 up(0.f, 1.f, 0.f);
    for (int iz = 0; iz < vz; ++iz) {
        for (int ix = 0; ix < vx; ++ix) {
            const float tx = static_cast<float>(ix) / static_cast<float>(vx - 1);
            const float tz = static_cast<float>(iz) / static_cast<float>(vz - 1);
            const float wx = -half_width_ + tx * (2.f * half_width_);
            const float wz = -half_depth_ + tz * (2.f * half_depth_);
            const float y = HeightAtGrid(ix, iz);

            D3DXVECTOR3 n;
            const float hxp = HeightAtGrid(ix + 1, iz);
            const float hxm = HeightAtGrid(ix - 1, iz);
            const float hzp = HeightAtGrid(ix, iz + 1);
            const float hzm = HeightAtGrid(ix, iz - 1);
            const float cell_x = (2.f * half_width_) / static_cast<float>(grid_cells_x_);
            const float cell_z = (2.f * half_depth_) / static_cast<float>(grid_cells_z_);
            D3DXVECTOR3 tn(-(hxp - hxm) / (2.f * cell_x), 1.f, -(hzp - hzm) / (2.f * cell_z));
            D3DXVec3Normalize(&n, &tn);

            const float slope = 1.f - std::fabs(D3DXVec3Dot(&n, &up));
            const BYTE alpha =
                static_cast<BYTE>(Saturate((slope - 0.22f) * 2.8f) * 255.f);

            TerrainVertex& vtx = v[static_cast<size_t>(iz) * static_cast<size_t>(vx) + static_cast<size_t>(ix)];
            vtx.pos = D3DXVECTOR3(wx, y, wz);
            vtx.nml = n;
            vtx.diffuse = D3DCOLOR_RGBA(255, 255, 255, alpha);
            vtx.u = tx * 8.f;
            vtx.v = tz * 8.f;
        }
    }
    vb_->Unlock();

    WORD* idx = nullptr;
    if (FAILED(ib_->Lock(0, 0, reinterpret_cast<void**>(&idx), 0))) {
        return false;
    }
    UINT k = 0;
    for (int iz = 0; iz < vz - 1; ++iz) {
        for (int ix = 0; ix < vx - 1; ++ix) {
            const WORD i00 = static_cast<WORD>(iz * vx + ix);
            const WORD i10 = static_cast<WORD>(i00 + 1);
            const WORD i01 = static_cast<WORD>(i00 + vx);
            const WORD i11 = static_cast<WORD>(i01 + 1);
            idx[k++] = i00;
            idx[k++] = i11;
            idx[k++] = i10;
            idx[k++] = i00;
            idx[k++] = i01;
            idx[k++] = i11;
        }
    }
    ib_->Unlock();
    return k == num_indices_;
}

bool Terrain::Initialize(IDirect3DDevice9* device) {
    Shutdown();
    device_ = device;
    if (!BuildHeightData()) {
        return false;
    }
    if (!CreateTextures(device)) {
        Shutdown();
        return false;
    }
    if (!CreateMesh(device)) {
        Shutdown();
        return false;
    }
    return true;
}

void Terrain::Draw(IDirect3DDevice9* device) {
    if (!device || !vb_ || !ib_ || !grass_tex_) {
        return;
    }

    device->SetFVF(kTerrainFvf);
    device->SetStreamSource(0, vb_, 0, sizeof(TerrainVertex));
    device->SetIndices(ib_);

    device->SetTexture(0, grass_tex_);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                 static_cast<UINT>(hm_res_x_ * hm_res_z_), 0,
                                 num_indices_ / 3);

    if (rock_tex_) {
        device->SetTexture(0, rock_tex_);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                     static_cast<UINT>(hm_res_x_ * hm_res_z_), 0,
                                     num_indices_ / 3);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    }

    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
}
