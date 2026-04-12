#include "ModelLoader.h"

#include <d3dx9.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr DWORD kMeshFvf = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;

struct MeshVertex {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float nx = 0.f;
    float ny = 0.f;
    float nz = 0.f;
    float u = 0.f;
    float v = 0.f;
};

struct ObjVertexKey {
    int position = -1;
    int texcoord = -1;
    int normal = -1;

    bool operator==(const ObjVertexKey& other) const {
        return position == other.position && texcoord == other.texcoord && normal == other.normal;
    }
};

struct ObjVertexKeyHash {
    size_t operator()(const ObjVertexKey& key) const noexcept {
        const size_t p = static_cast<size_t>(key.position + 1);
        const size_t t = static_cast<size_t>(key.texcoord + 1);
        const size_t n = static_cast<size_t>(key.normal + 1);
        return (p * 73856093u) ^ (t * 19349663u) ^ (n * 83492791u);
    }
};

struct ObjTriangle {
    ObjVertexKey vertices[3];
    DWORD material_index = 0;
};

D3DMATERIAL9 MakeDefaultMaterial() {
    D3DMATERIAL9 material{};
    material.Diffuse.r = 1.f;
    material.Diffuse.g = 1.f;
    material.Diffuse.b = 1.f;
    material.Diffuse.a = 1.f;
    material.Ambient = material.Diffuse;
    return material;
}

struct ObjMaterial {
    std::string name;
    D3DMATERIAL9 material = MakeDefaultMaterial();
    std::string texture_filename;
    bool has_ambient = false;
};

std::wstring AnsiToWide(const char* ansi) {
    if (!ansi || !ansi[0]) {
        return {};
    }

    const int needed = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
    if (needed <= 0) {
        return {};
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(needed));
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, buffer.data(), needed);
    return std::wstring(buffer.data());
}

std::string TrimCopy(const std::string& s) {
    const size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string TrimLeftCopy(const std::string& s) {
    const size_t first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? std::string() : s.substr(first);
}

std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool ShouldSkipCinema4dLine(const std::string& line) {
    const std::string trimmed = TrimLeftCopy(line);
    return trimmed.rfind("//", 0) == 0 || trimmed.rfind("{C4DMAT", 0) == 0;
}

std::filesystem::path MakeSanitizedXPath(const wchar_t* x_file_path) {
    if (!x_file_path) {
        return {};
    }

    std::ifstream input{std::filesystem::path(x_file_path)};
    if (!input) {
        return {};
    }

    const std::filesystem::path source_path(x_file_path);
    const std::filesystem::path sanitized_path =
        std::filesystem::temp_directory_path() / (source_path.stem().wstring() + L"_sanitized.x");

    std::ofstream output(sanitized_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return {};
    }

    std::string line;
    while (std::getline(input, line)) {
        if (!ShouldSkipCinema4dLine(line)) {
            output << line << '\n';
        }
    }

    return output ? sanitized_path : std::filesystem::path();
}

HRESULT TryLoadTexture(IDirect3DDevice9* device,
                       const std::string& texture_filename,
                       const std::filesystem::path& base_directory,
                       IDirect3DTexture9** out_texture) {
    *out_texture = nullptr;
    if (!device || texture_filename.empty()) {
        return S_OK;
    }

    const std::wstring wide_name = AnsiToWide(texture_filename.c_str());
    if (!wide_name.empty()) {
        const std::filesystem::path candidate = base_directory / wide_name;
        const HRESULT hr = D3DXCreateTextureFromFileW(device, candidate.c_str(), out_texture);
        if (SUCCEEDED(hr) && *out_texture) {
            return hr;
        }
    }

    return D3DXCreateTextureFromFileA(device, texture_filename.c_str(), out_texture);
}

int ResolveObjIndex(int raw_index, size_t count) {
    if (raw_index > 0) {
        const int resolved = raw_index - 1;
        return resolved >= 0 && static_cast<size_t>(resolved) < count ? resolved : -1;
    }
    if (raw_index < 0) {
        const int resolved = static_cast<int>(count) + raw_index;
        return resolved >= 0 && static_cast<size_t>(resolved) < count ? resolved : -1;
    }
    return -1;
}

bool ParseObjVertexRef(const std::string& token,
                       size_t position_count,
                       size_t texcoord_count,
                       size_t normal_count,
                       ObjVertexKey* out_key) {
    if (!out_key || token.empty()) {
        return false;
    }

    ObjVertexKey key{};
    std::stringstream ss(token);
    std::string item;
    int component_index = 0;
    while (std::getline(ss, item, '/')) {
        if (!item.empty()) {
            const int raw_index = std::stoi(item);
            if (component_index == 0) {
                key.position = ResolveObjIndex(raw_index, position_count);
            } else if (component_index == 1) {
                key.texcoord = ResolveObjIndex(raw_index, texcoord_count);
            } else if (component_index == 2) {
                key.normal = ResolveObjIndex(raw_index, normal_count);
            }
        }
        ++component_index;
    }

    if (key.position < 0) {
        return false;
    }

    *out_key = key;
    return true;
}

void ParseColorTriplet(const std::string& values, D3DCOLORVALUE* out_color) {
    if (!out_color) {
        return;
    }
    std::stringstream ss(values);
    ss >> out_color->r >> out_color->g >> out_color->b;
}

DWORD EnsureDefaultMaterial(std::vector<ObjMaterial>* materials,
                            std::unordered_map<std::string, DWORD>* material_lookup) {
    if (!materials || !material_lookup) {
        return 0;
    }

    if (materials->empty()) {
        ObjMaterial material;
        material.name = "default";
        materials->push_back(material);
        material_lookup->emplace(material.name, 0u);
    }

    return 0;
}

void LoadMaterialsFromMtl(const std::filesystem::path& mtl_path,
                          std::vector<ObjMaterial>* materials,
                          std::unordered_map<std::string, DWORD>* material_lookup) {
    if (!materials || !material_lookup) {
        return;
    }

    std::ifstream input(mtl_path);
    if (!input) {
        return;
    }

    ObjMaterial* current = nullptr;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::stringstream ss(trimmed);
        std::string keyword;
        ss >> keyword;
        std::string rest;
        std::getline(ss, rest);
        rest = TrimCopy(rest);

        if (keyword == "newmtl") {
            ObjMaterial material;
            material.name = rest.empty() ? "default" : rest;
            material.material = MakeDefaultMaterial();
            materials->push_back(material);
            const DWORD index = static_cast<DWORD>(materials->size() - 1);
            (*material_lookup)[material.name] = index;
            current = &materials->back();
            continue;
        }

        if (!current) {
            continue;
        }

        if (keyword == "Ka") {
            ParseColorTriplet(rest, &current->material.Ambient);
            current->has_ambient = true;
        } else if (keyword == "Kd") {
            ParseColorTriplet(rest, &current->material.Diffuse);
        } else if (keyword == "Ks") {
            ParseColorTriplet(rest, &current->material.Specular);
        } else if (keyword == "d") {
            current->material.Diffuse.a = std::stof(rest);
            current->material.Ambient.a = current->material.Diffuse.a;
        } else if (keyword == "Tr") {
            current->material.Diffuse.a = 1.f - std::stof(rest);
            current->material.Ambient.a = current->material.Diffuse.a;
        } else if (keyword == "Ns") {
            current->material.Power = std::stof(rest);
        } else if (keyword == "map_Kd") {
            current->texture_filename = rest;
        }
    }

    for (ObjMaterial& material : *materials) {
        if (!material.has_ambient) {
            material.material.Ambient = material.material.Diffuse;
        }
    }
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
    other.materials_.clear();
    other.textures_.clear();
}

ModelLoader& ModelLoader::operator=(ModelLoader&& other) noexcept {
    if (this != &other) {
        Release();
        mesh_ = other.mesh_;
        materials_ = std::move(other.materials_);
        textures_ = std::move(other.textures_);
        other.mesh_ = nullptr;
        other.materials_.clear();
        other.textures_.clear();
    }
    return *this;
}

void ModelLoader::Release() {
    for (IDirect3DTexture9* texture : textures_) {
        if (texture) {
            texture->Release();
        }
    }
    textures_.clear();
    materials_.clear();
    if (mesh_) {
        mesh_->Release();
        mesh_ = nullptr;
    }
}

HRESULT ModelLoader::LoadMeshFromX(IDirect3DDevice9* device, const wchar_t* x_file_path) {
    Release();
    if (!device || !x_file_path) {
        return E_INVALIDARG;
    }

    const std::filesystem::path source_path(x_file_path);
    const std::filesystem::path source_dir = source_path.parent_path();

    ID3DXBuffer* adjacency = nullptr;
    ID3DXBuffer* materials_buffer = nullptr;
    ID3DXBuffer* effect_instances = nullptr;
    ID3DXMesh* mesh = nullptr;
    DWORD num_materials = 0;

    constexpr HRESULT kD3dxParseError = static_cast<HRESULT>(0x88760390L);

    HRESULT hr = D3DXLoadMeshFromXW(x_file_path, D3DXMESH_MANAGED, device, &adjacency,
                                    &materials_buffer, &effect_instances, &num_materials, &mesh);

    if (hr == kD3dxParseError) {
        if (adjacency) {
            adjacency->Release();
            adjacency = nullptr;
        }
        if (materials_buffer) {
            materials_buffer->Release();
            materials_buffer = nullptr;
        }
        if (effect_instances) {
            effect_instances->Release();
            effect_instances = nullptr;
        }
        if (mesh) {
            mesh->Release();
            mesh = nullptr;
        }

        const std::filesystem::path sanitized_path = MakeSanitizedXPath(x_file_path);
        if (!sanitized_path.empty()) {
            hr = D3DXLoadMeshFromXW(sanitized_path.c_str(), D3DXMESH_MANAGED, device, &adjacency,
                                    &materials_buffer, &effect_instances, &num_materials, &mesh);
        }
    }

    if (adjacency) {
        adjacency->Release();
    }
    if (effect_instances) {
        effect_instances->Release();
    }

    if (FAILED(hr) || !mesh) {
        if (mesh) {
            mesh->Release();
        }
        if (materials_buffer) {
            materials_buffer->Release();
        }
        return FAILED(hr) ? hr : E_FAIL;
    }

    if (materials_buffer && num_materials > 0) {
        auto* d3dx_materials =
            static_cast<D3DXMATERIAL*>(materials_buffer->GetBufferPointer());
        materials_.resize(num_materials);
        textures_.assign(num_materials, nullptr);

        for (DWORD i = 0; i < num_materials; ++i) {
            materials_[i] = d3dx_materials[i].MatD3D;
            if (materials_[i].Ambient.r == 0.f && materials_[i].Ambient.g == 0.f &&
                materials_[i].Ambient.b == 0.f) {
                materials_[i].Ambient = materials_[i].Diffuse;
            }
            const std::string texture_name =
                d3dx_materials[i].pTextureFilename ? d3dx_materials[i].pTextureFilename : "";
            TryLoadTexture(device, texture_name, source_dir, &textures_[i]);
        }
    }

    if (materials_buffer) {
        materials_buffer->Release();
    }

    mesh_ = mesh;
    return S_OK;
}

HRESULT ModelLoader::LoadMeshFromObj(IDirect3DDevice9* device, const wchar_t* obj_file_path) {
    Release();
    if (!device || !obj_file_path) {
        return E_INVALIDARG;
    }

    const std::filesystem::path source_path(obj_file_path);
    const std::filesystem::path source_dir = source_path.parent_path();

    std::ifstream input(source_path);
    if (!input) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::vector<D3DXVECTOR3> positions;
    std::vector<D3DXVECTOR3> normals;
    std::vector<D3DXVECTOR2> texcoords;
    std::vector<ObjTriangle> triangles;

    std::vector<ObjMaterial> obj_materials;
    std::unordered_map<std::string, DWORD> material_lookup;
    DWORD current_material = EnsureDefaultMaterial(&obj_materials, &material_lookup);

    bool missing_normals = false;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = TrimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::stringstream ss(trimmed);
        std::string keyword;
        ss >> keyword;

        if (keyword == "v") {
            D3DXVECTOR3 position{};
            ss >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (keyword == "vn") {
            D3DXVECTOR3 normal{};
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (keyword == "vt") {
            D3DXVECTOR2 texcoord{};
            ss >> texcoord.x >> texcoord.y;
            texcoord.y = 1.f - texcoord.y;
            texcoords.push_back(texcoord);
        } else if (keyword == "mtllib") {
            std::string mtl_name;
            std::getline(ss, mtl_name);
            mtl_name = TrimCopy(mtl_name);
            if (!mtl_name.empty()) {
                LoadMaterialsFromMtl(source_dir / AnsiToWide(mtl_name.c_str()), &obj_materials,
                                     &material_lookup);
                EnsureDefaultMaterial(&obj_materials, &material_lookup);
            }
        } else if (keyword == "usemtl") {
            std::string material_name;
            std::getline(ss, material_name);
            material_name = TrimCopy(material_name);
            const auto it = material_lookup.find(material_name);
            current_material = it != material_lookup.end()
                ? it->second
                : EnsureDefaultMaterial(&obj_materials, &material_lookup);
        } else if (keyword == "f") {
            std::vector<ObjVertexKey> face_vertices;
            std::string token;
            while (ss >> token) {
                ObjVertexKey key{};
                if (!ParseObjVertexRef(token, positions.size(), texcoords.size(), normals.size(),
                                       &key)) {
                    return E_FAIL;
                }
                if (key.normal < 0) {
                    missing_normals = true;
                }
                face_vertices.push_back(key);
            }

            if (face_vertices.size() < 3) {
                continue;
            }

            for (size_t i = 1; i + 1 < face_vertices.size(); ++i) {
                ObjTriangle triangle{};
                triangle.vertices[0] = face_vertices[0];
                triangle.vertices[1] = face_vertices[i];
                triangle.vertices[2] = face_vertices[i + 1];
                triangle.material_index = current_material;
                triangles.push_back(triangle);
            }
        }
    }

    if (positions.empty() || triangles.empty()) {
        return E_FAIL;
    }

    std::unordered_map<ObjVertexKey, DWORD, ObjVertexKeyHash> vertex_lookup;
    std::vector<MeshVertex> vertices;
    std::vector<DWORD> indices;
    std::vector<DWORD> attributes;
    vertices.reserve(triangles.size() * 3);
    indices.reserve(triangles.size() * 3);
    attributes.reserve(triangles.size());

    auto intern_vertex = [&](const ObjVertexKey& key) -> DWORD {
        const auto it = vertex_lookup.find(key);
        if (it != vertex_lookup.end()) {
            return it->second;
        }

        MeshVertex vertex{};
        const D3DXVECTOR3& position = positions[static_cast<size_t>(key.position)];
        vertex.x = position.x;
        vertex.y = position.y;
        vertex.z = position.z;

        if (key.texcoord >= 0) {
            const D3DXVECTOR2& texcoord = texcoords[static_cast<size_t>(key.texcoord)];
            vertex.u = texcoord.x;
            vertex.v = texcoord.y;
        }

        if (key.normal >= 0) {
            const D3DXVECTOR3& normal = normals[static_cast<size_t>(key.normal)];
            vertex.nx = normal.x;
            vertex.ny = normal.y;
            vertex.nz = normal.z;
        }

        const DWORD index = static_cast<DWORD>(vertices.size());
        vertices.push_back(vertex);
        vertex_lookup.emplace(key, index);
        return index;
    };

    for (const ObjTriangle& triangle : triangles) {
        indices.push_back(intern_vertex(triangle.vertices[0]));
        indices.push_back(intern_vertex(triangle.vertices[1]));
        indices.push_back(intern_vertex(triangle.vertices[2]));
        attributes.push_back(triangle.material_index);
    }

    const DWORD mesh_options =
        D3DXMESH_MANAGED | (vertices.size() > 0xFFFFu ? D3DXMESH_32BIT : 0u);

    ID3DXMesh* mesh = nullptr;
    HRESULT hr = D3DXCreateMeshFVF(static_cast<DWORD>(triangles.size()),
                                   static_cast<DWORD>(vertices.size()), mesh_options, kMeshFvf,
                                   device, &mesh);
    if (FAILED(hr) || !mesh) {
        if (mesh) {
            mesh->Release();
        }
        return FAILED(hr) ? hr : E_FAIL;
    }

    void* vertex_data = nullptr;
    hr = mesh->LockVertexBuffer(0, &vertex_data);
    if (FAILED(hr)) {
        mesh->Release();
        return hr;
    }
    std::memcpy(vertex_data, vertices.data(), sizeof(MeshVertex) * vertices.size());
    mesh->UnlockVertexBuffer();

    void* index_data = nullptr;
    hr = mesh->LockIndexBuffer(0, &index_data);
    if (FAILED(hr)) {
        mesh->Release();
        return hr;
    }

    if (mesh_options & D3DXMESH_32BIT) {
        std::memcpy(index_data, indices.data(), sizeof(DWORD) * indices.size());
    } else {
        auto* out_indices = static_cast<WORD*>(index_data);
        for (size_t i = 0; i < indices.size(); ++i) {
            out_indices[i] = static_cast<WORD>(indices[i]);
        }
    }
    mesh->UnlockIndexBuffer();

    DWORD* attribute_data = nullptr;
    hr = mesh->LockAttributeBuffer(0, &attribute_data);
    if (FAILED(hr)) {
        mesh->Release();
        return hr;
    }
    std::memcpy(attribute_data, attributes.data(), sizeof(DWORD) * attributes.size());
    mesh->UnlockAttributeBuffer();

    if (missing_normals) {
        hr = D3DXComputeNormals(mesh, nullptr);
        if (FAILED(hr)) {
            mesh->Release();
            return hr;
        }
    }

    if (obj_materials.empty()) {
        EnsureDefaultMaterial(&obj_materials, &material_lookup);
    }

    materials_.resize(obj_materials.size());
    textures_.assign(obj_materials.size(), nullptr);
    for (size_t i = 0; i < obj_materials.size(); ++i) {
        materials_[i] = obj_materials[i].material;
        if (materials_[i].Ambient.r == 0.f && materials_[i].Ambient.g == 0.f &&
            materials_[i].Ambient.b == 0.f) {
            materials_[i].Ambient = materials_[i].Diffuse;
        }
        TryLoadTexture(device, obj_materials[i].texture_filename, source_dir, &textures_[i]);
    }

    mesh_ = mesh;
    return S_OK;
}

HRESULT ModelLoader::CreateBox(IDirect3DDevice9* device, float width, float height, float depth) {
    Release();
    if (!device) {
        return E_INVALIDARG;
    }

    ID3DXMesh* mesh = nullptr;
    const HRESULT hr = D3DXCreateBox(device, width, height, depth, &mesh, nullptr);
    if (FAILED(hr) || !mesh) {
        if (mesh) {
            mesh->Release();
        }
        return FAILED(hr) ? hr : E_FAIL;
    }

    mesh_ = mesh;
    return S_OK;
}

HRESULT ModelLoader::CreateSphere(IDirect3DDevice9* device, float radius, UINT slices,
                                  UINT stacks) {
    Release();
    if (!device) {
        return E_INVALIDARG;
    }

    ID3DXMesh* mesh = nullptr;
    const HRESULT hr = D3DXCreateSphere(device, radius, slices, stacks, &mesh, nullptr);
    if (FAILED(hr) || !mesh) {
        if (mesh) {
            mesh->Release();
        }
        return FAILED(hr) ? hr : E_FAIL;
    }

    mesh_ = mesh;
    return S_OK;
}

HRESULT ModelLoader::LoadFromFile(IDirect3DDevice9* device, const wchar_t* path) {
    if (!device || !path) {
        return E_INVALIDARG;
    }

    const std::string extension = ToLowerCopy(std::filesystem::path(path).extension().string());
    if (extension == ".obj") {
        return LoadMeshFromObj(device, path);
    }
    if (extension == ".x") {
        return LoadMeshFromX(device, path);
    }
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

void ModelLoader::Draw(IDirect3DDevice9* device) const {
    if (!device || !mesh_) {
        return;
    }

    device->SetFVF(mesh_->GetFVF());

    if (materials_.empty()) {
        D3DMATERIAL9 material = MakeDefaultMaterial();
        device->SetMaterial(&material);
        device->SetTexture(0, nullptr);
        mesh_->DrawSubset(0);
        return;
    }

    const DWORD subset_count = static_cast<DWORD>(materials_.size());
    for (DWORD i = 0; i < subset_count; ++i) {
        device->SetMaterial(&materials_[i]);
        device->SetTexture(0, textures_[i]);
        mesh_->DrawSubset(i);
    }
    device->SetTexture(0, nullptr);
}
