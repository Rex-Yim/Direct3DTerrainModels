#pragma once

#include <d3d9.h>
#include <d3dx9math.h>

#include <vector>

/**
 * @brief Heightfield terrain: bilinear height sampling, CPU mesh, grass/rock splatting (FFP).
 */
class Terrain {
public:
    Terrain() = default;
    ~Terrain();

    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    bool Initialize(IDirect3DDevice9* device);
    void Shutdown();

    /** World XZ extends [-halfWidth, halfWidth] x [-halfDepth, halfDepth]. */
    float HalfWidth() const { return half_width_; }
    float HalfDepth() const { return half_depth_; }

    float SampleHeight(float world_x, float world_z) const;
    void SampleNormal(float world_x, float world_z, D3DXVECTOR3* out_normal) const;

    void Draw(IDirect3DDevice9* device);

private:
    bool BuildHeightData();
    bool CreateTextures(IDirect3DDevice9* device);
    bool CreateMesh(IDirect3DDevice9* device);

    int CellCountX() const { return grid_cells_x_; }
    int CellCountZ() const { return grid_cells_z_; }
    float HeightAtGrid(int ix, int iz) const;

    IDirect3DDevice9* device_ = nullptr;

    float half_width_ = 200.f;
    float half_depth_ = 200.f;
    int grid_cells_x_ = 128;
    int grid_cells_z_ = 128;

    std::vector<float> heights_;
    int hm_res_x_ = 0;
    int hm_res_z_ = 0;

    IDirect3DVertexBuffer9* vb_ = nullptr;
    IDirect3DIndexBuffer9* ib_ = nullptr;
    UINT num_indices_ = 0;

    IDirect3DTexture9* grass_tex_ = nullptr;
    IDirect3DTexture9* rock_tex_ = nullptr;
};
