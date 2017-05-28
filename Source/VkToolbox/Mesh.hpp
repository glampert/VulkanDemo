#pragma once

// ================================================================================================
// File: VkToolbox/Mesh.hpp
// Author: Guilherme R. Lampert
// Created on: 03/04/17
// Brief: Mesh and vertex definition helpers.
// ================================================================================================

#include "VulkanContext.hpp"

#include <cstdint>
#include <vector>

namespace VkToolbox
{

#define VKTB_MESH_MODELS_PATH "Assets/Models/"

// 32 bits indexing to support large meshes.
using MeshIndex = std::uint32_t;
constexpr VkIndexType MeshVkIndexType = VK_INDEX_TYPE_UINT32;

struct Float2
{
    float x, y;
};

struct Float3
{
    float x, y, z;
};

struct Float4
{
    float x, y, z, w;
};

struct MeshVertex
{
    Float3 position;
    Float2 texcoords;
    Float4 color;
    Float3 normal;
    Float3 tangent;
    Float3 bitangent;

    // These must be in sync with the shader declarations in VSCommon.glsl!
    static constexpr int VSInLocationPosition  = 0;
    static constexpr int VSInLocationTexCoords = 1;
    static constexpr int VSInLocationColor     = 2;
    static constexpr int VSInLocationNormal    = 3;
    static constexpr int VSInLocationTangent   = 4;
    static constexpr int VSInLocationBitangent = 5;

    static const VkVertexInputBindingDescription & bindingDescription();
    static const std::array<VkVertexInputAttributeDescription, 6> & attributeDescriptions();
};

struct MeshSubSection
{
    int vertexStart;
    int vertexCount;
    int indexStart;
    int indexCount;
    int materialIndex;
};

struct MeshMaterialString
{
    static constexpr int MaxChars = 64;

    // Must be a POD for the binary load/saving.
    int  length;
    char chars[MaxChars];

    MeshMaterialString() : length{ 0 }
    {
        std::memset(chars, 0, MaxChars);
    }
    MeshMaterialString(const char * const src)
    {
        assert(std::strlen(src) < MaxChars);
        length = str::copy(chars, MaxChars, src);
    }
    MeshMaterialString & operator = (const char * const src)
    {
        assert(std::strlen(src) < MaxChars);
        length = str::copy(chars, MaxChars, src);
        return *this;
    }
};

struct MeshMaterial
{
    static constexpr int InvalidIndex = -1;

    int index = InvalidIndex;
    MeshMaterialString name;

    Color32 diffuseColor;
    Color32 specularColor;
    Color32 emissiveColor;
    Color32 ambientColor;
    float   shininess = 0.0f;

    MeshMaterialString diffuseTexture;
    MeshMaterialString normalTexture;
    MeshMaterialString specularTexture;

    void setDefaults();
};

struct MeshBinaryHeader
{
    std::uint32_t magic; // BMSH 4cc
    std::int32_t  vertexCount;
    std::int32_t  indexCount;
    std::int32_t  submeshCount;
    std::int32_t  materialCount;
    // Arrays of data follow in the same order.
};

class Mesh final
{
public:

    static const char * const BinaryFormatFileExt;

    std::vector<MeshVertex>     vertexes;
    std::vector<MeshIndex>      indexes;
    std::vector<MeshSubSection> submeshes;
    std::vector<MeshMaterial>   materials;

    int vertexCount()   const { return static_cast<int>(vertexes.size());  }
    int indexCount()    const { return static_cast<int>(indexes.size());   }
    int submeshCount()  const { return static_cast<int>(submeshes.size()); }
    int materialCount() const { return static_cast<int>(materials.size()); }

    Mesh() = default;

    Mesh(Mesh && other);
    Mesh & operator = (Mesh && other);

    Mesh(const Mesh &) = delete;
    Mesh & operator = (const Mesh &) = delete;

    bool initFromFile(const char * filePath, float vertexScaling = 1.0f);
    bool isInitialized() const;
    void shutdown();

    // Write/load the optimized binary representation from file.
    bool saveBinary(const char * filePath) const;
    bool loadBinary(const char * filePath);
};

// ========================================================

constexpr int BoxFaces    = 6;
constexpr int BoxVertexes = 24;
constexpr int BoxIndexes  = 36;

void createBoxMesh(const float   width,
                   const float   height,
                   const float   depth,
                   const Color32 inFaceColors[BoxFaces],
                   MeshVertex    outVertexes[BoxVertexes],
                   MeshIndex     outIndexes[BoxIndexes]);

// ========================================================

} // namespace VkToolbox
