
// ================================================================================================
// File: VkToolbox/Model3D.cpp
// Author: Guilherme R. Lampert
// Created on: 03/04/17
// Brief: Model3D definition.
// ================================================================================================

#include "Model3D.hpp"
#include "Utils.hpp"
#include "Pool.hpp"

namespace VkToolbox
{

// ========================================================
// class RenderStates: 
// ========================================================

RenderStates::RenderStates(const VulkanContext & vkContext)
    : renderProgram{ nullptr }
    , diffuseTexSlot{ TextureManager::InvalidResIndex }
    , normalTexSlot{ TextureManager::InvalidResIndex }
    , specularTexSlot{ TextureManager::InvalidResIndex }
    , diffuseSampler{ vkContext }
    , normalSampler{ vkContext }
    , specularSampler{ vkContext }
{
}

void RenderStates::initDescriptorSets()
{
    // TODO
}

void RenderStates::initPipelineState()
{
    // TODO
}

void RenderStates::setRenderProgram(const RenderProgram & renderProg)
{
    renderProgram = &renderProg;
    initDescriptorSets();
    initPipelineState();
}

// ========================================================
// class Model3D:
// ========================================================

float Model3D::sm_importerScale    = 1.0f;
bool  Model3D::sm_asyncTextureLoad = false;

Model3D::Model3D(const VulkanContext & vkContext, StrId<str> && id)
    : m_vkContext{ &vkContext }
    , m_resId{ std::move(id) }
    , m_drawData{ nullptr }
{
}

Model3D::Model3D(Model3D && other)
    : m_vkContext{ other.m_vkContext }
    , m_resId{ std::move(other.m_resId) }
    , m_drawData{ other.m_drawData }
    , m_mesh{ std::move(other.m_mesh) }
{
    other.clear();
}

Model3D & Model3D::operator = (Model3D && other)
{
    shutdown();

    m_vkContext = other.m_vkContext;
    m_resId     = std::move(other.m_resId);
    m_drawData  = other.m_drawData;
    m_mesh      = std::move(other.m_mesh);

    other.clear();
    return *this;
}

ModelDrawData * Model3D::allocateDrawData()
{
    const int materialCount = m_mesh.materialCount();
    const int submeshCount  = m_mesh.submeshCount();

    // Count the number of indexes we need to allocate for the material buckets:
    int indexesNeeded = 0;
    for (int mat = 0; mat < materialCount; ++mat)
    {
        for (int sm = 0; sm < submeshCount; ++sm)
        {
            if (m_mesh.submeshes[sm].materialIndex == mat)
            {
                assert(m_mesh.materials[mat].index == mat);
                ++indexesNeeded;
            }
        }
    }

    const auto drawDataSize   = sizeof(ModelDrawData);
    const auto matBucketsSize = materialCount * sizeof(MaterialSortIndexes);
    const auto matRsSize      = materialCount * sizeof(RenderStates);
    const auto indexesSize    = indexesNeeded * sizeof(std::uint16_t);
    const auto totalSize      = drawDataSize + matBucketsSize + matRsSize + indexesSize;

    auto * blob = static_cast<std::uint8_t *>(std::malloc(totalSize));

    auto * newDrawData = construct(reinterpret_cast<ModelDrawData *>(blob), *m_vkContext);
    blob += drawDataSize;

    newDrawData->materialDrawBuckets = reinterpret_cast<MaterialSortIndexes *>(blob);
    blob += matBucketsSize;

    newDrawData->materialRenderStates = reinterpret_cast<RenderStates *>(blob);
    blob += matRsSize;

    auto * idxPtr = reinterpret_cast<std::uint16_t *>(blob);
    newDrawData->submeshIndexes = idxPtr;

    // Sort the submeshes by material:
    for (int mat = 0; mat < materialCount; ++mat)
    {
        construct(&newDrawData->materialRenderStates[mat], *m_vkContext);

        MaterialSortIndexes si;
        si.submeshCount   = 0;
        si.submeshIndexes = idxPtr;

        for (int sm = 0; sm < submeshCount; ++sm)
        {
            if (m_mesh.submeshes[sm].materialIndex == mat)
            {
                assert(m_mesh.materials[mat].index == mat);
                si.submeshIndexes[si.submeshCount++] = narrowCast<std::uint16_t>(sm);
            }
        }

        newDrawData->materialDrawBuckets[mat] = si;
        idxPtr += si.submeshCount;
    }

    deleteDrawData();
    m_drawData = newDrawData;
    return m_drawData;
}

void Model3D::deleteDrawData()
{
    if (m_drawData != nullptr)
    {
        // Draw data was placement constructed in a malloc'd block,
        // so we have to manually call the appropriate destructors.
        const int materialCount = m_mesh.materialCount();
        for (int mat = 0; mat < materialCount; ++mat)
        {
            destroy(&m_drawData->materialRenderStates[mat]);
        }
        destroy(m_drawData);

        std::free(m_drawData);
        m_drawData = nullptr;
    }
}

bool Model3D::load()
{
    if (isShutdown())
    {
        Log::warningF("Model3D is already shutdown and cannot be loaded!");
        return false;
    }

    if (!m_mesh.initFromFile(resourceId().c_str(), sm_importerScale))
    {
        return false;
    }

    ModelDrawData & dd = *allocateDrawData();

    // Reference the required textures:
    for (MeshMaterial & mat : m_mesh.materials)
    {
        str256 texName;

        TextureManager::ResourceIndex diffuseTexSlot;
        texName.setf("%s%s", VKTB_TEXTURES_PATH, mat.diffuseTexture.chars);
        TextureManager::registerSlot(TextureManager::ResourceId{ std::move(texName) }, &diffuseTexSlot);

        TextureManager::ResourceIndex normalTexSlot;
        texName.setf("%s%s", VKTB_TEXTURES_PATH, mat.normalTexture.chars);
        TextureManager::registerSlot(TextureManager::ResourceId{ std::move(texName) }, &normalTexSlot);

        TextureManager::ResourceIndex specularTexSlot;
        texName.setf("%s%s", VKTB_TEXTURES_PATH, mat.specularTexture.chars);
        TextureManager::registerSlot(TextureManager::ResourceId{ std::move(texName) }, &specularTexSlot);

        // TODO:
        // - Proper sampler setup
        // - Texture arrays handling

        //dd.samplers.push(Sampler{ *m_vkContext, Sampler::defaults() });
        //dd.samplers.push(Sampler{ *m_vkContext, Sampler::defaults() });
        //dd.samplers.push(Sampler{ *m_vkContext, Sampler::defaults() });

        if (!sm_asyncTextureLoad) // Load immediately
        {
            if (!TextureManager::isLoaded(diffuseTexSlot))  { TextureManager::reload(diffuseTexSlot);  }
            if (!TextureManager::isLoaded(normalTexSlot))   { TextureManager::reload(normalTexSlot);   }
            if (!TextureManager::isLoaded(specularTexSlot)) { TextureManager::reload(specularTexSlot); }
        }
    }

    /*
    if (sm_asyncTextureLoad)
    {
        // Push the deferred load requests, must wait for completion!
        for (auto texSlot : dd.textureSlots)
        {
            if (!TextureManager::isLoaded(texSlot) &&
                !TextureManager::hasPendingAsyncLoadRequest(texSlot))
            {
                TextureManager::pushAsyncLoadRequest(texSlot);
            }
        }
    }
    */

    // Set up the vertex and index buffers:
    dd.buffersUploaded = false;

    dd.vertexBuffer.initialize(m_mesh.vertexCount());
    dd.indexBuffer.initialize(m_mesh.indexCount());

    dd.vertexBuffer.writeN(make_array_view(m_mesh.vertexes));
    dd.indexBuffer.writeN(make_array_view(m_mesh.indexes));

    assert(dd.vertexBuffer.elementCount() == m_mesh.vertexCount());
    assert(dd.indexBuffer.elementCount()  == m_mesh.indexCount());

    return true;
}

void Model3D::unload()
{
    deleteDrawData();
    m_mesh.shutdown();
}

void Model3D::clear()
{
    m_drawData  = nullptr;
    m_vkContext = nullptr;
    m_resId     = {};
}

void Model3D::shutdown()
{
    unload();
    clear();
}

void Model3D::uploadBuffersToGpu(const CommandBuffer & cmdBuff, const bool force) const
{
    if (!isLoaded())
    {
        return;
    }

    ModelDrawData & dd = *m_drawData;
    if (dd.buffersUploaded && !force)
    {
        return;
    }

    dd.vertexBuffer.uploadStagingToGpu(cmdBuff);
    dd.indexBuffer.uploadStagingToGpu(cmdBuff);
    dd.buffersUploaded = true;
}

void Model3D::drawInstance(const CommandBuffer & cmdBuff) const
{
    if (!isLoaded())
    {
        return;
    }

    ModelDrawData & dd = *m_drawData;
    m_vkContext->bindVertexBuffer(cmdBuff, dd.vertexBuffer);
    m_vkContext->bindIndexBuffer(cmdBuff, dd.indexBuffer, vkIndexTypeForBuffer(dd.indexBuffer));

    const int materialCount = m_mesh.materialCount();
    for (int mat = 0; mat < materialCount; ++mat)
    {
        setRenderStates(dd.materialRenderStates[mat]);

        // Draw all submeshes affected by the material:
        const int submeshCount = dd.materialDrawBuckets[mat].submeshCount;
        for (int sm = 0; sm < submeshCount; ++sm)
        {
            const std::size_t idx = dd.materialDrawBuckets[mat].submeshIndexes[sm];
            const MeshSubSection & submesh = m_mesh.submeshes[idx];

            m_vkContext->drawIndexed(cmdBuff, submesh.indexCount, 1, submesh.indexStart, 0, 0);
        }
    }
}

void Model3D::setRenderStates(const RenderStates & rs) const
{
    (void)rs;
    // TODO
}

} // namespace VkToolbox
