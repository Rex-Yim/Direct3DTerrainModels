#pragma once

#include <d3d9.h>
#include <d3dx9mesh.h>

#include <vector>

// Loads static meshes from legacy .x files or Wavefront OBJ/MTL pairs.
class ModelLoader {
public:
    ModelLoader() = default;
    ~ModelLoader();

    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;

    ModelLoader(ModelLoader&& other) noexcept;
    ModelLoader& operator=(ModelLoader&& other) noexcept;

    /**
     * @brief Load geometry and materials from a .x file using D3DXLoadMeshFromXW.
     * @note D3DX exposes D3DXLoadMeshFromX / D3DXLoadMeshFromXA / D3DXLoadMeshFromXW; we use the W variant for Unicode paths.
     */
    HRESULT LoadMeshFromX(IDirect3DDevice9* device, const wchar_t* xFilePath);
    // Loads mesh from Wavefront OBJ and optional MTL materials/textures.
    HRESULT LoadMeshFromObj(IDirect3DDevice9* device, const wchar_t* objFilePath);
    HRESULT CreateBox(IDirect3DDevice9* device, float width, float height, float depth,
                      const wchar_t* optional_diffuse_texture = nullptr);
    HRESULT CreateSphere(IDirect3DDevice9* device, float radius, UINT slices, UINT stacks,
                         const wchar_t* optional_diffuse_texture = nullptr);
    // Extension-dispatch helper: currently supports .x and .obj.
    HRESULT LoadFromFile(IDirect3DDevice9* device, const wchar_t* path);

    void Draw(IDirect3DDevice9* device) const;

    void Unload() { Release(); }

    bool IsLoaded() const { return mesh_ != nullptr; }
    ID3DXMesh* Mesh() const { return mesh_; }
    DWORD NumMaterials() const { return static_cast<DWORD>(materials_.size()); }

private:
    void Release();

    ID3DXMesh* mesh_ = nullptr;
    std::vector<D3DMATERIAL9> materials_;
    std::vector<IDirect3DTexture9*> textures_;
};
