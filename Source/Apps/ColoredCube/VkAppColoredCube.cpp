
// ================================================================================================
// File: Apps/ColoredCube/VkAppColoredCube.cpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Draws a colored rotating cube using vertex+index buffers.
// ================================================================================================

#include "Apps/VulkanDemoApp.hpp"
#include "VkToolbox/DescriptorSets.hpp"
#include "VkToolbox/PipelineState.hpp"
#include "VkToolbox/GlslShader.hpp"
#include "VkToolbox/Buffers.hpp"
#include "VkToolbox/Mesh.hpp"

using namespace VkToolbox;

// ========================================================
// class VkAppColoredCube:
// ========================================================

class VkAppColoredCube final
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
    const char *             m_shaderFilename = VKTB_SHADER_SOURCE_PATH "VertexColorSimple.glsl";
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
    StructuredIndexBuffer<std::uint16_t>    m_indexBuffer;

private:

    void initDescriptorSets();
    void initPipeline();
    void initVertexBuffer();

    void updateBuffers(CommandBuffer & cmdBuff);
    void prepareCommandBuffer(CommandBuffer & cmdBuff);

public:

    VkAppColoredCube(const StartupOptions & options);
    void onFrameUpdate() override;
};

// ========================================================

VULKAN_DEMO_REGISTER_APP(VkAppColoredCube);

// ========================================================

VkAppColoredCube::VkAppColoredCube(const StartupOptions & options)
    : VulkanDemoApp{ options }
    , m_cmdPool{ context() }
    , m_shaderProgram{ context(), strReg().access(m_shaderFilename) }
    , m_descriptorSetPool{ context() }
    , m_descriptorSetLayout{ context() }
    , m_pipelineStateLayout{ context() }
    , m_pipelineState{ m_pipelineStateLayout }
    , m_uniformBuffer{ context() }
    , m_vertexBuffer{ context() }
    , m_indexBuffer{ context() }
{
    m_shaderProgram.load();

    m_uniformBuffer.initialize(1);
    m_vertexBuffer.initialize(BoxVertexes); // Number of unique vertexes in the object
    m_indexBuffer.initialize(BoxIndexes);   // Number of indexes mapping the shared verts

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

    initDescriptorSets();
    initPipeline();
    initVertexBuffer();
}

void VkAppColoredCube::initDescriptorSets()
{
    // Pool:
    const VkDescriptorPoolSize descriptorPoolSize[] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
    m_descriptorSetPool.initialize(1, make_array_view(descriptorPoolSize));

    // Set layout:
    VkDescriptorSetLayoutBinding uboLayoutBinding[1] = {};
    uboLayoutBinding[0].binding            = 0; // layout(binding = 0)
    uboLayoutBinding[0].descriptorCount    = 1; // Matching 'matrices' declaration in the VS
    uboLayoutBinding[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding[0].pImmutableSamplers = nullptr;
    m_descriptorSetLayout.initialize(make_array_view(uboLayoutBinding));

    // The Descriptor Set:
    const VkDescriptorSetLayout layouts[] = { m_descriptorSetLayout.descriptorSetLayoutHandle() };
    m_descriptorSet.initialize(&m_descriptorSetPool);
    m_descriptorSet.allocate(make_array_view(layouts));

    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = m_uniformBuffer.bufferHandle();
    bufferInfo.range  = m_uniformBuffer.elementSizeBytes();
    bufferInfo.offset = 0;

    VkWriteDescriptorSet descriptorWrites[1] = {};
    descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[0].dstBinding      = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo     = &bufferInfo;
    m_descriptorSet.update(make_array_view(descriptorWrites));
}

void VkAppColoredCube::initPipeline()
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

void VkAppColoredCube::initVertexBuffer()
{
    MeshVertex verts[BoxVertexes];
    std::uint16_t indexes[BoxIndexes];

    const Color32 faceColors[6] = {
        { 255, 255, 255 },
        {   0,   0, 255 },
        {   0, 255,   0 },
        { 255,   0,   0 },
        { 255, 255,   0 },
        {   0, 255, 255 },
    };
    createBoxMesh(1.0f, 1.0f, 1.0f, faceColors, verts, indexes);

    m_vertexBuffer.writeN(verts);
    m_indexBuffer.writeN(indexes);
}

void VkAppColoredCube::updateBuffers(CommandBuffer & cmdBuff)
{
    // Since the geometry never changes, we don't need to issue
    // a GPU copy command more than once!
    static bool vbUploadedToGpu = false;

    if (!vbUploadedToGpu)
    {
        m_vertexBuffer.uploadStagingToGpu(cmdBuff);
        m_indexBuffer.uploadStagingToGpu(cmdBuff);
        vbUploadedToGpu = true;
    }

    //
    // Uniform buffer per-frame update:
    //
    const float time      = timeSeconds();
    const float rotationX = normalizeAngle180(time * 60.0f);
    const float rotationY = normalizeAngle180(time * 30.0f);

    m_matrices.model      = Matrix4::rotationZYX(Vector3{ 0.0f, rotationY * DegToRad, rotationX * DegToRad });
    m_matrices.view       = Matrix4::lookAt(Point3{ 0.0f, 0.0f, 3.0f }, Point3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f });
    m_matrices.projection = Matrix4::perspective(45.0f * DegToRad, context().framebufferAspect(), 0.1f, 10.0f);
    m_matrices.mvp        = m_matrices.projection * m_matrices.view * m_matrices.model;

    m_uniformBuffer.write(m_matrices);
    m_uniformBuffer.uploadStagingToGpu(cmdBuff);
}

void VkAppColoredCube::prepareCommandBuffer(CommandBuffer & cmdBuff)
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

void VkAppColoredCube::onFrameUpdate()
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
