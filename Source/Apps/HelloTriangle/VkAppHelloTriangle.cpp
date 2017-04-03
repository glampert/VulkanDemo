
// ================================================================================================
// File: Apps/HelloTriangle/VkAppHelloTriangle.cpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Draws a triangle without using vertex or index buffer - data is embedded into the shader.
// ================================================================================================

#include "Apps/VulkanDemoApp.hpp"
#include "VkToolbox/DescriptorSets.hpp"
#include "VkToolbox/PipelineState.hpp"
#include "VkToolbox/GlslShader.hpp"
#include "VkToolbox/Buffers.hpp"

using namespace VkToolbox;

// ========================================================
// class VkAppHelloTriangle:
// ========================================================

class VkAppHelloTriangle final
    : public VulkanDemoApp
{
private:

    using BufferPtr = std::unique_ptr<CommandBuffer>;
    using FencePtr  = std::unique_ptr<Fence>;

    // One buffer for each frame in flight (assuming the context was created with triple buffering).
    CommandPool              m_cmdPool;
    std::array<BufferPtr, 3> m_cmdBuffers;
    std::array<FencePtr,  3> m_cmdBufferFences;
    CommandBuffer *          m_currentCmdBuffer      = nullptr;
    Fence *                  m_currentCmdBufferFence = nullptr;
    int                      m_nextCmdBufferIndex    = 0;

    // Shader draws a triangle using built-in vertexes, so we don't need to create a VB/IB for this demo.
    const char * m_shaderFilename = VKTB_SHADER_SOURCE_PATH "BuiltInTriangleTest.glsl";
    GlslShader   m_shaderProgram;

    // Backing buffer for the shader data. We will have 4 color vectors and 1 integer
    // value (5 descriptors total). These sizes must match the shader declarations.
    static constexpr int NumTintColors   = 4;
    static constexpr int NumOfShaders    = 1;
    static constexpr int DescriptorCount = 5;
    const int m_uniformBufferTypeCounts[2] = { NumTintColors, 1 };
    const VkDescriptorPoolSize m_descriptorPoolSize = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DescriptorCount };
    MultiStructuredShaderUniformBuffer<Vector4, int> m_uniformBuffer;

    // Each uniform variable or buffer in the shader is a Descriptor;
    // Each shader has a set of descriptors (a Descriptor Set);
    // DescriptorSetLayout is basically the data layout of all uniforms in a shader.
    DescriptorSetPool   m_descriptorSetPool;
    DescriptorSetLayout m_descriptorSetLayout;
    DescriptorSets<1>   m_descriptorSet;

    // Similarly to the descriptors, pipeline state objects (PSOs) also
    // have a layout object that defines the layout of the descriptors used.
    // The PSO keeps all the render states needed for a render pass.
    PipelineStateLayout m_pipelineStateLayout;
    PipelineStateObject m_pipelineState;

private:

    void initDescriptorSets();
    void initPipeline();

    void updateUniformBuffer(CommandBuffer & cmdBuff);
    void prepareCommandBuffer(CommandBuffer & cmdBuff);

public:

    VkAppHelloTriangle(const StartupOptions & options);
    void onFrameUpdate() override;
};

// ========================================================

VULKAN_DEMO_REGISTER_APP(VkAppHelloTriangle);

// ========================================================

VkAppHelloTriangle::VkAppHelloTriangle(const StartupOptions & options)
    : VulkanDemoApp{ options }
    , m_cmdPool{ context() }
    , m_shaderProgram{ context(), strReg().access(m_shaderFilename) }
    , m_uniformBuffer{ context() }
    , m_descriptorSetPool{ context() }
    , m_descriptorSetLayout{ context() }
    , m_pipelineStateLayout{ context() }
    , m_pipelineState{ m_pipelineStateLayout }
{
    m_shaderProgram.load();

    m_uniformBuffer.initialize(m_uniformBufferTypeCounts);
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
}

void VkAppHelloTriangle::initDescriptorSets()
{
    // Pool:
    m_descriptorSetPool.initialize(NumOfShaders, make_array_view(&m_descriptorPoolSize, 1));

    // Set layout:
    VkDescriptorSetLayoutBinding uboLayoutBindings[2] = {};

    uboLayoutBindings[0].binding         = 0;
    uboLayoutBindings[0].descriptorCount = 4; // Matching 'ubo[4]' declaration in the FS
    uboLayoutBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    uboLayoutBindings[1].binding         = 1;
    uboLayoutBindings[1].descriptorCount = 1; // Matching 'ubi' declaration in the FS
    uboLayoutBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    m_descriptorSetLayout.initialize(make_array_view(uboLayoutBindings));

    // The Descriptor Set:
    const VkDescriptorSetLayout layouts[] = { m_descriptorSetLayout.descriptorSetLayoutHandle() };
    m_descriptorSet.initialize(&m_descriptorSetPool);
    m_descriptorSet.allocate(make_array_view(layouts));

    VkDescriptorBufferInfo bufferInfos[DescriptorCount] = {};
    VkWriteDescriptorSet   descriptorWrites[2] = {}; // Two different binding points

    for (int i = 0; i < DescriptorCount; ++i)
    {
        bufferInfos[i].buffer = m_uniformBuffer.bufferHandle();
        bufferInfos[i].range  = m_uniformBuffer.elementSizeBytes();
        bufferInfos[i].offset = m_uniformBuffer.elementSizeBytes() * i;
    }

    descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[0].dstBinding      = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 4;
    descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo     = &bufferInfos[0];

    descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[1].dstBinding      = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].pBufferInfo     = &bufferInfos[4];

    m_descriptorSet.update(make_array_view(descriptorWrites));
}

void VkAppHelloTriangle::initPipeline()
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

    m_pipelineState.initialize(psoBuilder);
}

void VkAppHelloTriangle::updateUniformBuffer(CommandBuffer & cmdBuff)
{
    static int colorIndex = 0;
    const float time = timeSeconds();

    // Make the triangle colors flash using a sine wave
    const float s = std::abs(std::sin(time * HalfPI));
    if (s <= 0.0001f)
    {
        colorIndex = (colorIndex + 1) % NumTintColors;
    }

    const Vector4 tintColors[NumTintColors] = {
        Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } * s,
        Vector4{ 0.1f, 0.1f, 1.0f, 1.0f } * s,
        Vector4{ 0.1f, 1.0f, 0.1f, 1.0f } * s,
        Vector4{ 1.0f, 0.1f, 0.1f, 1.0f } * s
    };

    m_uniformBuffer.writeN(tintColors);   // offsets 0,1,2,3
    m_uniformBuffer.write(colorIndex, 4); // offset 4

    m_uniformBuffer.uploadStagingToGpu(cmdBuff);
}

void VkAppHelloTriangle::prepareCommandBuffer(CommandBuffer & cmdBuff)
{
    cmdBuff.beginRecording();

    updateUniformBuffer(cmdBuff);

    context().beginRenderPass(cmdBuff);

    context().bindGraphicsPipelineState(cmdBuff, m_pipelineState);

    const auto descriptorSet = make_array_view(m_descriptorSet.descriptorSetHandles);
    context().bindGraphicsDescriptorSets(cmdBuff, m_pipelineStateLayout, descriptorSet);

    context().drawUnindexed(cmdBuff, 3, 1, 0, 0);

    context().endRenderPass(cmdBuff);

    cmdBuff.endRecording();
}

void VkAppHelloTriangle::onFrameUpdate()
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
