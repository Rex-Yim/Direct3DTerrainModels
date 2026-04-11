#include "ModelLoader.h"

#include <d3dx9.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

std::wstring AnsiToWide(const char* ansi) {
    if (!ansi || !ansi[0]) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::vector<wchar_t> buf(static_cast<size_t>(n));
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, buf.data(), n);
    return std::wstring(buf.data());
}

std::wstring DirectoryOf(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return {};
    }
    return path.substr(0, pos + 1);
}

HRESULT TryLoadTexture(IDirect3DDevice9* device,
                       const char* textureFilename,
                       const std::wstring& xDirectory,
                       IDirect3DTexture9** outTexture) {
    *outTexture = nullptr;
    if (!textureFilename || textureFilename[0] == '\0') {
        return S_OK;
    }

    HRESULT hr = D3DXCreateTextureFromFileA(device, textureFilename, outTexture);
    if (SUCCEEDED(hr) && *outTexture) {
        return S_OK;
    }

    const std::wstring wideName = AnsiToWide(textureFilename);
    if (wideName.empty()) {
        return hr;
    }

    const std::wstring combined = xDirectory + wideName;
    return D3DXCreateTextureFromFileW(device, combined.c_str(), outTexture);
}

}  // namespace

ModelLoader::~ModelLoader() {
    Release();
}

ModelLoader::ModelLoader(ModelLoader&& other) noexcept {
    mesh_ = other.mesh_;
    materials_ = std::move(other.materials_);
    textures_ = std::move(other.textures_);
    other.mesh_ = nullptr;
    other.textures_.clear();
    other.materials_.clear();
}

ModelLoader& ModelLoader::operator=(ModelLoader&& other) noexcept {
    if (this != &other) {
        Release();
        mesh_ = other.mesh_;
        materials_ = std::move(other.materials_);
        textures_ = std::move(other.textures_);
        other.mesh_ = nullptr;
        other.textures_.clear();
        other.materials_.clear();
    }
    return *this;
}

void ModelLoader::Release() {
    for (IDirect3DTexture9* t : textures_) {
        if (t) {
            t->Release();
        }
    }
    textures_.clear();
    materials_.clear();
    if (mesh_) {
        mesh_->Release();
        mesh_ = nullptr;
    }
}

HRESULT ModelLoader::LoadMeshFromX(IDirect3DDevice9* device, const wchar_t* xFilePath) {
    Release();
    if (!device || !xFilePath) {
        return E_INVALIDARG;
    }

    const std::wstring xPath(xFilePath);
    const std::wstring xDir = DirectoryOf(xPath);

    LPD3DXBUFFER adjacency = nullptr;
    LPD3DXBUFFER materialsBuffer = nullptr;
    LPD3DXBUFFER effectInstances = nullptr;
    DWORD numMaterials = 0;
    ID3DXMesh* mesh = nullptr;

    // D3DXLoadMeshFromX family: loads mesh + material list from a legacy .x scene file.
    const HRESULT hr = D3DXLoadMeshFromXW(
        xFilePath,
        D3DXMESH_MANAGED,
        device,
        &adjacency,
        &materialsBuffer,
        &effectInstances,
        &numMaterials,
        &mesh);

    if (adjacency) {
        adjacency->Release();
    }
    if (effectInstances) {
        effectInstances->Release();
    }

    if (FAILED(hr) || !mesh) {
        if (mesh) {
            mesh->Release();
        }
        if (materialsBuffer) {
            materialsBuffer->Release();
        }
        return FAILED(hr) ? hr : E_FAIL;
    }

    materials_.clear();
    textures_.clear();

    if (materialsBuffer && numMaterials > 0) {
        auto* d3dxMaterials = static_cast<D3DXMATERIAL*>(materialsBuffer->GetBufferPointer());
        materials_.resize(numMaterials);
        textures_.assign(numMaterials, nullptr);

        for (DWORD i = 0; i < numMaterials; ++i) {
            materials_[i] = d3dxMaterials[i].MatD3D;
            TryLoadTexture(device, d3dxMaterials[i].pTextureFilename, xDir, &textures_[i]);
        }
    }

    if (materialsBuffer) {
        materialsBuffer->Release();
    }

    mesh_ = mesh;
    return S_OK;
}

void ModelLoader::Draw(IDirect3DDevice9* device) const {
    if (!device || !mesh_) {
        return;
    }

    const DWORD fvf = mesh_->GetFVF();
    device->SetFVF(fvf);

    if (materials_.empty()) {
        D3DMATERIAL9 m{};
        m.Diffuse.r = m.Diffuse.g = m.Diffuse.b = m.Diffuse.a = 1.f;
        m.Ambient = m.Diffuse;
        device->SetMaterial(&m);
        device->SetTexture(0, nullptr);
        mesh_->DrawSubset(0);
        return;
    }

    const DWORD n = static_cast<DWORD>(materials_.size());
    for (DWORD i = 0; i < n; ++i) {
        device->SetMaterial(&materials_[i]);
        device->SetTexture(0, textures_[i]);
        mesh_->DrawSubset(i);
    }
    device->SetTexture(0, nullptr);
}
