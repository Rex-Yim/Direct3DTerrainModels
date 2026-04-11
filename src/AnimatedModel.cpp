#include "AnimatedModel.h"

#include <d3dx9mesh.h>

#include <cstring>
#include <string>
#include <vector>
#include <new>

namespace {

IDirect3DDevice9* g_alloc_device = nullptr;
std::wstring g_alloc_tex_dir;

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

HRESULT TryLoadTexture(IDirect3DDevice9* device, const char* textureFilename,
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
    const std::wstring combined = g_alloc_tex_dir + wideName;
    return D3DXCreateTextureFromFileW(device, combined.c_str(), outTexture);
}

struct MeshContainerEx : public D3DXMESHCONTAINER {
    IDirect3DTexture9** textures = nullptr;
};

struct FrameEx : public D3DXFRAME {
    D3DXMATRIX combined;
};

class AllocateHierarchy : public ID3DXAllocateHierarchy {
public:
    STDMETHOD(CreateFrame)(THIS_ LPCSTR Name, LPD3DXFRAME* ppNewFrame) override;
    STDMETHOD(CreateMeshContainer)(THIS_ LPCSTR Name, CONST D3DXMESHDATA* pMeshData,
                                   CONST D3DXMATERIAL* pMaterials,
                                   CONST D3DXEFFECTINSTANCE* pEffectInstances, DWORD NumMaterials,
                                   CONST DWORD* pAdjacency, LPD3DXSKININFO pSkinInfo,
                                   LPD3DXMESHCONTAINER* ppNewMeshContainer) override;
    STDMETHOD(DestroyFrame)(THIS_ LPD3DXFRAME pFrameToFree) override;
    STDMETHOD(DestroyMeshContainer)(THIS_ LPD3DXMESHCONTAINER pMeshContainerToFree) override;
};

STDMETHODIMP AllocateHierarchy::CreateFrame(LPCSTR Name, LPD3DXFRAME* ppNewFrame) {
    auto* f = new FrameEx();
    memset(static_cast<D3DXFRAME*>(f), 0, sizeof(D3DXFRAME));
    D3DXMatrixIdentity(&f->TransformationMatrix);
    D3DXMatrixIdentity(&f->combined);
    if (Name && Name[0]) {
        const size_t len = strlen(Name) + 1;
        f->Name = new (std::nothrow) char[len];
        if (!f->Name) {
            delete f;
            *ppNewFrame = nullptr;
            return E_OUTOFMEMORY;
        }
        memcpy(f->Name, Name, len);
    } else {
        f->Name = nullptr;
    }
    *ppNewFrame = f;
    return S_OK;
}

STDMETHODIMP AllocateHierarchy::CreateMeshContainer(LPCSTR Name, CONST D3DXMESHDATA* pMeshData,
                                                    CONST D3DXMATERIAL* pMaterials,
                                                    CONST D3DXEFFECTINSTANCE* /*pEffectInstances*/,
                                                    DWORD NumMaterials, CONST DWORD* /*pAdjacency*/,
                                                    LPD3DXSKININFO /*pSkinInfo*/,
                                                    LPD3DXMESHCONTAINER* ppNewMeshContainer) {
    *ppNewMeshContainer = nullptr;
    if (!pMeshData || pMeshData->Type != D3DXMESHTYPE_MESH || !pMeshData->pMesh) {
        return E_FAIL;
    }

    auto* mc = new (std::nothrow) MeshContainerEx();
    if (!mc) {
        return E_OUTOFMEMORY;
    }
    memset(static_cast<D3DXMESHCONTAINER*>(mc), 0, sizeof(D3DXMESHCONTAINER));
    mc->textures = nullptr;
    if (Name && Name[0]) {
        const size_t len = strlen(Name) + 1;
        mc->Name = new (std::nothrow) char[len];
        if (!mc->Name) {
            delete mc;
            return E_OUTOFMEMORY;
        }
        memcpy(mc->Name, Name, len);
    } else {
        mc->Name = nullptr;
    }

    mc->MeshData.Type = D3DXMESHTYPE_MESH;
    mc->MeshData.pMesh = pMeshData->pMesh;
    pMeshData->pMesh->AddRef();

    mc->NumMaterials = NumMaterials;
    if (NumMaterials > 0 && pMaterials) {
        const size_t bytes = sizeof(D3DXMATERIAL) * static_cast<size_t>(NumMaterials);
        mc->pMaterials = static_cast<D3DXMATERIAL*>(operator new[](bytes));
        memset(mc->pMaterials, 0, bytes);
        mc->textures = new (std::nothrow) IDirect3DTexture9*[NumMaterials];
        if (!mc->textures) {
            delete[] reinterpret_cast<char*>(mc->pMaterials);
            mc->pMaterials = nullptr;
            mc->MeshData.pMesh->Release();
            delete mc;
            return E_OUTOFMEMORY;
        }
        for (DWORD i = 0; i < NumMaterials; ++i) {
            mc->pMaterials[i].MatD3D = pMaterials[i].MatD3D;
            mc->pMaterials[i].pTextureFilename = nullptr;
            if (pMaterials[i].pTextureFilename) {
                const size_t len = strlen(pMaterials[i].pTextureFilename) + 1;
                mc->pMaterials[i].pTextureFilename = new (std::nothrow) char[len];
                if (!mc->pMaterials[i].pTextureFilename) {
                    for (DWORD j = 0; j < i; ++j) {
                        delete[] mc->pMaterials[j].pTextureFilename;
                    }
                    delete[] mc->textures;
                    mc->textures = nullptr;
                    delete[] reinterpret_cast<char*>(mc->pMaterials);
                    mc->pMaterials = nullptr;
                    mc->MeshData.pMesh->Release();
                    delete mc;
                    return E_OUTOFMEMORY;
                }
                memcpy(mc->pMaterials[i].pTextureFilename, pMaterials[i].pTextureFilename, len);
            }
            mc->textures[i] = nullptr;
            TryLoadTexture(g_alloc_device, mc->pMaterials[i].pTextureFilename, &mc->textures[i]);
        }
    }

    *ppNewMeshContainer = mc;
    return S_OK;
}

STDMETHODIMP AllocateHierarchy::DestroyFrame(LPD3DXFRAME pFrameToFree) {
    auto* f = static_cast<FrameEx*>(pFrameToFree);
    if (f && f->Name) {
        delete[] f->Name;
        f->Name = nullptr;
    }
    delete f;
    return S_OK;
}

STDMETHODIMP AllocateHierarchy::DestroyMeshContainer(LPD3DXMESHCONTAINER pMeshContainerToFree) {
    auto* mc = static_cast<MeshContainerEx*>(pMeshContainerToFree);
    if (!mc) {
        return S_OK;
    }
    if (mc->Name) {
        delete[] mc->Name;
        mc->Name = nullptr;
    }
    if (mc->textures) {
        for (DWORD i = 0; i < mc->NumMaterials; ++i) {
            if (mc->textures[i]) {
                mc->textures[i]->Release();
            }
        }
        delete[] mc->textures;
        mc->textures = nullptr;
    }
    if (mc->MeshData.pMesh) {
        mc->MeshData.pMesh->Release();
        mc->MeshData.pMesh = nullptr;
    }
    if (mc->pMaterials) {
        for (DWORD i = 0; i < mc->NumMaterials; ++i) {
            delete[] mc->pMaterials[i].pTextureFilename;
        }
        delete[] reinterpret_cast<char*>(mc->pMaterials);
        mc->pMaterials = nullptr;
    }
    delete mc;
    return S_OK;
}

void UpdateFrames(LPD3DXFRAME frame, const D3DXMATRIX* parent) {
    auto* f = static_cast<FrameEx*>(frame);
    D3DXMatrixMultiply(&f->combined, &frame->TransformationMatrix, parent);
    if (frame->pFrameSibling) {
        UpdateFrames(frame->pFrameSibling, parent);
    }
    if (frame->pFrameFirstChild) {
        UpdateFrames(frame->pFrameFirstChild, &f->combined);
    }
}

void DrawFrames(LPD3DXFRAME frame, IDirect3DDevice9* device, const D3DXMATRIX& root_world) {
    auto* f = static_cast<FrameEx*>(frame);
    D3DXMATRIX world;
    D3DXMatrixMultiply(&world, &root_world, &f->combined);

    auto* mc = static_cast<MeshContainerEx*>(frame->pMeshContainer);
    if (mc && mc->MeshData.Type == D3DXMESHTYPE_MESH && mc->MeshData.pMesh) {
        ID3DXMesh* mesh = mc->MeshData.pMesh;
        device->SetTransform(D3DTS_WORLD, &world);
        const DWORD fvf = mesh->GetFVF();
        device->SetFVF(fvf);
        const DWORD n = mc->NumMaterials;
        if (n == 0) {
            D3DMATERIAL9 m{};
            m.Diffuse.r = m.Diffuse.g = m.Diffuse.b = m.Diffuse.a = 1.f;
            m.Ambient = m.Diffuse;
            device->SetMaterial(&m);
            device->SetTexture(0, nullptr);
            mesh->DrawSubset(0);
        } else {
            for (DWORD i = 0; i < n; ++i) {
                device->SetMaterial(&mc->pMaterials[i].MatD3D);
                IDirect3DTexture9* tex =
                    (mc->textures && i < mc->NumMaterials) ? mc->textures[i] : nullptr;
                device->SetTexture(0, tex);
                mesh->DrawSubset(i);
            }
        }
        device->SetTexture(0, nullptr);
    }

    if (frame->pFrameSibling) {
        DrawFrames(frame->pFrameSibling, device, root_world);
    }
    if (frame->pFrameFirstChild) {
        DrawFrames(frame->pFrameFirstChild, device, root_world);
    }
}

std::wstring DirectoryOf(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return {};
    }
    return path.substr(0, pos + 1);
}

}  // namespace

HierarchyModel::~HierarchyModel() {
    Release();
}

void HierarchyModel::Release() {
    if (root_frame_) {
        AllocateHierarchy alloc;
        D3DXFrameDestroy(root_frame_, &alloc);
        root_frame_ = nullptr;
    }
    if (anim_controller_) {
        anim_controller_->Release();
        anim_controller_ = nullptr;
    }
    device_ = nullptr;
}

HRESULT HierarchyModel::Load(IDirect3DDevice9* device, const wchar_t* x_path) {
    Release();
    device_ = device;
    if (!device || !x_path) {
        return E_INVALIDARG;
    }

    const std::wstring path(x_path);
    g_alloc_tex_dir = DirectoryOf(path);
    g_alloc_device = device;
    AllocateHierarchy alloc;
    LPD3DXANIMATIONCONTROLLER anim = nullptr;
    const HRESULT hr = D3DXLoadMeshHierarchyFromX(
        x_path, D3DXMESH_MANAGED, device, &alloc, nullptr, &root_frame_, &anim);
    g_alloc_device = nullptr;
    g_alloc_tex_dir.clear();

    if (FAILED(hr) || !root_frame_) {
        if (anim) {
            anim->Release();
        }
        Release();
        return FAILED(hr) ? hr : E_FAIL;
    }

    anim_controller_ = anim;
    return S_OK;
}

void HierarchyModel::AdvanceTime(float dt_seconds) {
    if (anim_controller_) {
        anim_controller_->AdvanceTime(dt_seconds, nullptr);
    }
    D3DXMATRIX id;
    D3DXMatrixIdentity(&id);
    if (root_frame_) {
        UpdateFrames(root_frame_, &id);
    }
}

void HierarchyModel::Draw(IDirect3DDevice9* device, const D3DXMATRIX* root_world) {
    if (!device || !root_frame_) {
        return;
    }
    D3DXMATRIX rw;
    if (root_world) {
        rw = *root_world;
    } else {
        D3DXMatrixIdentity(&rw);
    }
    DrawFrames(root_frame_, device, rw);
}
