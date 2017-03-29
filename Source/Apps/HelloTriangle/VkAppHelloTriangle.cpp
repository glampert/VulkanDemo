
// ================================================================================================
// -*- C++ -*-
// File: Apps/HelloTriangle/VkAppHelloTriangle.cpp
// Author: Guilherme R. Lampert
// Created on: 29/03/17
// Brief: Draws a triangle without using vertex or index buffer - data is embedded into the shader.
// ================================================================================================

/*
Vk demo idea 1:
 - Render the Sponza Scene
 - Write a simple deferred renderer 
 - Support normal/diffuse/specular maps
 - FXAA and Depth of Field for image filters
 - Some kind of shadow mapping
 - Explore threaded rendering with Vulkan!

Vk demo idea 2:
 - Render many instances of animated MD5 models
 - Something like the CoreGL demo I did before, but with several model instances instead, each running a different animation;
 - Explore some way of updating them in parallel (each thread animates and renders a single model)

Vk demo idea 3:
 - Virtual Texturing revisited!
 - 256 page size with inner border (or outer?)
 - Trilinear sw filter to save memory?
 - Use compute shaders to process page id feedback buffer.
 - Explore virtual decals?
 - Compress the indirection texture to save mem?
 - Maybe unnormalized texture coordinates can be useful? [0,texWidth),[0,texHeight)?
*/

#include "Apps/VulkanDemoApp.hpp"

#include "VkToolbox/GlslShader.hpp"
#include "VkToolbox/Texture.hpp"

#include <chrono>

using namespace VkToolbox;

/*
struct ShaderLib final
{
    struct Shader
    {
        void init();
        const GlslShader & get() const;

        OR merge both?

        const GlslShader & shader() const;
        WeakHandle<VkPipeline> pso() const;

    private:
        GlslShaderManager::ResourceIndex m_shaderHandle;
        OwnedHandle<VkPipeline>          m_psoHandle;
    };
};

struct PsoLib final
{
    struct Pso
    {
        void init();
        WeakHandle<VkPipeline> get() const;
    };
};

//PsoLib psoLib;
//psoLib.bar.init();
//psoLib.baz.init();

ShaderLib shaders;
shaders.foo.init();
shaders.bar.init();
or
shader.initAll();
*/

// ================================================================================================

//TODO TEMP
static VkPipeline graphicsPipeline;
static VkPipelineLayout pipelineLayout;
static GlslShader * testShader;

static int nextBuffer = 0;
static VkCommandBuffer currentCmdBuffer;
static VkFence currentFence;
static CommandBuffer* cmdBuffers[3];
static VkFence bufferFences[3];

struct UniformBufferObject
{
    Vector4 color;
};

static VkBuffer uniformStagingBuffer;
static VkDeviceMemory uniformStagingBufferMemory;

static VkBuffer uniformBuffer;
static VkDeviceMemory uniformBufferMemory;

static VkDescriptorSetLayout descriptorSetLayout;
static VkDescriptorPool descriptorPool;
static VkDescriptorSet descriptorSet; // owned by the pool - need not be freed

void createUniformBuffer(VulkanContext & vkContext)
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    vkContext.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformStagingBuffer, &uniformStagingBufferMemory);
    vkContext.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &uniformBuffer, &uniformBufferMemory);
}

void updateUniformBuffer(VulkanContext & vkContext)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;

    UniformBufferObject ubo;
    ubo.color = Vector4(1.0f) * std::sin(time);

    {
        void * pData = nullptr;
        ScopedMapMemory memMap{ vkContext, uniformStagingBufferMemory, 0, sizeof(ubo), 0, &pData };
        std::memcpy(pData, &ubo, sizeof(ubo));
    }

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = sizeof(ubo);
    vkCmdCopyBuffer(currentCmdBuffer, uniformStagingBuffer, uniformBuffer, 1, &copyRegion);

    //vkContext.copyBuffer(currentCmdBuffer, uniformStagingBufferMemory, uniformBuffer, sizeof(ubo));
}

void createDescriptorPool(VulkanContext & vkContext)
{
    VkDescriptorPoolSize poolSize;
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo;
    poolInfo.sType           = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext           = nullptr;
    poolInfo.flags           = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets         = 1;
    poolInfo.poolSizeCount   = 1;
    poolInfo.pPoolSizes      = &poolSize;

    VKTB_CHECK(vkCreateDescriptorPool(vkContext.getVkDeviceHandle(), &poolInfo, 
        vkContext.getAllocationCallbacks(), &descriptorPool));
}

void createDescriptorSet(VulkanContext & vkContext)
{
    const VkDescriptorSetLayout layouts[] = { descriptorSetLayout };

    VkDescriptorSetAllocateInfo allocInfo;
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext              = nullptr;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = layouts;

    VKTB_CHECK(vkAllocateDescriptorSets(vkContext.getVkDeviceHandle(), &allocInfo, &descriptorSet));

    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet               = descriptorSet;
    descriptorWrite.dstBinding           = 0;
    descriptorWrite.dstArrayElement      = 0;
    descriptorWrite.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount      = 1;
    descriptorWrite.pBufferInfo          = &bufferInfo;

    vkUpdateDescriptorSets(vkContext.getVkDeviceHandle(), 1, &descriptorWrite, 0, nullptr);
}

void createDescriptorSetLayout(VulkanContext & vkContext)
{
    VkDescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT; //VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.sType                    = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext                    = nullptr;
    layoutInfo.flags                    = 0;
    layoutInfo.bindingCount             = 1;
    layoutInfo.pBindings                = &uboLayoutBinding;

    VKTB_CHECK(vkCreateDescriptorSetLayout(vkContext.getVkDeviceHandle(), &layoutInfo, 
        vkContext.getAllocationCallbacks(), &descriptorSetLayout));
}

void createPipeline(VulkanContext & vkContext)
{
    for (int i = 0; i < 3; ++i)
    {
        VkFenceCreateInfo fenceCreateInfo;
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFence newFence = VK_NULL_HANDLE;
        VKTB_CHECK(vkCreateFence(vkContext.getVkDeviceHandle(), &fenceCreateInfo,
                                 vkContext.getAllocationCallbacks(), &newFence));

        assert(newFence != VK_NULL_HANDLE);
        bufferFences[i] = newFence;

        cmdBuffers[i] = new CommandBuffer{ vkContext, VK_COMMAND_BUFFER_LEVEL_PRIMARY, vkContext.getMainTextureStagingCmdBufferPool() };
    }
    currentCmdBuffer = cmdBuffers[0]->getVkCmdBufferHandle();
    currentFence = bufferFences[0];

    str32 shdrName = VKTB_SHADER_SOURCE_PATH "BuiltInTriangleTest.glsl";
    testShader = new GlslShader{ vkContext, ResourceId{ &shdrName, Hash64{ shdrName.c_str() } } };
    testShader->load();

    std::array<VkPipelineShaderStageCreateInfo, GlslShaderStage::MaxStages> shaderStages;
    const int shaderStageCount = testShader->getVkPipelineStages(&shaderStages);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vkContext.getFramebufferSize().width;
    viewport.height = (float)vkContext.getFramebufferSize().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = { (uint32_t)vkContext.getFramebufferSize().width, (uint32_t)vkContext.getFramebufferSize().height };

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VulkanContext::ColorMaskRGBA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    const VkDescriptorSetLayout setLayouts[] = { descriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    VKTB_CHECK(vkCreatePipelineLayout(vkContext.getVkDeviceHandle(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStageCount;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = vkContext.getMainRenderPass();
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VKTB_CHECK(vkCreateGraphicsPipelines(vkContext.getVkDeviceHandle(), VK_NULL_HANDLE, 
        1, &pipelineInfo, vkContext.getAllocationCallbacks(), &graphicsPipeline));
}

void createCommandBuffers(VulkanContext & vkContext)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    VKTB_CHECK(vkBeginCommandBuffer(currentCmdBuffer, &beginInfo));

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vkContext.getMainRenderPass();
    renderPassInfo.framebuffer = vkContext.getVkFramebufferHandle(vkContext.getSwapChainBufferIndex());
    renderPassInfo.renderArea.offset = { 0,0 };
    renderPassInfo.renderArea.extent = { (uint32_t)vkContext.getFramebufferSize().width, (uint32_t)vkContext.getFramebufferSize().height };

    const VkClearValue clearColors[][4] = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    renderPassInfo.clearValueCount = 3;                        // one per framebuffer ????
    renderPassInfo.pClearValues = (VkClearValue *)clearColors; // ugh

    updateUniformBuffer(vkContext);

    vkCmdBeginRenderPass(currentCmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        vkCmdBindDescriptorSets(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdDraw(currentCmdBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(currentCmdBuffer);

    VKTB_CHECK(vkEndCommandBuffer(currentCmdBuffer));
}

// ================================================================================================

// Test Shader:
/*
{
    GlslShaderPreproc::setVersion(450, true, true);
    GlslShaderPreproc::setOptimizations(false, true);

    GlslShaderPreproc::setExtension("GL_ARB_separate_shader_objects",  "enable");
    GlslShaderPreproc::setExtension("GL_ARB_shading_language_420pack", "enable");
    GlslShaderPreproc::setExtension("GL_GOOGLE_include_directive",     "enable");

    GlslShaderPreproc::setDefine("POS_LOC",       0);
    GlslShaderPreproc::setDefine("COLOR_IN_LOC",  1);
    GlslShaderPreproc::setDefine("COLOR_OUT_LOC", 0);

    GlslShaderPreproc::setShaderIncludePath("test_includes/sys/");

    str32 shdrName = "test.glsl";
    GlslShader shdr{ &vkContext, ResourceId{ &shdrName, Hash64{ shdrName.c_str() } } };
    shdr.load();

    std::printf("defines string:\n%s", GlslShaderPreproc::getAllDefinesString());

    //TODO: need to query the uniform variables from the shader compiler!

    //TODO:
    // VkPipelineShaderStageCreateInfo::VkSpecializationInfo seem interesting, should give it a try!
    // it allows specifying values for constants without having to recompile the shader ourselves!
}
//*/

// Test Image:
/*
{
    //Image::sm_loadOptions.forceRGBA = true;
    //Image::sm_loadOptions.flipVerticallyOnLoad = true;
    //Image::sm_loadOptions.roundDownToPowerOfTwo = true;
    //Image::sm_loadOptions.roundUpToPowerOfTwo = true;

    Image img0{ "cppreference.jpg" };
    img0.generateMipmapSurfaces();
    img0.writeAllSurfacesToFiles("img0", "png");

    Image img1{{32,32}, Color32{0,0,255}};
    img1.writeToFile("img1.png");

    Image img2;
    img2.initWithCheckerPattern({64,64}, 4);
    img2.writeToFile("img2.png");

    Image img3;
    img3.initWithCheckerPattern({256,256}, 4);
    img3.generateMipmapSurfaces();
    img3.writeAllSurfacesToFiles("img3", "png");
}
//*/

/*
// Test Texture:
{
    const CommandBuffer & cmdBuff = vkContext.getTextureStagingCmdBuffer();

    str32 imgName = "cppreference.jpg";
    Texture tex{ &vkContext, ResourceId{ &imgName, Hash64{ imgName.c_str() } } };

    tex.setGenerateMipmapsOnLoad(false);

    cmdBuff.beginRecording();
    {
        tex.load();
        // load as many as you like...
    }
    cmdBuff.endRecording();
    cmdBuff.submitAndWaitComplete(vkContext.getGraphisQueue());
    cmdBuff.reset();

    tex.releaseStagingImage();

    // TODO mipmaps!
    // TODO write a simple mipmapping test:
    //
    // - draw a screen quad for each level of the image and play 
    // with the lod to use the corresponding mip for each quad (or something like that)
    //
    // - a test for anisotropic filtering is also nice.
    // draw a plane with a checkerboard texture and place the camera at a near edge-on angle
}
//*/

// - render targets/buffers
// - index/vertex buffers
// - shader uniform buffers
// - model loading (assimp?)
// - debug draw (bring up dd lib?)
//
// Other TODOs: 
// - swap chain re-creation (needed for window resize!)
// - better GPU selection (don't just grab GPU 0, try to find the best match)
// - Do some RenderDoc GPU profiling!

// ================================================================================================

class VkAppHelloTriangle final
    : public VulkanDemoApp
{
public:
    static VulkanDemoApp * createInstance(const StartupOptions & options)
    {
        return new VkAppHelloTriangle{ options };
    }

    VkAppHelloTriangle(const StartupOptions & options)
        : VulkanDemoApp{ options }
    {
        createUniformBuffer(m_vkContext);
        createDescriptorPool(m_vkContext);
        createDescriptorSetLayout(m_vkContext);
        createDescriptorSet(m_vkContext);
        createPipeline(m_vkContext);

        m_window.onResize = [](const Size2D newSize)
        {
            Log::debugF("Resize[w=%i, h=%i]", newSize.width, newSize.height);
        };

        m_window.onRedraw = [this]()
        {
            m_vkContext.beginFrame();

            vkWaitForFences(m_vkContext.getVkDeviceHandle(), 1, &currentFence, VK_TRUE, InfiniteFenceWaitTimeout);
            vkResetFences(m_vkContext.getVkDeviceHandle(), 1, &currentFence);
            vkResetCommandBuffer(currentCmdBuffer, 0);

            createCommandBuffers(m_vkContext);

            auto submitBuffers = make_array_view(&currentCmdBuffer, 1);
            m_vkContext.endFrame(submitBuffers, currentFence);

            nextBuffer = (nextBuffer + 1) % 3;
            currentCmdBuffer = cmdBuffers[nextBuffer]->getVkCmdBufferHandle();
            currentFence = bufferFences[nextBuffer];
        };
    }
};

VULKAN_DEMO_REGISTER_APP(VkAppHelloTriangle);
