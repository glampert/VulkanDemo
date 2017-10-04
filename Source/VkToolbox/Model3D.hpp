#pragma once

// ================================================================================================
// File: VkToolbox/Model3D.hpp
// Author: Guilherme R. Lampert
// Created on: 03/04/17
// Brief: Model3D definition.
// ================================================================================================

#include "Buffers.hpp"
#include "DescriptorSets.hpp"
#include "FixedSizeArray.hpp"
#include "GlslShader.hpp"
#include "Mesh.hpp"
#include "PipelineState.hpp"
#include "ResourceManager.hpp"
#include "Texture.hpp"
#include "VulkanContext.hpp"

namespace VkToolbox
{

// ========================================================

class RenderProgram final
{
public:

    explicit RenderProgram(const VulkanContext & vkContext)
        : shaderSlot{ GlslShaderManager::InvalidResIndex }
        , descriptorSetPool{ vkContext }
        , descriptorSetLayout{ vkContext }
        , pipelineStateLayout{ vkContext }
    { }

    // Handle to the shader instance:
    GlslShaderManager::ResourceIndex shaderSlot;

    // Shader pipeline layouts:
    DescriptorSetPool   descriptorSetPool;
    DescriptorSetLayout descriptorSetLayout;
    PipelineStateLayout pipelineStateLayout;
};

// ========================================================

class RenderStates final
{
public:

    explicit RenderStates(const VulkanContext & vkContext);
    void setRenderProgram(const RenderProgram & renderProg);

    const RenderProgram * renderProgram;

    DescriptorSets<1>   descriptorSets;
    PipelineStateObject pipelineStateObject;

    TextureManager::ResourceIndex diffuseTexSlot;
    TextureManager::ResourceIndex normalTexSlot;
    TextureManager::ResourceIndex specularTexSlot;

    Sampler diffuseSampler;
    Sampler normalSampler;
    Sampler specularSampler;

private:

    void initDescriptorSets();
    void initPipelineState();
};

// ========================================================

using MeshVB = StructuredVertexBuffer<MeshVertex>;
using MeshIB = StructuredIndexBuffer<MeshIndex>;

struct MaterialSortIndexes
{
    int submeshCount;
    std::uint16_t * submeshIndexes;
};

struct ModelDrawData
{
    mutable bool          buffersUploaded;
    MeshVB                vertexBuffer;
    MeshIB                indexBuffer;
    RenderStates        * materialRenderStates; // [num materials]
    MaterialSortIndexes * materialDrawBuckets;  // [num materials]
    std::uint16_t       * submeshIndexes;       // [num submesh indexes]

    explicit ModelDrawData(const VulkanContext & vkContext)
        : buffersUploaded{ false }
        , vertexBuffer{ vkContext }
        , indexBuffer{ vkContext }
        , materialRenderStates{ nullptr }
        , materialDrawBuckets{ nullptr }
        , submeshIndexes{ nullptr }
    { }
};

// ========================================================

class Model3D final
{
public:

    static float sm_importerScale;
    static bool  sm_asyncTextureLoad;

    Model3D(const VulkanContext & vkContext, const char * id);
    Model3D(const VulkanContext & vkContext, StrId<str> && id);
    ~Model3D();

    // Movable.
    Model3D(Model3D && other);
    Model3D & operator = (Model3D && other);

    // Not copyable.
    Model3D(const Model3D &) = delete;
    Model3D & operator = (const Model3D &) = delete;

    // Resourcing methods:
    bool load();
    void unload();
    void shutdown();
    bool isLoaded() const;
    bool isShutdown() const;
    const VulkanContext & context() const;
    const StrId<str> & resourceId() const;

    // Accessors:
    const Mesh & mesh() const;
    const ModelDrawData & drawData() const;

    // Draw handlers:
    void uploadBuffersToGpu(const CommandBuffer & cmdBuff, bool force = false) const;
    void drawInstance(const CommandBuffer & cmdBuff) const;

private:

    void setRenderStates(const RenderStates & rs) const;
    ModelDrawData * allocateDrawData();
    void deleteDrawData();
    void clear();

private:

    const VulkanContext * m_vkContext;
    StrId<str> m_resId;

    ModelDrawData * m_drawData;
    Mesh m_mesh;
};

// ========================================================

inline Model3D::Model3D(const VulkanContext & vkContext, const char * const id)
    : Model3D{ vkContext, StrId<str>{ id } }
{
}

inline Model3D::~Model3D()
{
    deleteDrawData();
}

inline bool Model3D::isLoaded() const
{
    return (m_drawData != nullptr);
}

inline bool Model3D::isShutdown() const
{
    return (m_vkContext == nullptr);
}

inline const VulkanContext & Model3D::context() const
{
    return *m_vkContext;
}

inline const StrId<str> & Model3D::resourceId() const
{
    return m_resId;
}

inline const Mesh & Model3D::mesh() const
{
    return m_mesh;
}

inline const ModelDrawData & Model3D::drawData() const
{
    assert(m_drawData != nullptr);
    return *m_drawData;
}

// ========================================================

} // namespace VkToolbox
