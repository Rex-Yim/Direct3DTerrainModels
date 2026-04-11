#pragma once

#include <d3d9.h>
#include <d3dx9mesh.h>

#include <vector>

// Loads a DirectX .x mesh via D3DXLoadMeshFromX (wide API: D3DXLoadMeshFromXW) and optional textures.
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

    HRESULT LoadFromFile(IDirect3DDevice9* device, const wchar_t* xFilePath) {
        return LoadMeshFromX(device, xFilePath);
    }

    void Draw(IDirect3DDevice9* device) const;

    bool IsLoaded() const { return mesh_ != nullptr; }
    ID3DXMesh* Mesh() const { return mesh_; }
    DWORD NumMaterials() const { return static_cast<DWORD>(materials_.size()); }

private:
    void Release();

    ID3DXMesh* mesh_ = nullptr;
    std::vector<D3DMATERIAL9> materials_;
    std::vector<IDirect3DTexture9*> textures_;
};
