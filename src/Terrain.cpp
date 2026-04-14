#include "Terrain.h"

#include <windows.h>

#include <d3dx9.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <vector>

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

// Full-resolution splats (e.g. 4K 16-bit PNGs) are slow to decode, heavy in VRAM, and costly to
// sample every frame. Cap the longer edge when loading so gameplay stays responsive.
bool CreateTerrainTextureFromFile(IDirect3DDevice9* device, const std::filesystem::path& path,
                                  UINT max_edge, IDirect3DTexture9** out_texture) {
    *out_texture = nullptr;
    if (!device || path.empty()) {
        return false;
    }

    D3DXIMAGE_INFO info{};
    if (FAILED(D3DXGetImageInfoFromFileW(path.c_str(), &info))) {
        return false;
    }

    UINT w = info.Width;
    UINT h = info.Height;
    while (w > max_edge || h > max_edge) {
        w = (w + 1u) / 2u;
        h = (h + 1u) / 2u;
    }
    if (w == 0 || h == 0) {
        return false;
    }

    IDirect3DTexture9* texture = nullptr;
    const HRESULT hr = D3DXCreateTextureFromFileExW(
        device, path.c_str(), w, h, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
        D3DX_FILTER_TRIANGLE, D3DX_FILTER_BOX, 0, nullptr, nullptr, &texture);
    if (SUCCEEDED(hr) && texture) {
        *out_texture = texture;
        return true;
    }
    if (texture) {
        texture->Release();
    }
    return false;
}

template <size_t N>
bool TryLoadTextureCandidates(IDirect3DDevice9* device,
                              const wchar_t* const (&candidates)[N],
                              IDirect3DTexture9** out_texture) {
    if (!device || !out_texture) {
        return false;
    }

    constexpr UINT kMaxTerrainTextureEdge = 1024;

    *out_texture = nullptr;
    for (const wchar_t* candidate : candidates) {
        const std::filesystem::path resolved = ResolveTerrainAssetPath(candidate);
        if (resolved.empty()) {
            continue;
        }

        IDirect3DTexture9* texture = nullptr;
        if (CreateTerrainTextureFromFile(device, resolved, kMaxTerrainTextureEdge, &texture) &&
            texture) {
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

bool LoadHeightsFromHeightmapImage(IDirect3DDevice9* device,
                                   std::vector<float>* heights,
                                   int hm_res_x,
                                   int hm_res_z,
                                   bool* heightmap_ok) {
    *heightmap_ok = false;
    if (!device || !heights || hm_res_x < 2 || hm_res_z < 2) {
        return false;
    }

    const wchar_t* candidates[] = {
        L"Assets/terrain/shanmai-height.png",
    };
    std::filesystem::path path;
    for (const wchar_t* candidate : candidates) {
        path = ResolveTerrainAssetPath(candidate);
        if (!path.empty()) {
            break;
        }
    }
    if (path.empty()) {
        return false;
    }

    D3DXIMAGE_INFO info{};
    if (FAILED(D3DXGetImageInfoFromFileW(path.c_str(), &info))) {
        return false;
    }

    const UINT w = info.Width;
    const UINT h = info.Height;
    if (w < 2 || h < 2) {
        return false;
    }

    IDirect3DSurface9* surf = nullptr;
    HRESULT hr = device->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &surf,
                                                     nullptr);
    if (FAILED(hr)) {
        hr = device->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surf,
                                                 nullptr);
    }
    if (FAILED(hr) || !surf) {
        return false;
    }

    if (FAILED(D3DXLoadSurfaceFromFileW(surf, nullptr, nullptr, path.c_str(), nullptr,
                                        D3DX_FILTER_NONE, 0, nullptr))) {
        surf->Release();
        return false;
    }

    D3DLOCKED_RECT lr{};
    if (FAILED(surf->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
        surf->Release();
        return false;
    }

    heights->resize(static_cast<size_t>(hm_res_x) * static_cast<size_t>(hm_res_z));

    auto sample_luma = [&](float fu, float fv) -> float {
        fu = std::clamp(fu, 0.f, 1.f);
        fv = std::clamp(fv, 0.f, 1.f);
        const float u = fu * static_cast<float>(w - 1);
        const float v = fv * static_cast<float>(h - 1);
        const int x0 = static_cast<int>(std::floor(u));
        const int y0 = static_cast<int>(std::floor(v));
        const int x1 = (std::min)(x0 + 1, static_cast<int>(w) - 1);
        const int y1 = (std::min)(y0 + 1, static_cast<int>(h) - 1);
        const float tx = u - static_cast<float>(x0);
        const float ty = v - static_cast<float>(y0);

        auto read = [&](int x, int y) -> float {
            const auto* row = reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(lr.pBits) +
                                                             y * lr.Pitch);
            const DWORD c = row[x];
            const int r = static_cast<int>((c >> 16) & 0xFF);
            const int g = static_cast<int>((c >> 8) & 0xFF);
            const int b = static_cast<int>(c & 0xFF);
            return (0.299f * static_cast<float>(r) + 0.587f * static_cast<float>(g) +
                    0.114f * static_cast<float>(b)) /
                255.f;
        };

        const float s00 = read(x0, y0);
        const float s10 = read(x1, y0);
        const float s01 = read(x0, y1);
        const float s11 = read(x1, y1);
        const float s0 = s00 * (1.f - tx) + s10 * tx;
        const float s1 = s01 * (1.f - tx) + s11 * tx;
        return s0 * (1.f - ty) + s1 * ty;
    };

    constexpr float kHeightScale = 44.f;
    constexpr float kHeightBias = -6.f;

    for (int iz = 0; iz < hm_res_z; ++iz) {
        const float fv = static_cast<float>(iz) / static_cast<float>(hm_res_z - 1);
        for (int ix = 0; ix < hm_res_x; ++ix) {
            const float fu = static_cast<float>(ix) / static_cast<float>(hm_res_x - 1);
            const float h01 = sample_luma(fu, fv);
            (*heights)[static_cast<size_t>(iz) * static_cast<size_t>(hm_res_x) +
                        static_cast<size_t>(ix)] = h01 * kHeightScale + kHeightBias;
        }
    }

    surf->UnlockRect();
    surf->Release();
    *heightmap_ok = true;
    return true;
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
    if (vb_mountain_splat_) {
        vb_mountain_splat_->Release();
        vb_mountain_splat_ = nullptr;
    }
    if (vb_stone_splat_) {
        vb_stone_splat_->Release();
        vb_stone_splat_ = nullptr;
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
    if (stone_tex_) {
        stone_tex_->Release();
        stone_tex_ = nullptr;
    }
    if (mountain_tex_) {
        mountain_tex_->Release();
        mountain_tex_ = nullptr;
    }
    heights_.clear();
    hm_res_x_ = hm_res_z_ = 0;
    num_indices_ = 0;
    chunks_.clear();
    heightmap_loaded_ = false;
    device_ = nullptr;
}

void Terrain::ExtractFrustumPlanes(const D3DXMATRIX& view_proj, D3DXPLANE out_planes[6]) {
    const D3DXMATRIX& m = view_proj;
    // Row-vector clip = v * m; planes from combined view*proj (Frank Luna / DX9 style).
    out_planes[0].a = m._14 + m._11;
    out_planes[0].b = m._24 + m._21;
    out_planes[0].c = m._34 + m._31;
    out_planes[0].d = m._44 + m._41;
    D3DXPlaneNormalize(&out_planes[0], &out_planes[0]);

    out_planes[1].a = m._14 - m._11;
    out_planes[1].b = m._24 - m._21;
    out_planes[1].c = m._34 - m._31;
    out_planes[1].d = m._44 - m._41;
    D3DXPlaneNormalize(&out_planes[1], &out_planes[1]);

    out_planes[2].a = m._14 + m._12;
    out_planes[2].b = m._24 + m._22;
    out_planes[2].c = m._34 + m._32;
    out_planes[2].d = m._44 + m._42;
    D3DXPlaneNormalize(&out_planes[2], &out_planes[2]);

    out_planes[3].a = m._14 - m._12;
    out_planes[3].b = m._24 - m._22;
    out_planes[3].c = m._34 - m._32;
    out_planes[3].d = m._44 - m._42;
    D3DXPlaneNormalize(&out_planes[3], &out_planes[3]);

    out_planes[4].a = m._13;
    out_planes[4].b = m._23;
    out_planes[4].c = m._33;
    out_planes[4].d = m._43;
    D3DXPlaneNormalize(&out_planes[4], &out_planes[4]);

    out_planes[5].a = m._14 - m._13;
    out_planes[5].b = m._24 - m._23;
    out_planes[5].c = m._34 - m._33;
    out_planes[5].d = m._44 - m._43;
    D3DXPlaneNormalize(&out_planes[5], &out_planes[5]);
}

bool Terrain::ChunkIntersectsFrustum(const TerrainChunk& chunk, const D3DXPLANE planes[6]) {
    for (int i = 0; i < 6; ++i) {
        const D3DXPLANE& p = planes[i];
        const D3DXVECTOR3 positive((p.a >= 0.f) ? chunk.aabb_max.x : chunk.aabb_min.x,
                                   (p.b >= 0.f) ? chunk.aabb_max.y : chunk.aabb_min.y,
                                   (p.c >= 0.f) ? chunk.aabb_max.z : chunk.aabb_min.z);
        if (D3DXPlaneDotCoord(&p, &positive) < 0.f) {
            return false;
        }
    }
    return true;
}

bool Terrain::BuildHeightData(IDirect3DDevice9* device) {
    hm_res_x_ = grid_cells_x_ + 1;
    hm_res_z_ = grid_cells_z_ + 1;
    heightmap_loaded_ = false;

    if (device && LoadHeightsFromHeightmapImage(device, &heights_, hm_res_x_, hm_res_z_,
                                               &heightmap_loaded_) &&
        heightmap_loaded_) {
        return true;
    }

    heights_.assign(static_cast<size_t>(hm_res_x_) * static_cast<size_t>(hm_res_z_), 0.f);
    heightmap_loaded_ = false;
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
        L"Assets/terrain/caoditietu.jpg",
    };
    const wchar_t* rock_candidates[] = {
        L"Assets/terrain/yanshitietu.jpg",
        L"Assets/terrain/shitoutietu.jpg",
    };
    const wchar_t* stone_candidates[] = {
        L"Assets/terrain/shitoutietu.jpg",
    };
    const wchar_t* mountain_candidates[] = {
        L"Assets/terrain/shanmaitietu.png",
    };

    if (!TryLoadTextureCandidates(device, grass_candidates, &grass_tex_) &&
        FAILED(FillSolidNoiseTexture(device, &grass_tex_, D3DCOLOR_XRGB(55, 120, 45), true))) {
        return false;
    }
    if (!TryLoadTextureCandidates(device, rock_candidates, &rock_tex_) &&
        FAILED(FillSolidNoiseTexture(device, &rock_tex_, D3DCOLOR_XRGB(110, 105, 98), true))) {
        return false;
    }
    TryLoadTextureCandidates(device, stone_candidates, &stone_tex_);
    TryLoadTextureCandidates(device, mountain_candidates, &mountain_tex_);
    return true;
}

bool Terrain::CreateMesh(IDirect3DDevice9* device) {
    if (vb_mountain_splat_) {
        vb_mountain_splat_->Release();
        vb_mountain_splat_ = nullptr;
    }
    if (vb_stone_splat_) {
        vb_stone_splat_->Release();
        vb_stone_splat_ = nullptr;
    }

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

    float min_h = FLT_MAX;
    float max_h = -FLT_MAX;
    for (int iz = 0; iz < vz; ++iz) {
        for (int ix = 0; ix < vx; ++ix) {
            const float y = HeightAtGrid(ix, iz);
            min_h = (std::min)(min_h, y);
            max_h = (std::max)(max_h, y);
        }
    }
    const float h_den = max_h - min_h + 1e-3f;

    std::vector<TerrainVertex> tmp(static_cast<size_t>(num_verts));
    std::vector<BYTE> stone_alpha(static_cast<size_t>(num_verts), 0);
    std::vector<BYTE> mountain_alpha(static_cast<size_t>(num_verts), 0);

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
            const BYTE rock_alpha =
                static_cast<BYTE>(Saturate((slope - 0.22f) * 2.8f) * 255.f);

            const float hn = (y - min_h) / h_den;
            const float stone_f =
                Saturate((slope - 0.1f) * 2.8f) * Saturate((0.42f - slope) * 4.f);
            const float mount_f =
                Saturate((hn - 0.42f) * 3.8f) * (0.35f + 0.65f * Saturate(slope * 1.4f));

            const size_t vi = static_cast<size_t>(iz) * static_cast<size_t>(vx) + static_cast<size_t>(ix);
            stone_alpha[vi] = static_cast<BYTE>(Saturate(stone_f) * 255.f);
            mountain_alpha[vi] = static_cast<BYTE>(Saturate(mount_f) * 255.f);

            TerrainVertex& vtx = tmp[vi];
            vtx.pos = D3DXVECTOR3(wx, y, wz);
            vtx.nml = n;
            vtx.diffuse = D3DCOLOR_RGBA(255, 255, 255, rock_alpha);
            vtx.u = tx * 8.f;
            vtx.v = tz * 8.f;
        }
    }

    TerrainVertex* v = nullptr;
    if (FAILED(vb_->Lock(0, 0, reinterpret_cast<void**>(&v), 0))) {
        return false;
    }
    std::memcpy(v, tmp.data(), static_cast<size_t>(num_verts) * sizeof(TerrainVertex));
    vb_->Unlock();

    auto make_splat_vb = [&](IDirect3DVertexBuffer9** out_vb, const std::vector<BYTE>& alpha) -> bool {
        if (FAILED(device->CreateVertexBuffer(
                num_verts * sizeof(TerrainVertex), 0, kTerrainFvf, D3DPOOL_MANAGED, out_vb,
                nullptr))) {
            return false;
        }
        TerrainVertex* dst = nullptr;
        if (FAILED((*out_vb)->Lock(0, 0, reinterpret_cast<void**>(&dst), 0))) {
            (*out_vb)->Release();
            *out_vb = nullptr;
            return false;
        }
        for (UINT i = 0; i < num_verts; ++i) {
            dst[i] = tmp[static_cast<size_t>(i)];
            dst[i].diffuse = D3DCOLOR_RGBA(255, 255, 255, alpha[static_cast<size_t>(i)]);
        }
        (*out_vb)->Unlock();
        return true;
    };

    if (stone_tex_ && !make_splat_vb(&vb_stone_splat_, stone_alpha)) {
        return false;
    }
    if (mountain_tex_ && !make_splat_vb(&vb_mountain_splat_, mountain_alpha)) {
        return false;
    }

    // Larger chunks => fewer draw calls when many are visible; smaller => tighter culling.
    constexpr int kCellsPerChunk = 32;
    if (grid_cells_x_ % kCellsPerChunk != 0 || grid_cells_z_ % kCellsPerChunk != 0) {
        return false;
    }
    const int chunks_x = grid_cells_x_ / kCellsPerChunk;
    const int chunks_z = grid_cells_z_ / kCellsPerChunk;

    chunks_.clear();
    chunks_.reserve(static_cast<size_t>(chunks_x * chunks_z));

    WORD* idx = nullptr;
    if (FAILED(ib_->Lock(0, 0, reinterpret_cast<void**>(&idx), 0))) {
        return false;
    }

    UINT k = 0;
    for (int cj = 0; cj < chunks_z; ++cj) {
        for (int ci = 0; ci < chunks_x; ++ci) {
            TerrainChunk chunk{};
            const int ix0 = ci * kCellsPerChunk;
            const int iz0 = cj * kCellsPerChunk;
            const int ix1 = ix0 + kCellsPerChunk;
            const int iz1 = iz0 + kCellsPerChunk;

            float min_x = FLT_MAX;
            float min_y = FLT_MAX;
            float min_z = FLT_MAX;
            float max_x = -FLT_MAX;
            float max_y = -FLT_MAX;
            float max_z = -FLT_MAX;
            for (int iz = iz0; iz <= iz1; ++iz) {
                for (int ix = ix0; ix <= ix1; ++ix) {
                    const float tx = static_cast<float>(ix) / static_cast<float>(vx - 1);
                    const float tz = static_cast<float>(iz) / static_cast<float>(vz - 1);
                    const float wx = -half_width_ + tx * (2.f * half_width_);
                    const float wz = -half_depth_ + tz * (2.f * half_depth_);
                    const float y = HeightAtGrid(ix, iz);
                    min_x = (std::min)(min_x, wx);
                    max_x = (std::max)(max_x, wx);
                    min_y = (std::min)(min_y, y);
                    max_y = (std::max)(max_y, y);
                    min_z = (std::min)(min_z, wz);
                    max_z = (std::max)(max_z, wz);
                }
            }
            constexpr float kPadXz = 2.f;
            constexpr float kPadY = 6.f;
            chunk.aabb_min = D3DXVECTOR3(min_x - kPadXz, min_y - kPadY, min_z - kPadXz);
            chunk.aabb_max = D3DXVECTOR3(max_x + kPadXz, max_y + kPadY, max_z + kPadXz);

            chunk.start_index = k;
            WORD min_v = 0xFFFF;
            WORD max_v = 0;
            for (int iz = iz0; iz < iz1; ++iz) {
                for (int ix = ix0; ix < ix1; ++ix) {
                    const WORD i00 = static_cast<WORD>(iz * vx + ix);
                    const WORD i10 = static_cast<WORD>(i00 + 1);
                    const WORD i01 = static_cast<WORD>(i00 + vx);
                    const WORD i11 = static_cast<WORD>(i01 + 1);
                    const WORD tri_indices[] = {i00, i11, i10, i00, i01, i11};
                    for (WORD index : tri_indices) {
                        idx[k++] = index;
                        min_v = (std::min)(min_v, index);
                        max_v = (std::max)(max_v, index);
                    }
                }
            }
            chunk.primitive_count = static_cast<UINT>(kCellsPerChunk * kCellsPerChunk * 2);
            chunk.min_vertex_index = min_v;
            chunk.vertex_count = static_cast<UINT>(max_v) - static_cast<UINT>(min_v) + 1u;
            chunks_.push_back(chunk);
        }
    }
    ib_->Unlock();
    return k == num_indices_ && chunks_.size() == static_cast<size_t>(chunks_x * chunks_z);
}

bool Terrain::Initialize(IDirect3DDevice9* device) {
    Shutdown();
    device_ = device;
    if (!BuildHeightData(device)) {
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

    // Without this, terrain can inherit the last mesh material (often dark) from the prior frame.
    D3DMATERIAL9 terrain_mtl{};
    terrain_mtl.Diffuse.r = terrain_mtl.Diffuse.g = terrain_mtl.Diffuse.b = terrain_mtl.Diffuse.a =
        1.f;
    terrain_mtl.Ambient = terrain_mtl.Diffuse;
    terrain_mtl.Specular.a = terrain_mtl.Specular.r = terrain_mtl.Specular.g =
        terrain_mtl.Specular.b = 0.f;
    terrain_mtl.Emissive.a = terrain_mtl.Emissive.r = terrain_mtl.Emissive.g =
        terrain_mtl.Emissive.b = 0.f;
    terrain_mtl.Power = 0.f;
    device->SetMaterial(&terrain_mtl);

    device->SetFVF(kTerrainFvf);
    device->SetStreamSource(0, vb_, 0, sizeof(TerrainVertex));
    device->SetIndices(ib_);

    D3DXMATRIX view{};
    D3DXMATRIX proj{};
    D3DXMATRIX vp{};
    device->GetTransform(D3DTS_VIEW, &view);
    device->GetTransform(D3DTS_PROJECTION, &proj);
    D3DXMatrixMultiply(&vp, &view, &proj);

    D3DXPLANE frustum[6]{};
    ExtractFrustumPlanes(vp, frustum);

    const bool use_chunks = !chunks_.empty();

    device->SetTexture(0, grass_tex_);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    if (use_chunks) {
        for (const TerrainChunk& ch : chunks_) {
            if (!ChunkIntersectsFrustum(ch, frustum)) {
                continue;
            }
            device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, ch.min_vertex_index, ch.vertex_count,
                                         ch.start_index, ch.primitive_count);
        }
    } else {
        device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                     static_cast<UINT>(hm_res_x_ * hm_res_z_), 0,
                                     num_indices_ / 3);
    }

    const auto draw_alpha_splat = [this, device, use_chunks, &frustum](IDirect3DTexture9* tex,
                                                                       IDirect3DVertexBuffer9* stream_vb) {
        if (!tex || !stream_vb) {
            return;
        }
        device->SetStreamSource(0, stream_vb, 0, sizeof(TerrainVertex));
        device->SetTexture(0, tex);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        if (use_chunks) {
            for (const TerrainChunk& ch : chunks_) {
                if (!ChunkIntersectsFrustum(ch, frustum)) {
                    continue;
                }
                device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, ch.min_vertex_index,
                                             ch.vertex_count, ch.start_index, ch.primitive_count);
            }
        } else {
            device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                         static_cast<UINT>(hm_res_x_ * hm_res_z_), 0,
                                         num_indices_ / 3);
        }
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    };

    draw_alpha_splat(rock_tex_, vb_);
    draw_alpha_splat(stone_tex_, vb_stone_splat_);
    draw_alpha_splat(mountain_tex_, vb_mountain_splat_);

    device->SetStreamSource(0, vb_, 0, sizeof(TerrainVertex));
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
}
