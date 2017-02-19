#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/GlslShader.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: GLSL-syntax Vulkan shader program.
// ================================================================================================

#include "Resource.hpp"
#include <array>

namespace VkToolbox
{

// ========================================================
// struct GlslShaderStage:
// ========================================================

struct GlslShaderStage final
{
    enum class Id
    {
        Vertex,
        Fragment,
        Geometry,
        Compute,
        TessellationCtrl,
        TessellationEval,

        // Number of entries in this enum - internal use.
        Count
    };

    static const enum_array<Id, const char *> IdStrings;
    static const enum_array<Id, VkShaderStageFlagBits> VkShaderStageFlags;
    static constexpr int MaxStages = static_cast<int>(Id::Count);

    Id id = Id{};
    int sourceStart  = 0;
    int sourceLength = 0;
    OwnedHandle<VkShaderModule> moduleHandle = VK_NULL_HANDLE;
};

using GlslShaderStageArray = std::array<GlslShaderStage, GlslShaderStage::MaxStages>;

// ========================================================
// GlslShaderPreproc:
// ========================================================

namespace GlslShaderPreproc
{

struct Define final
{
    str32 name;
    str32 value;
    int   index;
};

// Add or overwrite a global shader #define
int setDefine(const char * name);
int setDefine(const char * name, int value);
int setDefine(const char * name, float value);
int setDefine(const char * name, const char * value);

// Find a global shader #define. Null when not found or bad index.
const Define * findDefine(const char * name);
const Define * getDefine(int index);
int getDefinesCount();

// Set/get the include path appended to all shader #includes. Initially empty.
void setShaderIncludePath(const char * pathEndingWithSlash);
const str & getShaderIncludePath();

} // namespace GlslShaderPreproc

// ========================================================
// class GlslShader:
// ========================================================

class GlslShader final
    : public Resource
{
public:

    GlslShader(WeakRef<const VulkanContext> vkContext, ResourceId id);
    ~GlslShader();

    // Movable.
    GlslShader(GlslShader && other);
    GlslShader & operator = (GlslShader && other);

    // Resource overrides:
    bool load() override;
    void unload() override;
    void shutdown() override;
    bool isLoaded() const override;

    // Shader stage / source code access:
    const char * getSourceCode() const;
    int getStageCount() const;
    const GlslShaderStage & getStage(int index) const;

    // Create the pipeline state structs for Vulkan shader setup. Assumes the default "main"
    // entry point for std GLSL stages. Size of the out array must be >= GlslShaderStage::MaxStages.
    // Return value is the number of stages actually written.
    int getVkPipelineStages(array_view<VkPipelineShaderStageCreateInfo> outStages) const;
    int getVkPipelineStages(std::array<VkPipelineShaderStageCreateInfo, GlslShaderStage::MaxStages> * outStages) const;

private:

    // Override from Resource.
    void clear() override;

    // Creates a single VK shader module from the null-terminated GLSL source code string.
    static OwnedHandle<VkShaderModule> createShaderModule(const VulkanContext & vkContext,
                                                          const char * sourceCode,
                                                          std::size_t sourceLen);

    // Create stages from an annotated GLSL source code string. This looks for the custom tags that
    // split the string for the different shader stages, then creates a VK shader module for each tag found.
    static bool createShaderStages(const VulkanContext & vkContext, const char * sourceCode,
                                   std::size_t sourceLen, GlslShaderStageArray * outStages,
                                   int * outStageCount);

private:

    // GLSL shader string (possibly containing more than one stage).
    std::unique_ptr<char[]> m_sourceCode;

    // Shader stages initialized from the source code.
    // m_stages can be in any order, not necessarily matching the Id enum.
    int m_stageCount;
    GlslShaderStageArray m_stages;
};

// ========================================================

inline bool GlslShader::isLoaded() const
{
    return (m_sourceCode != nullptr && m_stageCount > 0);
}

inline const char * GlslShader::getSourceCode() const
{
    return m_sourceCode.get();
}

inline int GlslShader::getStageCount() const
{
    return m_stageCount;
}

inline const GlslShaderStage & GlslShader::getStage(const int index) const
{
    assert(index >= 0);
    assert(index < m_stageCount);
    return m_stages[index];
}

inline int GlslShader::getVkPipelineStages(std::array<VkPipelineShaderStageCreateInfo, GlslShaderStage::MaxStages> * outStages) const
{
    assert(outStages != nullptr);
    return getVkPipelineStages(make_array_view(*outStages));
}

// ========================================================

} // namespace VkToolbox
