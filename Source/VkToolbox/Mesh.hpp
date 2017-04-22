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

struct MeshVertex final
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

struct MeshSubSection final
{
    int vertexStart;
    int vertexCount;
    int indexStart;
    int indexCount;
    int materialIndex;
};

struct MeshMaterial final
{
    static constexpr int InvalidIndex = -1;

    //TODO
};

struct Mesh final
{
    std::vector<MeshVertex>     vertexes;
    std::vector<MeshIndex>      indexes;
    std::vector<MeshSubSection> submeshes;
    std::vector<MeshMaterial>   materials;

    int vertexCount()   const { return static_cast<int>(vertexes.size());  }
    int indexCount()    const { return static_cast<int>(indexes.size());   }
    int submeshCount()  const { return static_cast<int>(submeshes.size()); }
    int materialCount() const { return static_cast<int>(materials.size()); }

    Mesh() = default;
    Mesh(const Mesh & other) = delete;
    Mesh & operator = (const Mesh & other) = delete;

    bool initFromFile(const char * filePath, float vertexScaling = 1.0f);
    bool isInitialized() const;
    void shutdown();
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
