#pragma once

// ================================================================================================
// File: VkToolbox/GlslShader.hpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: GLSL-syntax Vulkan shader program.
// ================================================================================================

#include "VulkanContext.hpp"
#include "Hashing.hpp"

namespace VkToolbox
{

// ========================================================

// Base path for GLSL shader files.
#define VKTB_SHADER_SOURCE_PATH "Source/Shaders/"

// If non-zero, failing to compile a shader will abort with a fatal error.
#define VKTB_GLSL_COMPILER_ERRORS_ARE_FATAL 1

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
    VkShaderModule moduleHandle = VK_NULL_HANDLE;

    bool isValid() const
    {
        return (moduleHandle != VK_NULL_HANDLE);
    }
};

using GlslShaderStageArray = FixedSizeArray<GlslShaderStage, GlslShaderStage::MaxStages>;

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

// Set #version directive and #extension directives used by all shaders.
void setExtension(const char * extName, const char * state);
void setVersion(int version, bool core, bool fwdCompat);
void setOptimizations(bool optimize, bool debug);

// Find a global shader #define. Null when not found or bad index.
const Define * findDefineByName(const char * name);
const Define * findDefineByIndex(int index);

// Number of #defines only.
int definesCount();

// Combined string with all #defines, #extensions and #version.
const char * allDefinesString();
int allDefinesStringLength();

// Set/get the include path appended to all shader #includes. Initially empty.
void setShaderIncludePath(const char * pathEndingWithSlash);
const char * shaderIncludePath();

// Removes all define, extension, version directives and clears the default include path.
void shutdown();

} // namespace GlslShaderPreproc

// ========================================================
// class GlslShader:
// ========================================================

class GlslShader final
{
public:

    // Perform static initialization and shutdown for the shared state required
    // by the GLSL to SPIR-V compiler. Initialization must be called at least
    // once before creating an instance of a GlslShader.
    static void initClass();
    static void shutdownClass();

    // Constructor/destructor:
    GlslShader(const VulkanContext & vkContext, const char * id);
    GlslShader(const VulkanContext & vkContext, StrId<str> && id);
    ~GlslShader();

    // Movable.
    GlslShader(GlslShader && other);
    GlslShader & operator = (GlslShader && other);

    // Resourcing methods:
    bool load();
    void unload();
    void shutdown();
    bool isLoaded() const;
    bool isShutdown() const;
    const VulkanContext & context() const;
    const StrId<str> & resourceId() const;

    // Reload using the current GLSL source code, rather than reloading from file.
    bool reloadCurrent();

    // Set the GLSL source code. This can be useful, for example, to create a shader
    // instance from an embedded C string, then calling reloadCurrent() to create the
    // stages. NOTE: GlslShader will take ownership of the pointer and delete it when
    // the shader class instance is destroyed.
    void setSourceCode(const char * glslSource);

    // Read-only access to the GLSL source code (owned by the shader instance).
    const char * sourceCode() const;

    // Shader stage access:
    int stageCount() const;
    const GlslShaderStage & stage(int index) const;
    const GlslShaderStage * findStageById(GlslShaderStage::Id id) const;

    // Create the pipeline state structs for Vulkan shader setup. Assumes the default "main"
    // entry point for std GLSL stages. Size of the out array must be >= GlslShaderStage::MaxStages.
    // Return value is the number of stages actually written.
    int pipelineStages(array_view<VkPipelineShaderStageCreateInfo> outStages) const;
    int pipelineStages(std::array<VkPipelineShaderStageCreateInfo, GlslShaderStage::MaxStages> * outStages) const;

private:

    void clear();

    // Creates a single VK shader module from the null-terminated GLSL source code strings.
    static VkShaderModule createShaderModule(const VulkanContext & vkContext,
                                             const VkShaderStageFlagBits shaderType,
                                             const char * const shaderDebugName,
                                             const array_view<const char *> glslSourceStrings,
                                             const array_view<const int> glslSourceStringLengths);

    // Create stages from an annotated GLSL source code string. This looks for the custom tags that
    // split the string for the different shader stages, then creates a VK shader module for each tag found.
    static bool createShaderStages(const VulkanContext & vkContext,
                                   const char * const sourceCode,
                                   const std::size_t sourceLen,
                                   const char * const shaderDebugName,
                                   GlslShaderStageArray * outStages);

private:

    const VulkanContext * m_vkContext;
    StrId<str> m_resId;

    // GLSL shader string (possibly containing more than one stage).
    std::unique_ptr<char[]> m_sourceCode;

    // Shader stages initialized from the source code.
    // Stages can be in any order, not necessarily matching the Id enum.
    GlslShaderStageArray m_stages;
};

// ========================================================

inline GlslShader::GlslShader(const VulkanContext & vkContext, const char * const id)
    : GlslShader{ vkContext, StrId<str>{ id } }
{
}

inline GlslShader::~GlslShader()
{
    shutdown();
}

inline bool GlslShader::isLoaded() const
{
    return (m_sourceCode != nullptr && !m_stages.empty());
}

inline bool GlslShader::isShutdown() const
{
    return (m_vkContext == nullptr);
}

inline const VulkanContext & GlslShader::context() const
{
    return *m_vkContext;
}

inline const StrId<str> & GlslShader::resourceId() const
{
    return m_resId;
}

inline const char * GlslShader::sourceCode() const
{
    return m_sourceCode.get();
}

inline int GlslShader::stageCount() const
{
    return m_stages.size();
}

inline const GlslShaderStage & GlslShader::stage(const int index) const
{
    return m_stages[index];
}

inline const GlslShaderStage * GlslShader::findStageById(const GlslShaderStage::Id id) const
{
    for (int s = 0; s < m_stages.size(); ++s)
    {
        if (m_stages[s].id == id)
        {
            return &m_stages[s];
        }
    }
    return nullptr;
}

inline int GlslShader::pipelineStages(std::array<VkPipelineShaderStageCreateInfo, GlslShaderStage::MaxStages> * outStages) const
{
    assert(outStages != nullptr);
    return pipelineStages(make_array_view(*outStages));
}

// ========================================================

} // namespace VkToolbox
