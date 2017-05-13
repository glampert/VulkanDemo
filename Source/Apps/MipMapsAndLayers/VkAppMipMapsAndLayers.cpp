
// ================================================================================================
// File: Apps/MipMapsAndLayers/VkAppMipMapsAndLayers.cpp
// Author: Guilherme R. Lampert
// Created on: 07/05/17
// Brief: Testing texture mipmaps and layers.
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
// class VkAppMipMapsAndLayers:
// ========================================================

class VkAppMipMapsAndLayers final
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
    const char *             m_shaderFilename = VKTB_SHADER_SOURCE_PATH "MipsLayersTest.glsl";
    GlslShader               m_shaderProgram;
    DescriptorSetPool        m_descriptorSetPool;
    DescriptorSetLayout      m_descriptorSetLayout;
    DescriptorSets<1>        m_descriptorSet;
    PipelineStateLayout      m_pipelineStateLayout;
    PipelineStateObject      m_pipelineState;

    struct Matrices
    {
        Matrix4 model      = Matrix4::identity();
        Matrix4 view       = Matrix4::identity();
        Matrix4 projection = Matrix4::identity();
        Matrix4 mvp        = Matrix4::identity();
    };

    Matrices                                m_matrices;
    StructuredShaderUniformBuffer<Matrices> m_uniformBuffer;
    StructuredVertexBuffer<MeshVertex>      m_vbMipmaps;
    StructuredVertexBuffer<MeshVertex>      m_vbLayers;

    // Textures/samplers:
    static constexpr int NumMipmaps = 6;
    static constexpr int NumLayers  = 3;
    Texture m_arrayTexture;
    Texture m_mipmapsTexture;
    Sampler m_arraySampler;
    Sampler m_mipmapsSampler;

private:

    void initDescriptorSets();
    void initPipeline();
    void initTexture();
    void initVertexBuffer();

    void updateBuffers(CommandBuffer & cmdBuff);
    void prepareCommandBuffer(CommandBuffer & cmdBuff);

public:

    VkAppMipMapsAndLayers(const StartupOptions & options);
    void onFrameUpdate() override;
};

// ========================================================

VULKAN_DEMO_REGISTER_APP(VkAppMipMapsAndLayers);

// ========================================================

VkAppMipMapsAndLayers::VkAppMipMapsAndLayers(const StartupOptions & options)
    : VulkanDemoApp{ options }
    , m_cmdPool{ context() }
    , m_shaderProgram{ context(), m_shaderFilename }
    , m_descriptorSetPool{ context() }
    , m_descriptorSetLayout{ context() }
    , m_pipelineStateLayout{ context() }
    , m_pipelineState{ m_pipelineStateLayout }
    , m_uniformBuffer{ context() }
    , m_vbMipmaps{ context() }
    , m_vbLayers{ context() }
    , m_arrayTexture{ context(), VKTB_TEXTURES_PATH "Common/a_dummy.dds" }
    , m_mipmapsTexture{ context(), "Generated/mipmap_texture.png" }
    , m_arraySampler{ context() }
    , m_mipmapsSampler{ context() }
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

void VkAppMipMapsAndLayers::initDescriptorSets()
{
    // Pool:
    const VkDescriptorPoolSize descriptorPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1 }, // uniform matrices
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 }, // uniform mipTextureSampler/arrayTextureSampler
    };
    m_descriptorSetPool.initialize(1, make_array_view(descriptorPoolSizes));

    // Set layout:
    VkDescriptorSetLayoutBinding uboLayoutBindings[3] = {};
    uboLayoutBindings[0].binding         = 0; // layout(binding = 0)
    uboLayoutBindings[0].descriptorCount = 1; // Matching 'matrices' declaration in the VS
    uboLayoutBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBindings[1].binding         = 1; // layout(binding = 1)
    uboLayoutBindings[1].descriptorCount = 1; // Matching 'mipTextureSampler' declaration in the FS
    uboLayoutBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    uboLayoutBindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBindings[2].binding         = 2; // layout(binding = 2)
    uboLayoutBindings[2].descriptorCount = 1; // Matching 'arrayTextureSampler' declaration in the FS
    uboLayoutBindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    uboLayoutBindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    m_descriptorSetLayout.initialize(make_array_view(uboLayoutBindings));

    // The Descriptor Set:
    const VkDescriptorSetLayout layouts[] = { m_descriptorSetLayout.descriptorSetLayoutHandle() };
    m_descriptorSet.initialize(&m_descriptorSetPool);
    m_descriptorSet.allocate(make_array_view(layouts));

    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = m_uniformBuffer.bufferHandle();
    bufferInfo.range  = m_uniformBuffer.elementSizeBytes();
    bufferInfo.offset = 0;

    VkDescriptorImageInfo imageInfos[2];
    imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[0].imageView   = m_mipmapsTexture.imageViewHandle();
    imageInfos[0].sampler     = m_mipmapsSampler;
    imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[1].imageView   = m_arrayTexture.imageViewHandle();
    imageInfos[1].sampler     = m_arraySampler;

    VkWriteDescriptorSet descriptorWrites[3] = {};
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
    descriptorWrites[1].pImageInfo      = &imageInfos[0];
    descriptorWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet          = m_descriptorSet.descriptorSetHandles[0];
    descriptorWrites[2].dstBinding      = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[2].pImageInfo      = &imageInfos[1];
    m_descriptorSet.update(make_array_view(descriptorWrites));
}

void VkAppMipMapsAndLayers::initPipeline()
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

void VkAppMipMapsAndLayers::initTexture()
{
    Image img;
    img.initWithColorFill(Size2D{ 32, 32 }, Color32{ 255, 255, 255 });
    img.generateMipmapSurfaces();

    assert(img.format() == Image::Format::RGBA8);
    assert(img.surfaceCount() == NumMipmaps);

    const std::array<Color32, NumMipmaps> mipmapColors = {{
        { 255, 255, 255 }, // 0=white
        { 255, 0,   0   }, // 1=red
        { 0,   255, 0   }, // 2=green
        { 0,   0,   255 }, // 3=blue
        { 255, 255, 0   }, // 4=yellow
        { 0,   255, 255 }, // 5=cyan
    }};

    for (int s = 0; s < img.surfaceCount(); ++s)
    {
        ImageSurface & surface = img.surface(s);
        Color32 * pixels = surface.pixelDataAs<Color32>();
        for (int p = 0; p < surface.pixelCount(); ++p)
        {
            pixels[p] = mipmapColors[s];
        }
    }

    const auto & cmdBuff = context().mainTextureStagingCmdBuffer();
    cmdBuff.beginRecording();

    m_mipmapsTexture.loadFromImageInMemory(img);
    assert(m_mipmapsTexture.isMipmapped() && m_mipmapsTexture.mipmapCount() == NumMipmaps);

    m_arrayTexture.load();
    assert(m_arrayTexture.isArrayTexture() && m_arrayTexture.layerCount() == NumLayers);

    cmdBuff.endRecording();
    cmdBuff.submitAndWaitComplete(context().graphisQueue());
    cmdBuff.reset();

    // Set up the samplers:
    VkSamplerCreateInfo samplerDesc = Sampler::defaults();
    samplerDesc.maxLod     = NumMipmaps;
    samplerDesc.minFilter  = VK_FILTER_NEAREST;
    samplerDesc.magFilter  = VK_FILTER_NEAREST;
    samplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    m_mipmapsSampler.initialize(samplerDesc);

    samplerDesc = Sampler::defaults();
    samplerDesc.maxLod     = static_cast<float>(m_arrayTexture.mipmapCount());
    samplerDesc.minFilter  = VK_FILTER_NEAREST;
    samplerDesc.magFilter  = VK_FILTER_NEAREST;
    samplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    m_arraySampler.initialize(samplerDesc);

    // Done with the staging data.
    m_mipmapsTexture.releaseStagingResources();
    m_arrayTexture.releaseStagingResources();
}

void VkAppMipMapsAndLayers::initVertexBuffer()
{
    MeshVertex quad[6] = {};
    const Float4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    quad[0].position  = {-0.5f,  0.5f, -1.0f };
    quad[1].position  = {-0.5f, -0.5f, -1.0f };
    quad[2].position  = { 0.5f, -0.5f, -1.0f };
    quad[3].position  = { 0.5f, -0.5f, -1.0f };
    quad[4].position  = { 0.5f,  0.5f, -1.0f };
    quad[5].position  = {-0.5f,  0.5f, -1.0f };

    quad[0].texcoords = { 1.0f, 1.0f };
    quad[1].texcoords = { 1.0f, 0.0f };
    quad[2].texcoords = { 0.0f, 0.0f };
    quad[3].texcoords = { 0.0f, 0.0f };
    quad[4].texcoords = { 0.0f, 1.0f };
    quad[5].texcoords = { 1.0f, 1.0f };

    // Top quads -> one for each texture mipmap
    {
        unsigned i = 0;
        float tx = 1.0f, ty = 0.7f, s = 0.5f;
        MeshVertex verts[NumMipmaps * arrayLength(quad)] = {};

        for (unsigned m = 0; m < NumMipmaps; ++m)
        {
            for (unsigned v = 0; v < arrayLength(quad); ++v)
            {
                // scale
                verts[i].position.x = quad[v].position.x * s;
                verts[i].position.y = quad[v].position.y * s;
                verts[i].position.z = quad[v].position.z * s;

                // translate
                verts[i].position.x += tx;
                verts[i].position.y += ty;

                // copy color/UVs
                verts[i].color = white;
                verts[i].texcoords = quad[v].texcoords;

                // Layer index encoded in the normal
                verts[i].normal.x = -1.0f; // top quads, use m_mipmapsTexture

                ++i;
            }
            s  -= 0.08f;
            tx -= 1.350f * s;
            ty -= 0.008f * s;
        }

        m_vbMipmaps.initialize(static_cast<int>(arrayLength(verts)));
        m_vbMipmaps.writeN(verts);
    }

    // Bottom quads -> one for each texture layer
    {
        unsigned i = 0;
        float tx = 1.0f;
        MeshVertex verts[NumLayers * arrayLength(quad)] = {};

        for (unsigned l = 0; l < NumLayers; ++l)
        {
            for (unsigned v = 0; v < arrayLength(quad); ++v)
            {
                // scale
                verts[i].position.x = quad[v].position.x * 0.7f;
                verts[i].position.y = quad[v].position.y * 0.7f;
                verts[i].position.z = quad[v].position.z * 0.7f;

                // translate
                verts[i].position.x += tx;
                verts[i].position.y -= 0.5f;

                // copy color/UVs
                verts[i].color = white;
                verts[i].texcoords = quad[v].texcoords;

                // Layer index encoded in the normal
                verts[i].normal.x = static_cast<float>(l); // bottom quads, use one layer of m_arrayTexture each

                ++i;
            }
            tx -= 1.0f;
        }

        m_vbLayers.initialize(static_cast<int>(arrayLength(verts)));
        m_vbLayers.writeN(verts);
    }
}

void VkAppMipMapsAndLayers::updateBuffers(CommandBuffer & cmdBuff)
{
    // Since the geometry never changes, we don't need to issue
    // a GPU copy command more than once!
    static bool s_vbUploadedToGpu = false;

    if (!s_vbUploadedToGpu)
    {
        m_vbMipmaps.uploadStagingToGpu(cmdBuff);
        m_vbLayers.uploadStagingToGpu(cmdBuff);
        s_vbUploadedToGpu = true;
    }

    //
    // Uniform buffer per-frame update:
    // (matrices never change in this sample, so we could avoid updating them).
    //
    m_matrices.view       = Matrix4::lookAt(Point3{ 0.0f, 0.0f, 2.0f }, Point3{ 0.0f, 0.0f, -1.0f }, Vector3{ 0.0f, -1.0f, 0.0f });
    m_matrices.projection = Matrix4::perspective(45.0f * DegToRad, context().framebufferAspect(), 0.1f, 100.0f);
    m_matrices.mvp        = m_matrices.projection * m_matrices.view;

    m_uniformBuffer.write(m_matrices);
    m_uniformBuffer.uploadStagingToGpu(cmdBuff);
}

void VkAppMipMapsAndLayers::prepareCommandBuffer(CommandBuffer & cmdBuff)
{
    cmdBuff.beginRecording();

    updateBuffers(cmdBuff);

    context().beginRenderPass(cmdBuff);

    context().bindGraphicsPipelineState(cmdBuff, m_pipelineState);

    const auto descriptorSet = make_array_view(m_descriptorSet.descriptorSetHandles);
    context().bindGraphicsDescriptorSets(cmdBuff, m_pipelineStateLayout, descriptorSet);

    context().bindVertexBuffer(cmdBuff, m_vbMipmaps);
    context().drawUnindexed(cmdBuff, m_vbMipmaps.elementCount(), 1, 0, 0);

    context().bindVertexBuffer(cmdBuff, m_vbLayers);
    context().drawUnindexed(cmdBuff, m_vbLayers.elementCount(), 1, 0, 0);

    context().endRenderPass(cmdBuff);

    cmdBuff.endRecording();
}

void VkAppMipMapsAndLayers::onFrameUpdate()
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
