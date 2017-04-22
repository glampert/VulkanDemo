#pragma once

// ================================================================================================
// File: VkToolbox/PipelineState.hpp
// Author: Guilherme R. Lampert
// Created on: 30/03/17
// Brief: Vulkan PSO helpers.
// ================================================================================================

#include "VulkanContext.hpp"

namespace VkToolbox
{

// ========================================================
// class PipelineStateLayout:
// ========================================================

class PipelineStateLayout final
{
public:

    explicit PipelineStateLayout(const VulkanContext & vkContext);
    PipelineStateLayout(const VulkanContext & vkContext,
                        array_view<const VkDescriptorSetLayout> descriptorSetLayouts);
    ~PipelineStateLayout();

    PipelineStateLayout(const PipelineStateLayout &) = delete;
    PipelineStateLayout & operator = (const PipelineStateLayout &) = delete;

    void initialize(array_view<const VkDescriptorSetLayout> descriptorSetLayouts);
    void shutdown();
    bool isInitialized() const;

    const VulkanContext & context() const { return *m_vkContext; }
    VkPipelineLayout pipelineLayoutHandle() const { return m_pipelineLayoutHandle; }
    operator VkPipelineLayout() const { return m_pipelineLayoutHandle; }

private:

    VkPipelineLayout m_pipelineLayoutHandle = VK_NULL_HANDLE;
    const VulkanContext * m_vkContext;
};

// ========================================================
// struct PipelineStateBuilder:
// ========================================================

struct PipelineStateBuilder final
{
    static constexpr int MaxColorBlends  = 6; // Arbitrary
    static constexpr int MaxShaderStages = 6; // GlslShaderStage::MaxStages

    std::array<VkPipelineColorBlendAttachmentState, MaxColorBlends>  colorBlendStates;
    std::array<VkPipelineShaderStageCreateInfo,     MaxShaderStages> shaderPipelineStages;

    VkViewport                             viewport;
    VkRect2D                               scissor;
    VkPipelineViewportStateCreateInfo      viewportState;
    VkPipelineVertexInputStateCreateInfo   vertexInputState;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    VkPipelineRasterizationStateCreateInfo rasterizerState;
    VkPipelineColorBlendStateCreateInfo    colorBlendState;
    VkPipelineMultisampleStateCreateInfo   multiSamplingState;
    VkPipelineDepthStencilStateCreateInfo  depthStencilState;
    VkPipelineTessellationStateCreateInfo  tessellationState;
    VkGraphicsPipelineCreateInfo           pipelineState;

    // Sets all the defaults for you, so you only have to change the fields you care about.
    explicit PipelineStateBuilder(const bool setDefaults = true)
    {
        if (setDefaults)
        {
            defaults();
        }
    }

    // Explicitly set all the defaults for the pipeline state creation structs above.
    void defaults();
};

// ========================================================
// class PipelineStateObject:
// ========================================================

class PipelineStateObject final
{
public:

    explicit PipelineStateObject(const PipelineStateLayout & layout);
    PipelineStateObject(const PipelineStateLayout & layout, const PipelineStateBuilder & psoBuilder);
    ~PipelineStateObject();

    PipelineStateObject(const PipelineStateObject &) = delete;
    PipelineStateObject & operator = (const PipelineStateObject &) = delete;

    void initialize(const PipelineStateBuilder & psoBuilder);
    void shutdown();
    bool isInitialized() const;

    const VulkanContext & context() const { return m_layout->context(); }
    const PipelineStateLayout & layout() const { return *m_layout; }
    VkPipeline pipelineHandle() const { return m_pipelineHandle; }
    operator VkPipeline() const { return m_pipelineHandle; }

private:

    VkPipeline m_pipelineHandle = VK_NULL_HANDLE;
    const PipelineStateLayout * m_layout;
};

// ========================================================
// PipelineStateLayout inline methods:
// ========================================================

inline PipelineStateLayout::PipelineStateLayout(const VulkanContext & vkContext)
    : m_vkContext{ &vkContext }
{
}

inline PipelineStateLayout::PipelineStateLayout(const VulkanContext & vkContext,
                                                array_view<const VkDescriptorSetLayout> descriptorSetLayouts)
    : m_vkContext{ &vkContext }
{
    initialize(descriptorSetLayouts);
}

inline PipelineStateLayout::~PipelineStateLayout()
{
    shutdown();
}

inline void PipelineStateLayout::initialize(array_view<const VkDescriptorSetLayout> descriptorSetLayouts)
{
    assert(!descriptorSetLayouts.empty());
    assert(!isInitialized());

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext                  = nullptr;
    pipelineLayoutInfo.flags                  = 0;
    pipelineLayoutInfo.setLayoutCount         = static_cast<std::uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    VKTB_CHECK(vkCreatePipelineLayout(m_vkContext->deviceHandle(), &pipelineLayoutInfo,
                                      m_vkContext->allocationCallbacks(), &m_pipelineLayoutHandle));
}

inline void PipelineStateLayout::shutdown()
{
    if (m_pipelineLayoutHandle != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_vkContext->deviceHandle(), m_pipelineLayoutHandle,
                                m_vkContext->allocationCallbacks());
        m_pipelineLayoutHandle = VK_NULL_HANDLE;
    }
}

inline bool PipelineStateLayout::isInitialized() const
{
    return (m_pipelineLayoutHandle != VK_NULL_HANDLE);
}

// ========================================================
// PipelineStateBuilder inline methods:
// ========================================================

inline void PipelineStateBuilder::defaults()
{
    for (int i = 0; i < MaxColorBlends; ++i)
    {
        colorBlendStates[i] = {};
        colorBlendStates[i].colorWriteMask = ColorMaskRGBA;
    }

    for (int i = 0; i < MaxShaderStages; ++i)
    {
        shaderPipelineStages[i] = {};
    }

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 0.0f;
    viewport.height = 0.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = 0;
    scissor.extent.height = 0;

    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.pNext = nullptr;
    vertexInputState.flags = 0;
    vertexInputState.vertexBindingDescriptionCount = 0;
    vertexInputState.pVertexBindingDescriptions = nullptr;
    vertexInputState.vertexAttributeDescriptionCount = 0;
    vertexInputState.pVertexAttributeDescriptions = nullptr;

    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.pNext = nullptr;
    inputAssemblyState.flags = 0;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    rasterizerState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerState.pNext = nullptr;
    rasterizerState.flags = 0;
    rasterizerState.depthClampEnable = VK_FALSE;
    rasterizerState.rasterizerDiscardEnable = VK_FALSE;
    rasterizerState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerState.cullMode = VK_CULL_MODE_BACK_BIT; // Backface culling
    rasterizerState.frontFace = VK_FRONT_FACE_CLOCKWISE; // CW
    rasterizerState.depthBiasEnable = VK_FALSE;
    rasterizerState.depthBiasConstantFactor = 0.0f;
    rasterizerState.depthBiasClamp = 0.0f;
    rasterizerState.depthBiasSlopeFactor = 0.0f;
    rasterizerState.lineWidth = 1.0f;

    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.pNext = nullptr;
    colorBlendState.flags = 0;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
    colorBlendState.attachmentCount = 1; // Defaults to 1 (a default screen color framebuffer).
    colorBlendState.pAttachments = colorBlendStates.data();
    colorBlendState.blendConstants[0] = 0.0f;
    colorBlendState.blendConstants[1] = 0.0f;
    colorBlendState.blendConstants[2] = 0.0f;
    colorBlendState.blendConstants[3] = 0.0f;

    multiSamplingState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multiSamplingState.pNext = nullptr;
    multiSamplingState.flags = 0;
    multiSamplingState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multiSamplingState.sampleShadingEnable = VK_FALSE;
    multiSamplingState.minSampleShading = 0.0f;
    multiSamplingState.pSampleMask = nullptr;
    multiSamplingState.alphaToCoverageEnable = VK_FALSE;
    multiSamplingState.alphaToOneEnable = VK_FALSE;

    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.pNext = nullptr;
    depthStencilState.flags = 0;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // RH GL-style projection, depth-clear=1
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = {};
    depthStencilState.back = {};
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationState.pNext = nullptr;
    tessellationState.flags = 0;
    tessellationState.patchControlPoints = 0;

    pipelineState.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineState.pNext = nullptr;
    pipelineState.flags = 0;
    pipelineState.stageCount = 0;
    pipelineState.pStages = shaderPipelineStages.data();
    pipelineState.pVertexInputState = &vertexInputState;
    pipelineState.pInputAssemblyState = &inputAssemblyState;
    pipelineState.pTessellationState = &tessellationState;
    pipelineState.pViewportState = &viewportState;
    pipelineState.pRasterizationState = &rasterizerState;
    pipelineState.pMultisampleState = &multiSamplingState;
    pipelineState.pDepthStencilState = &depthStencilState;
    pipelineState.pColorBlendState = &colorBlendState;
    pipelineState.pDynamicState = nullptr;
    pipelineState.layout = VK_NULL_HANDLE;
    pipelineState.renderPass = VK_NULL_HANDLE;
    pipelineState.subpass = 0;
    pipelineState.basePipelineHandle = VK_NULL_HANDLE;
    pipelineState.basePipelineIndex = 0;
}

// ========================================================
// PipelineStateObject inline methods:
// ========================================================

inline PipelineStateObject::PipelineStateObject(const PipelineStateLayout & layout)
    : m_layout{ &layout }
{
}

inline PipelineStateObject::PipelineStateObject(const PipelineStateLayout & layout,
                                                const PipelineStateBuilder & psoBuilder)
    : m_layout{ &layout }
{
    initialize(psoBuilder);
}

inline PipelineStateObject::~PipelineStateObject()
{
    shutdown();
}

inline void PipelineStateObject::initialize(const PipelineStateBuilder & psoBuilder)
{
    assert(!isInitialized());
    VKTB_CHECK(vkCreateGraphicsPipelines(context().deviceHandle(), VK_NULL_HANDLE, 1, &psoBuilder.pipelineState,
                                         context().allocationCallbacks(), &m_pipelineHandle));
}

inline void PipelineStateObject::shutdown()
{
    if (m_pipelineHandle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(context().deviceHandle(), m_pipelineHandle, context().allocationCallbacks());
        m_pipelineHandle = VK_NULL_HANDLE;
    }
}

inline bool PipelineStateObject::isInitialized() const
{
    return (m_pipelineHandle != VK_NULL_HANDLE);
}

// ========================================================

} // namespace VkToolbox
