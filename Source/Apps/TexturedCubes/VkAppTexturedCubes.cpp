
// ================================================================================================
// File: Apps/TexturedCubes/VkAppTexturedCubes.cpp
// Author: Guilherme R. Lampert
// Created on: 08/04/17
// Brief: Draws two textured cubes. One texture is loaded from file, the other is generated.
// ================================================================================================

#include "Apps/VulkanDemoApp.hpp"
#include "VkToolbox/DescriptorSets.hpp"
#include "VkToolbox/PipelineState.hpp"
#include "VkToolbox/GlslShader.hpp"
#include "VkToolbox/Texture.hpp"
#include "VkToolbox/Buffers.hpp"
#include "VkToolbox/Mesh.hpp"

using namespace VkToolbox;

// ========================================================
// class VkAppTexturedCubes:
// ========================================================

class VkAppTexturedCubes final
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
    const char *             m_shaderFilename = VKTB_SHADER_SOURCE_PATH "TexturedCubes.glsl";
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

    std::array<Matrices, 2>                 m_matrices;
    StructuredShaderUniformBuffer<Matrices> m_uniformBuffer;
    StructuredVertexBuffer<MeshVertex>      m_vertexBuffer;
    StructuredIndexBuffer<MeshIndex>        m_indexBuffer;

    // Textures:
    const char * m_texture0Name = VKTB_TEXTURES_PATH "Common/lenna.png";
    const char * m_texture1Name = VKTB_TEXTURES_PATH "Generated/checkers.png";
    Texture      m_texture0;
    Texture      m_texture1;
    Sampler      m_sharedSampler;

private:

    void initDescriptorSets();
    void initPipeline();
    void initVertexBuffer();
    void initTextures();

    void updateBuffers(CommandBuffer & cmdBuff);
    void prepareCommandBuffer(CommandBuffer & cmdBuff);

public:

    VkAppTexturedCubes(const StartupOptions & options);
    void onFrameUpdate() override;
};

// ========================================================

VULKAN_DEMO_REGISTER_APP(VkAppTexturedCubes);

// ========================================================

VkAppTexturedCubes::VkAppTexturedCubes(const StartupOptions & options)
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
    , m_texture0{ context(), m_texture0Name }
    , m_texture1{ context(), m_texture1Name }
    , m_sharedSampler{ context() }
{
    m_shaderProgram.load();

    m_uniformBuffer.initialize(2);          // One matrix uniform buffer for each cube we are going to draw
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

    initTextures();
    initDescriptorSets();
    initPipeline();
    initVertexBuffer();

    context().setDefaultClearColor({ 0.7f, 0.7f, 0.7f, 1.0f });
}

void VkAppTexturedCubes::initDescriptorSets()
{
    // Pool:
    const VkDescriptorPoolSize descriptorPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         2 }, // matrices[2]
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 }, // textureSampler[2]
    };
    m_descriptorSetPool.initialize(1, make_array_view(descriptorPoolSizes));

    // Set layout:
    VkDescriptorSetLayoutBinding uboLayoutBindings[2] = {};
    uboLayoutBindings[0].binding         = 0; // layout(binding = 0)
    uboLayoutBindings[0].descriptorCount = 2; // Matching 'matrices[2]' declaration in the VS
    uboLayoutBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBindings[1].binding         = 1; // layout(binding = 1)
    uboLayoutBindings[1].descriptorCount = 2; // Matching 'textureSampler[2]' declaration in the FS
    uboLayoutBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    uboLayoutBindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    m_descriptorSetLayout.initialize(make_array_view(uboLayoutBindings));

    // The Descriptor Set:
    const VkDescriptorSetLayout layouts[] = { m_descriptorSetLayout.descriptorSetLayoutHandle() };
    m_descriptorSet.initialize(&m_descriptorSetPool);
    m_descriptorSet.allocate(make_array_view(layouts));

    VkDescriptorBufferInfo bufferInfos[2] = {};
    bufferInfos[0].buffer = m_uniformBuffer.bufferHandle();
    bufferInfos[0].range  = m_uniformBuffer.elementSizeBytes();
    bufferInfos[0].offset = 0;
    bufferInfos[1].buffer = m_uniformBuffer.bufferHandle();
    bufferInfos[1].range  = m_uniformBuffer.elementSizeBytes();
    bufferInfos[1].offset = m_uniformBuffer.elementSizeBytes();

    VkDescriptorImageInfo imageInfos[2] = {};
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[0].imageView   = m_texture0.imageViewHandle();
    imageInfos[0].sampler     = m_sharedSampler;
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[1].imageView   = m_texture1.imageViewHandle();
    imageInfos[1].sampler     = m_sharedSampler;

    VkWriteDescriptorSet descriptorWrites[2] = {};
    descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[0].dstBinding      = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 2;
    descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo     = bufferInfos;
    descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[1].dstBinding      = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorCount = 2;
    descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].pImageInfo      = imageInfos;
    m_descriptorSet.update(make_array_view(descriptorWrites));
}

void VkAppTexturedCubes::initPipeline()
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

void VkAppTexturedCubes::initVertexBuffer()
{
    MeshVertex verts[BoxVertexes];
    MeshIndex indexes[BoxIndexes];

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

void VkAppTexturedCubes::initTextures()
{
    m_sharedSampler.initialize(Sampler::defaults());

    const auto & cmdBuff = context().mainTextureStagingCmdBuffer();
    cmdBuff.beginRecording();
    {
        // Not using mipmaps this time.
        m_texture0.setGenerateMipmapsOnLoad(false);
        m_texture1.setGenerateMipmapsOnLoad(false);

        // Texture loaded form file:
        m_texture0.load();

        // Generated texture:
        Image image;
        image.initWithCheckerPattern({ 64, 64 }, 4);
        m_texture1.loadFromImageInMemory(image);
    }
    cmdBuff.endRecording();
    cmdBuff.submitAndWaitComplete(context().graphisQueue());
    cmdBuff.reset();

    // Done with the staging data.
    m_texture0.releaseStagingImage();
    m_texture1.releaseStagingImage();
}

void VkAppTexturedCubes::updateBuffers(CommandBuffer & cmdBuff)
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
    const float rotationX = normalizeAngle180(time * 60.0f);
    const float rotationY = normalizeAngle180(time * 30.0f);

    m_matrices[0].model      = Matrix4::translation(Vector3{ -1.0f, 0.0f, -3.0f }) * Matrix4::rotationZYX(Vector3{ 0.0f, rotationY * DegToRad, rotationX * DegToRad });
    m_matrices[0].view       = Matrix4::lookAt(Point3{ 0.0f, 0.0f, 2.0f }, Point3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f });
    m_matrices[0].projection = Matrix4::perspective(45.0f * DegToRad, context().framebufferAspect(), 0.1f, 100.0f);
    m_matrices[0].mvp        = m_matrices[0].projection * m_matrices[0].view * m_matrices[0].model;
    m_uniformBuffer.write(m_matrices[0], 0);

    m_matrices[1].model      = Matrix4::translation(Vector3{ 1.0f, 0.0f, -3.0f }) * Matrix4::rotationZYX(Vector3{ 0.0f, rotationY * DegToRad, rotationX * DegToRad });
    m_matrices[1].view       = Matrix4::lookAt(Point3{ 0.0f, 0.0f, 2.0f }, Point3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f });
    m_matrices[1].projection = Matrix4::perspective(45.0f * DegToRad, context().framebufferAspect(), 0.1f, 100.0f);
    m_matrices[1].mvp        = m_matrices[1].projection * m_matrices[1].view * m_matrices[1].model;
    m_uniformBuffer.write(m_matrices[1], 1);

    m_uniformBuffer.uploadStagingToGpu(cmdBuff);
}

void VkAppTexturedCubes::prepareCommandBuffer(CommandBuffer & cmdBuff)
{
    cmdBuff.beginRecording();

    updateBuffers(cmdBuff);

    context().beginRenderPass(cmdBuff);

    context().bindGraphicsPipelineState(cmdBuff, m_pipelineState);

    const auto descriptorSet = make_array_view(m_descriptorSet.descriptorSetHandles);
    context().bindGraphicsDescriptorSets(cmdBuff, m_pipelineStateLayout, descriptorSet);

    context().bindVertexBuffer(cmdBuff, m_vertexBuffer);
    context().bindIndexBuffer(cmdBuff, m_indexBuffer, vkIndexTypeForBuffer(m_indexBuffer));

    context().drawIndexed(cmdBuff, m_indexBuffer.elementCount(), 2, 0, 0, 0);

    context().endRenderPass(cmdBuff);

    cmdBuff.endRecording();
}

void VkAppTexturedCubes::onFrameUpdate()
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
