#pragma once

// ================================================================================================
// File: VkToolbox/Mesh.hpp
// Author: Guilherme R. Lampert
// Created on: 03/04/17
// Brief: Mesh and vertex definition helpers.
// ================================================================================================

#include "VulkanContext.hpp"
#include <cstddef> // For offsetof()

namespace VkToolbox
{

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

constexpr int BoxVertexes = 24;
constexpr int BoxIndexes  = 36;

void createBoxMesh(const float   width,
                   const float   height,
                   const float   depth,
                   const Color32 inFaceColors[6],
                   MeshVertex    outVertexes[BoxVertexes],
                   std::uint16_t outIndexes[BoxIndexes]);

} // namespace VkToolbox
