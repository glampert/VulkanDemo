#pragma once

// ================================================================================================
// File: VkToolbox/Mesh.cpp
// Author: Guilherme R. Lampert
// Created on: 03/04/17
// Brief: Mesh and vertex definition helpers.
// ================================================================================================

#include "Mesh.hpp"

namespace VkToolbox
{

const VkVertexInputBindingDescription & MeshVertex::bindingDescription()
{
    static const VkVertexInputBindingDescription s_bindingDescription = {
        /* binding   = */ 0,
        /* stride    = */ sizeof(MeshVertex),
        /* inputRate = */ VK_VERTEX_INPUT_RATE_VERTEX,
    };
    return s_bindingDescription;
}

const std::array<VkVertexInputAttributeDescription, 6> & MeshVertex::attributeDescriptions()
{
    static const std::array<VkVertexInputAttributeDescription, 6> s_attributeDescriptions = {{
        {
        /* location = */ VSInLocationPosition,
        /* binding  = */ 0,
        /* format   = */ VK_FORMAT_R32G32B32_SFLOAT,
        /* offset   = */ offsetof(MeshVertex, position),
        },
        {
        /* location = */ VSInLocationTexCoords,
        /* binding  = */ 0,
        /* format   = */ VK_FORMAT_R32G32_SFLOAT,
        /* offset   = */ offsetof(MeshVertex, texcoords),
        },
        {
        /* location = */ VSInLocationColor,
        /* binding  = */ 0,
        /* format   = */ VK_FORMAT_R32G32B32A32_SFLOAT,
        /* offset   = */ offsetof(MeshVertex, color),
        },
        {
        /* location = */ VSInLocationNormal,
        /* binding  = */ 0,
        /* format   = */ VK_FORMAT_R32G32B32_SFLOAT,
        /* offset   = */ offsetof(MeshVertex, normal),
        },
        {
        /* location = */ VSInLocationTangent,
        /* binding  = */ 0,
        /* format   = */ VK_FORMAT_R32G32B32_SFLOAT,
        /* offset   = */ offsetof(MeshVertex, tangent),
        },
        {
        /* location = */ VSInLocationBitangent,
        /* binding  = */ 0,
        /* format   = */ VK_FORMAT_R32G32B32_SFLOAT,
        /* offset   = */ offsetof(MeshVertex, bitangent),
        },
    }};
    return s_attributeDescriptions;
}

// ========================================================

void createBoxMesh(const float   width,
                   const float   height,
                   const float   depth,
                   const Color32 inFaceColors[BoxFaces],
                   MeshVertex    outVertexes[BoxVertexes],
                   std::uint16_t outIndexes[BoxIndexes])
{
    //
    // -0.5,+0.5 indexed box:
    //
    static const std::uint16_t s_boxFaces[][4] = {
        { 0, 1, 5, 4 },
        { 4, 5, 6, 7 },
        { 7, 6, 2, 3 },
        { 1, 0, 3, 2 },
        { 1, 2, 6, 5 },
        { 0, 4, 7, 3 },
    };
    static const float s_boxPositions[][3] = {
        { -0.5f, -0.5f, -0.5f },
        { -0.5f, -0.5f,  0.5f },
        {  0.5f, -0.5f,  0.5f },
        {  0.5f, -0.5f, -0.5f },
        { -0.5f,  0.5f, -0.5f },
        { -0.5f,  0.5f,  0.5f },
        {  0.5f,  0.5f,  0.5f },
        {  0.5f,  0.5f, -0.5f },
    };
    static const float s_boxNormals[][3] = {
        { -1.0f,  0.0f,  0.0f },
        {  0.0f,  1.0f,  0.0f },
        {  1.0f,  0.0f,  0.0f },
        {  0.0f, -1.0f,  0.0f },
        {  0.0f,  0.0f,  1.0f },
        {  0.0f,  0.0f, -1.0f },
    };
    static const float s_boxTexCoords[][2] = {
        { 0.0f, 1.0f },
        { 1.0f, 1.0f },
        { 1.0f, 0.0f },
        { 0.0f, 0.0f },
    };

    // Fill in the data:
    MeshVertex    * vertexPtr = outVertexes;
    std::uint16_t * facePtr   = outIndexes;

    // 'i' iterates over the faces, 2 triangles per face:
    std::uint16_t vertIndex = 0;
    for (int i = 0; i < 6; ++i, vertIndex += 4)
    {
        for (int j = 0; j < 4; ++j)
        {
            vertexPtr->position.x = s_boxPositions[s_boxFaces[i][j]][0] * width;
            vertexPtr->position.y = s_boxPositions[s_boxFaces[i][j]][1] * height;
            vertexPtr->position.z = s_boxPositions[s_boxFaces[i][j]][2] * depth;

            vertexPtr->texcoords.x = s_boxTexCoords[j][0];
            vertexPtr->texcoords.y = s_boxTexCoords[j][1];

            vertexPtr->color.x = inFaceColors[i].floatR();
            vertexPtr->color.y = inFaceColors[i].floatG();
            vertexPtr->color.z = inFaceColors[i].floatB();
            vertexPtr->color.w = inFaceColors[i].floatA();

            vertexPtr->normal.x = s_boxNormals[i][0];
            vertexPtr->normal.y = s_boxNormals[i][1];
            vertexPtr->normal.z = s_boxNormals[i][2];

            // Not set - zeroed out.
            vertexPtr->tangent   = {};
            vertexPtr->bitangent = {};

            ++vertexPtr;
        }

        facePtr[0] = vertIndex;
        facePtr[1] = vertIndex + 1;
        facePtr[2] = vertIndex + 2;
        facePtr += 3;

        facePtr[0] = vertIndex + 2;
        facePtr[1] = vertIndex + 3;
        facePtr[2] = vertIndex;
        facePtr += 3;
    }
}

// ========================================================

} // namespace VkToolbox
