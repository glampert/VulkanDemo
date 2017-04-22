
// ================================================================================================
// File: VkToolbox/Mesh.cpp
// Author: Guilherme R. Lampert
// Created on: 03/04/17
// Brief: Mesh and vertex definition helpers.
// ================================================================================================

#include "Mesh.hpp"
#include <cstddef> // For offsetof()
#include <cstring> // For mem-ops

// Asset importer lib:
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/cimport.h" 
#include "assimp/scene.h"

// ASSIMP log hook only enabled on debug.
#if DEBUG
    #include <mutex>
    #define VKTB_REGISTER_ASSIMP_LOG_CALLBACK 1
    static std::once_flag s_assimpLogHookRegistered;
#else // !DEBUG
    #define VKTB_REGISTER_ASSIMP_LOG_CALLBACK 0
#endif // DEBUG

// If the ASSIMP log hook is enabled, this controls the verbosity.
#define VKTB_VERBOSE_ASSIMP_LOGGING 1

namespace VkToolbox
{

// ========================================================
// MeshVertex:
// ========================================================

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
// ASSIMP importer helpers:
// ========================================================

static void assimpImportPreallocMeshArrays(const aiScene & inScene, Mesh & outMesh)
{
    const int submeshCount  = inScene.mNumMeshes;
    const int materialCount = inScene.mNumMaterials;

    int vertexCount = 0;
    int indexCount  = 0;
    for (int m = 0; m < submeshCount; ++m)
    {
        vertexCount += inScene.mMeshes[m]->mNumVertices;
        indexCount  += inScene.mMeshes[m]->mNumFaces * 3; // Always triangles (aiProcess_Triangulate)
    }

    Log::debugF("Mesh has %i verts, %i indexes, %i sub-meshes, %i materials.",
                vertexCount, indexCount, submeshCount, materialCount);

    outMesh.vertexes.resize(vertexCount);
    outMesh.indexes.resize(indexCount);
    outMesh.submeshes.resize(submeshCount);
    outMesh.materials.resize(materialCount);
}

static void assimpImportCopyVerts(const aiMesh & inSubmesh, const float vertexScaling, MeshVertex * outVerts)
{
    assert(inSubmesh.HasPositions());

    const aiVector3D nullVector{ 0.0f, 0.0f, 0.0f };
    const aiColor4D  whiteColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    const int count = inSubmesh.mNumVertices;
    for (int v = 0; v < count; ++v)
    {
        const aiVector3D P = inSubmesh.mVertices[v];
        const aiVector3D U = inSubmesh.HasTextureCoords(0)        ? inSubmesh.mTextureCoords[0][v] : nullVector;
        const aiColor4D  C = inSubmesh.HasVertexColors(0)         ? inSubmesh.mColors[0][v]        : whiteColor;
        const aiVector3D N = inSubmesh.HasNormals()               ? inSubmesh.mNormals[v]          : nullVector;
        const aiVector3D T = inSubmesh.HasTangentsAndBitangents() ? inSubmesh.mTangents[v]         : nullVector;
        const aiVector3D B = inSubmesh.HasTangentsAndBitangents() ? inSubmesh.mBitangents[v]       : nullVector;

        MeshVertex & vert = outVerts[v];
        vert.position.x   = P.x * vertexScaling;
        vert.position.y   = P.y * vertexScaling;
        vert.position.z   = P.z * vertexScaling;
        vert.texcoords.x  = U.x;
        vert.texcoords.y  = U.y;
        vert.color.x      = C.r;
        vert.color.y      = C.g;
        vert.color.z      = C.b;
        vert.color.w      = C.a;
        vert.normal.x     = N.x;
        vert.normal.y     = N.y;
        vert.normal.z     = N.z;
        vert.tangent.x    = T.x;
        vert.tangent.y    = T.y;
        vert.tangent.z    = T.z;
        vert.bitangent.x  = B.x;
        vert.bitangent.y  = B.y;
        vert.bitangent.z  = B.z;
    }
}

static void assimpImportCopyIndexes(const aiMesh & inSubmesh, const MeshIndex baseVertex, MeshIndex * outIndexes)
{
    const int count = inSubmesh.mNumFaces;
    for (int t = 0; t < count; ++t)
    {
        const aiFace & face = inSubmesh.mFaces[t];
        assert(face.mNumIndices == 3);

        *outIndexes++ = face.mIndices[0] + baseVertex;
        *outIndexes++ = face.mIndices[1] + baseVertex;
        *outIndexes++ = face.mIndices[2] + baseVertex;
    }
}

static void assimpImportMeshMain(const aiScene & inScene, const float vertexScaling, Mesh & outMesh)
{
    assimpImportPreallocMeshArrays(inScene, outMesh);

    //
    // Sub-meshes import:
    //
    int submeshVertStart = 0;
    int submeshIdxStart  = 0;

    const int submeshCount = inScene.mNumMeshes;
    const aiMesh * const * inSubmeshes = inScene.mMeshes;

    for (int m = 0; m < submeshCount; ++m)
    {
        const int submeshVertCount = inSubmeshes[m]->mNumVertices;
        const int submeshIdxCount  = inSubmeshes[m]->mNumFaces * 3; // Always triangles (aiProcess_Triangulate)

        MeshVertex * destVerts = outMesh.vertexes.data();
        assimpImportCopyVerts(*inSubmeshes[m], vertexScaling, &destVerts[submeshVertStart]);

        MeshIndex * destIndexes = outMesh.indexes.data();
        assimpImportCopyIndexes(*inSubmeshes[m], submeshVertStart, &destIndexes[submeshIdxStart]);

        outMesh.submeshes[m] = MeshSubSection{
            submeshVertStart, submeshVertCount,
            submeshIdxStart,  submeshIdxCount,
            MeshMaterial::InvalidIndex
        };

        submeshVertStart += submeshVertCount;
        submeshIdxStart  += submeshIdxCount;
    }

    //
    // Materials import:
    //
    const int materialCount = inScene.mNumMaterials;
    const aiMaterial * const * inMaterials = inScene.mMaterials;

    for (int m = 0; m < materialCount; ++m)
    {
        // TODO!
    }
}

// ========================================================
// Mesh:
// ========================================================

bool Mesh::initFromFile(const char * const filePath, const float vertexScaling)
{
    assert(filePath != nullptr && filePath[0] != '\0');
    if (!probeFile(filePath))
    {
        Log::warningF("Mesh file '%s' does not exist! Can't load mesh from it.", filePath);
        return false;
    }

    // Optional ASSIMP log hook:
    #if VKTB_REGISTER_ASSIMP_LOG_CALLBACK
    std::call_once(s_assimpLogHookRegistered, []() {
        // Direct all logging to stdout.
        aiEnableVerboseLogging(VKTB_VERBOSE_ASSIMP_LOGGING);
        auto logStream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT, nullptr);
        aiAttachLogStream(&logStream);
    });
    #endif // VKTB_REGISTER_ASSIMP_LOG_CALLBACK

    // Default ASSIMP post processing flags used.
    constexpr unsigned PostProcessingFlags = (aiProcess_Triangulate              |
                                              aiProcess_JoinIdenticalVertices    |
                                              aiProcess_RemoveRedundantMaterials |
                                              aiProcess_OptimizeMeshes           |
                                              aiProcess_OptimizeGraph            |
                                              aiProcess_ImproveCacheLocality     |
                                              aiProcess_GenSmoothNormals         |
                                              aiProcess_CalcTangentSpace         |
                                              aiProcess_SplitLargeMeshes);

    Assimp::Importer importer;
    const aiScene * scene = importer.ReadFile(filePath, PostProcessingFlags);

    if (scene == nullptr)
    {
        Log::warningF("Failed to import ASSIMP scene from file '%s'.", filePath);
        return false;
    }
    if (!scene->HasMeshes())
    {
        Log::warningF("Empty scene from ASSIMP import '%s'!", filePath);
        return false;
    }

    // Shutdown in case the Mesh object is being recycled.
    if (isInitialized())
    {
        shutdown();
    }

    assimpImportMeshMain(*scene, vertexScaling, *this);

    Log::debugF("Mesh '%s' imported successfully.", filePath);
    return true;
}

bool Mesh::isInitialized() const
{
    return (!vertexes.empty() && !indexes.empty() && !submeshes.empty());
}

void Mesh::shutdown()
{
    vertexes.clear();
    indexes.clear();
    submeshes.clear();
    materials.clear();
}

// ========================================================
// Miscellaneous:
// ========================================================

void createBoxMesh(const float   width,
                   const float   height,
                   const float   depth,
                   const Color32 inFaceColors[BoxFaces],
                   MeshVertex    outVertexes[BoxVertexes],
                   MeshIndex     outIndexes[BoxIndexes])
{
    //
    // -0.5,+0.5 indexed box, face winding order is CW:
    //
    static const MeshIndex s_boxFaces[][4] = {
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
    MeshVertex * vertexPtr = outVertexes;
    MeshIndex  * facePtr   = outIndexes;

    // 'i' iterates over the faces, 2 triangles per face:
    MeshIndex vertIndex = 0;
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
