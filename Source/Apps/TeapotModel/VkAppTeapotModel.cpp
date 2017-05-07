
// ================================================================================================
// File: Apps/TeapotModel/VkAppTeapotModel.cpp
// Author: Guilherme R. Lampert
// Created on: 09/04/17
// Brief: Draws a teapot model that was loaded from file.
// ================================================================================================

#include "Apps/VulkanDemoApp.hpp"
#include "VkToolbox/DescriptorSets.hpp"
#include "VkToolbox/PipelineState.hpp"
#include "VkToolbox/GlslShader.hpp"
#include "VkToolbox/Buffers.hpp"
#include "VkToolbox/Texture.hpp"
#include "VkToolbox/Mesh.hpp"
#include "VkToolbox/Input.hpp"

using namespace VkToolbox;

// ========================================================
// class VkAppTeapotModel:
// ========================================================

class VkAppTeapotModel final
    : public VulkanDemoApp
{
private:

    using BufferPtr = std::unique_ptr<CommandBuffer>;
    using FencePtr  = std::unique_ptr<Fence>;

    // One buffer for each frame in flight 
    //(assuming the context was created with triple buffering).
    CommandPool              m_cmdPool;
    std::array<BufferPtr, 3> m_cmdBuffers;
    std::array<FencePtr,  3> m_cmdBufferFences;
    CommandBuffer *          m_currentCmdBuffer      = nullptr;
    Fence *                  m_currentCmdBufferFence = nullptr;
    int                      m_nextCmdBufferIndex    = 0;

    // Shading pipeline state:
    const char *             m_shaderFilename = VKTB_SHADER_SOURCE_PATH "Teapot.glsl";
    GlslShader               m_shaderProgram;
    DescriptorSetPool        m_descriptorSetPool;
    DescriptorSetLayout      m_descriptorSetLayout;
    DescriptorSets<1>        m_descriptorSet;
    PipelineStateLayout      m_pipelineStateLayout;
    PipelineStateObject      m_pipelineState;

    struct Matrices
    {
        Matrix4 model        = Matrix4::identity();
        Matrix4 view         = Matrix4::identity();
        Matrix4 projection   = Matrix4::identity();
        Matrix4 mvp          = Matrix4::identity();
    };

    Matrices                                m_matrices;
    StructuredShaderUniformBuffer<Matrices> m_uniformBuffer;
    StructuredVertexBuffer<MeshVertex>      m_vertexBuffer;
    StructuredIndexBuffer<MeshIndex>        m_indexBuffer;

    // Texture/sampler/mesh:
    const char * m_textureName = VKTB_TEXTURES_PATH "Common/default.png";
    Texture      m_texture;
    Sampler      m_sampler;
    Mesh         m_mesh;

private:

    void initDescriptorSets();
    void initPipeline();
    void initTexture();
    void initVertexBuffer();

    void updateBuffers(CommandBuffer & cmdBuff);
    void prepareCommandBuffer(CommandBuffer & cmdBuff);

public:

    VkAppTeapotModel(const StartupOptions & options);
    void onFrameUpdate() override;
};

// ========================================================

VULKAN_DEMO_REGISTER_APP(VkAppTeapotModel);

// ========================================================

VkAppTeapotModel::VkAppTeapotModel(const StartupOptions & options)
    : VulkanDemoApp{ options }
    , m_cmdPool{ context() }
    , m_shaderProgram{ context(), m_shaderFilename }
    , m_descriptorSetPool{ context() }
    , m_descriptorSetLayout{ context() }
    , m_pipelineStateLayout{ context() }
    , m_pipelineState{ m_pipelineStateLayout }
    , m_uniformBuffer{ context() }
    , m_vertexBuffer{ context() }
    , m_indexBuffer{ context() }
    , m_texture{ context(), m_textureName }
    , m_sampler{ context() }
{
    m_shaderProgram.load();
    m_uniformBuffer.initialize(1);
    m_cmdPool.initialize(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context().graphisQueue().familyIndex);

    for (std::size_t i = 0; i < m_cmdBuffers.size(); ++i)
    {
        m_cmdBuffers[i].reset(new CommandBuffer{ context(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_cmdPool });
    }
    m_currentCmdBuffer = m_cmdBuffers[0].get();

    for (std::size_t i = 0; i < m_cmdBufferFences.size(); ++i)
    {
        m_cmdBufferFences[i].reset(new Fence{ context(), VK_FENCE_CREATE_SIGNALED_BIT });
    }
    m_currentCmdBufferFence = m_cmdBufferFences[0].get();

    initTexture();
    initDescriptorSets();
    initPipeline();
    initVertexBuffer();

    context().setDefaultClearColor({ 0.4f, 0.4f, 0.4f, 1.0f });

    // Quit on [ESCAPE]
    window().onVirtKeyPress = [this](unsigned keyCode)
    {
        if (keyCode == Key::VK_ESCAPE)
        {
            window().setStopEventLoop(true);
        }
    };
}

void VkAppTeapotModel::initDescriptorSets()
{
    // Pool:
    const VkDescriptorPoolSize descriptorPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1 }, // uniform matrices
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // uniform textureSampler
    };
    m_descriptorSetPool.initialize(1, make_array_view(descriptorPoolSizes));

    // Set layout:
    VkDescriptorSetLayoutBinding uboLayoutBindings[2] = {};
    uboLayoutBindings[0].binding         = 0; // layout(binding = 0)
    uboLayoutBindings[0].descriptorCount = 1; // Matching 'matrices' declaration in the VS
    uboLayoutBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBindings[1].binding         = 1; // layout(binding = 1)
    uboLayoutBindings[1].descriptorCount = 1; // Matching 'textureSampler' declaration in the FS
    uboLayoutBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    uboLayoutBindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    m_descriptorSetLayout.initialize(make_array_view(uboLayoutBindings));

    // The Descriptor Set:
    const VkDescriptorSetLayout layouts[] = { m_descriptorSetLayout.descriptorSetLayoutHandle() };
    m_descriptorSet.initialize(&m_descriptorSetPool);
    m_descriptorSet.allocate(make_array_view(layouts));

    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = m_uniformBuffer.bufferHandle();
    bufferInfo.range  = m_uniformBuffer.elementSizeBytes();
    bufferInfo.offset = 0;

    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = m_texture.imageViewHandle();
    imageInfo.sampler     = m_sampler;

    VkWriteDescriptorSet descriptorWrites[2] = {};
    descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[0].dstBinding      = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo     = &bufferInfo;
    descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[1].dstBinding      = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].pImageInfo      = &imageInfo;
    m_descriptorSet.update(make_array_view(descriptorWrites));
}

void VkAppTeapotModel::initPipeline()
{
    const VkDescriptorSetLayout setLayouts[] = { m_descriptorSetLayout.descriptorSetLayoutHandle() };
    m_pipelineStateLayout.initialize(make_array_view(setLayouts));

    PipelineStateBuilder psoBuilder;
    const Size2D fbSize = context().framebufferSize();

    psoBuilder.viewport.width           = static_cast<float>(fbSize.width);
    psoBuilder.viewport.height          = static_cast<float>(fbSize.height);
    psoBuilder.scissor.extent.width     = fbSize.width;
    psoBuilder.scissor.extent.height    = fbSize.height;
    psoBuilder.pipelineState.layout     = m_pipelineStateLayout;
    psoBuilder.pipelineState.renderPass = context().mainRenderPass();
    psoBuilder.pipelineState.stageCount = m_shaderProgram.pipelineStages(&psoBuilder.shaderPipelineStages);

    const auto & bindingDescription     = MeshVertex::bindingDescription();
    const auto & attributeDescriptions  = MeshVertex::attributeDescriptions();

    psoBuilder.vertexInputState.vertexBindingDescriptionCount   = 1;
    psoBuilder.vertexInputState.pVertexBindingDescriptions      = &bindingDescription;

    psoBuilder.vertexInputState.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
    psoBuilder.vertexInputState.pVertexAttributeDescriptions    = attributeDescriptions.data();

    m_pipelineState.initialize(psoBuilder);
}

void VkAppTeapotModel::initTexture()
{
    m_sampler.initialize(Sampler::defaults());

    const auto & cmdBuff = context().mainTextureStagingCmdBuffer();
    cmdBuff.beginRecording();

    m_texture.load();

    cmdBuff.endRecording();
    cmdBuff.submitAndWaitComplete(context().graphisQueue());
    cmdBuff.reset();

    // Done with the staging data.
    m_texture.releaseStagingResources();
}

void VkAppTeapotModel::initVertexBuffer()
{
    m_mesh.initFromFile(VKTB_MESH_MODELS_PATH "Teapot/teapot.obj", 0.1f);

    m_vertexBuffer.initialize(m_mesh.vertexCount());
    m_indexBuffer.initialize(m_mesh.indexCount());

    m_vertexBuffer.writeN(make_array_view(m_mesh.vertexes));
    m_indexBuffer.writeN(make_array_view(m_mesh.indexes));

    // Validate the index buffer:
    assert(m_vertexBuffer.elementCount() == m_mesh.vertexCount());
    assert(m_indexBuffer.elementCount()  == m_mesh.indexCount());
    for (MeshIndex idx : m_mesh.indexes)
    {
        assert(idx < MeshIndex(m_mesh.vertexCount()));
    }
}

void VkAppTeapotModel::updateBuffers(CommandBuffer & cmdBuff)
{
    // Since the geometry never changes, we don't need to issue
    // a GPU copy command more than once!
    static bool s_vbUploadedToGpu = false;

    if (!s_vbUploadedToGpu)
    {
        m_vertexBuffer.uploadStagingToGpu(cmdBuff);
        m_indexBuffer.uploadStagingToGpu(cmdBuff);
        s_vbUploadedToGpu = true;
    }

    //
    // Uniform buffer per-frame update:
    //
    const float time      = timeSeconds();
    const float rotationY = normalizeAngle180(time * 10.0f);

    m_matrices.model      = Matrix4::translation(Vector3{ 0.0f, -3.0f, -20.0f }) * Matrix4::rotationY(rotationY * DegToRad);
    m_matrices.view       = Matrix4::lookAt(Point3{ 0.0f, 0.0f, 2.0f }, Point3{ 0.0f, 0.0f, -1.0f }, Vector3{ 0.0f, -1.0f, 0.0f });
    m_matrices.projection = Matrix4::perspective(45.0f * DegToRad, context().framebufferAspect(), 0.1f, 100.0f);
    m_matrices.mvp        = m_matrices.projection * m_matrices.view * m_matrices.model;

    m_uniformBuffer.write(m_matrices);
    m_uniformBuffer.uploadStagingToGpu(cmdBuff);
}

void VkAppTeapotModel::prepareCommandBuffer(CommandBuffer & cmdBuff)
{
    cmdBuff.beginRecording();

    updateBuffers(cmdBuff);

    context().beginRenderPass(cmdBuff);

    context().bindGraphicsPipelineState(cmdBuff, m_pipelineState);

    const auto descriptorSet = make_array_view(m_descriptorSet.descriptorSetHandles);
    context().bindGraphicsDescriptorSets(cmdBuff, m_pipelineStateLayout, descriptorSet);

    context().bindVertexBuffer(cmdBuff, m_vertexBuffer);
    context().bindIndexBuffer(cmdBuff, m_indexBuffer, vkIndexTypeForBuffer(m_indexBuffer));

    context().drawIndexed(cmdBuff, m_indexBuffer.elementCount(), 1, 0, 0, 0);

    context().endRenderPass(cmdBuff);

    cmdBuff.endRecording();
}

void VkAppTeapotModel::onFrameUpdate()
{
    context().beginFrame();

    m_currentCmdBufferFence->wait(InfiniteFenceWaitTimeout);
    m_currentCmdBufferFence->reset();
    m_currentCmdBuffer->reset();

    prepareCommandBuffer(*m_currentCmdBuffer);

    VkCommandBuffer cbHandle = m_currentCmdBuffer->commandBufferHandle();
    const auto submitBuffers = make_array_view(&cbHandle, 1);

    context().endFrame(submitBuffers, m_currentCmdBufferFence->fenceHandle());

    // Grab the next buffer in the chain:
    m_nextCmdBufferIndex    = (m_nextCmdBufferIndex + 1) % m_cmdBuffers.size();
    m_currentCmdBuffer      = m_cmdBuffers[m_nextCmdBufferIndex].get();
    m_currentCmdBufferFence = m_cmdBufferFences[m_nextCmdBufferIndex].get();
}

// ========================================================
