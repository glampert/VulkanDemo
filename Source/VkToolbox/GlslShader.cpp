
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/GlslShader.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: GLSL-syntax Vulkan shader program.
// ================================================================================================

#include "GlslShader.hpp"

namespace VkToolbox
{

// ========================================================
// GlslShaderStage enum lists:
// ========================================================

const enum_array<GlslShaderStage::Id, const char *> GlslShaderStage::IdStrings = {{
    "Vertex",
    "Fragment",
    "Geometry",
    "Compute",
    "TessellationCtrl",
    "TessellationEval"
}};

const enum_array<GlslShaderStage::Id, VkShaderStageFlagBits> GlslShaderStage::VkShaderStageFlags = {{
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    VK_SHADER_STAGE_GEOMETRY_BIT,
    VK_SHADER_STAGE_COMPUTE_BIT,
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
}};

// ========================================================
// class GlslShader:
// ========================================================

GlslShader::GlslShader(WeakRef<const VulkanContext> vkContext, ResourceId id)
    : Resource{ vkContext, id }
    , m_sourceCode{ nullptr }
    , m_stageCount{ 0 }
    , m_stages{}
{
}

GlslShader::~GlslShader()
{
    GlslShader::shutdown();
}

GlslShader::GlslShader(GlslShader && other)
    : Resource{ other.m_vkContext, other.m_resId }
    , m_sourceCode{ std::move(other.m_sourceCode) }
    , m_stageCount{ other.m_stageCount }
    , m_stages{ other.m_stages }
{
    other.clear();
}

GlslShader & GlslShader::operator = (GlslShader && other)
{
    GlslShader::shutdown();

    m_vkContext  = other.m_vkContext;
    m_resId      = other.m_resId;
    m_sourceCode = std::move(other.m_sourceCode);
    m_stageCount = other.m_stageCount;
    m_stages     = other.m_stages;

    other.clear();
    return *this;
}

bool GlslShader::load()
{
    const char * const name = getId().getName();

    if (isShutdown())
    {
        Log::warningF("Resource %s is already shutdown and cannot be loaded!", name);
        return false;
    }

    std::size_t newSourceSize = 0;
    std::unique_ptr<char[]> newSourceCode = loadTextFile(name, &newSourceSize);
    if (newSourceCode == nullptr || newSourceSize == 0)
    {
        Log::warningF("Couldn't load shader source file '%s'", name);
        return false;
    }

    int newStageCount = 0;
    GlslShaderStageArray newStages{};
    if (!createShaderStages(getVkContext(), newSourceCode.get(), newSourceSize, &newStages, &newStageCount))
    {
        Log::warningF("Failed to create stages for shader %s!", name);
        return false;
    }

    GlslShader::unload();

    m_sourceCode = std::move(newSourceCode);
    m_stageCount = newStageCount;
    m_stages     = newStages;

    return true;
}

void GlslShader::unload()
{
    // Release the old stage modules if needed:
    if (m_stageCount > 0)
    {
        WeakHandle<VkDevice> device = getVkContext().getDevice();
        WeakRef<const VkAllocationCallbacks> allocator = getVkContext().getAllocationCallbacks();
        for (int s = 0; s < m_stageCount; ++s)
        {
            vkDestroyShaderModule(device, m_stages[s].moduleHandle, allocator);
        }
        m_stageCount = 0;
    }
    m_sourceCode = nullptr;
}

void GlslShader::shutdown()
{
    GlslShader::unload();
    Resource::clear();
}

void GlslShader::clear()
{
    Resource::clear();
    m_sourceCode = nullptr;
    m_stageCount = 0;
    m_stages.fill({});
}

int GlslShader::getVkPipelineStages(array_view<VkPipelineShaderStageCreateInfo> outStages) const
{
    assert(outStages != nullptr);
    assert(outStages.size() >= GlslShaderStage::MaxStages);
    assert(getStageCount()  <= GlslShaderStage::MaxStages);

    const int count = getStageCount();
    for (int s = 0; s < count; ++s)
    {
        outStages[s].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        outStages[s].pNext  = nullptr;
        outStages[s].flags  = 0;
        outStages[s].stage  = GlslShaderStage::VkShaderStageFlags[m_stages[s].id];
        outStages[s].module = m_stages[s].moduleHandle;
        outStages[s].pName  = "main";
        outStages[s].pSpecializationInfo = nullptr;
    }
    return count;
}

OwnedHandle<VkShaderModule> GlslShader::createShaderModule(const VulkanContext & vkContext,
                                                           const char * const sourceCode,
                                                           const std::size_t sourceLen)
{
    VkShaderModuleCreateInfo shaderModuleInfo = {};
    shaderModuleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleInfo.pNext    = nullptr;
    shaderModuleInfo.flags    = 0;
    shaderModuleInfo.codeSize = sourceLen;
    shaderModuleInfo.pCode    = reinterpret_cast<const std::uint32_t *>(sourceCode);

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    const VkResult result = vkCreateShaderModule(vkContext.getDevice(), &shaderModuleInfo,
                                                 vkContext.getAllocationCallbacks(), &shaderModule);

    if (result != VK_SUCCESS || shaderModule == VK_NULL_HANDLE)
    {
        VKTB_LOG_ERROR(result, "Failed to create shader module!");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

bool GlslShader::createShaderStages(const VulkanContext & vkContext, const char * const sourceCode,
                                    const std::size_t sourceLen, GlslShaderStageArray * outStages,
                                    int * outStageCount)
{
    assert(sourceCode    != nullptr);
    assert(outStages     != nullptr);
    assert(outStageCount != nullptr);

    int numStagesFound = 0;
    std::array<const char *, GlslShaderStage::MaxStages> splitSources;
    splitSources.fill(nullptr);

    // Will need a copy since we modify the source string.
    std::unique_ptr<char[]> tempSrc{ new char[sourceLen + 1] };
    char * ptr = tempSrc.get();

    // Copy with null terminator and all:
    std::memcpy(ptr, sourceCode, sourceLen + 1);

    // Walk the source string, looking for our custom tags. When we find a tag, change the hash
    // to a null char and grab the pointer to the start of the next line for the desired shader.
    while (ptr[0] != '\0')
    {
        while (ptr[0] != '\0' && ptr[0] != '#')
        {
            ++ptr;
        }

        if (ptr[0] == '#') // Found a preprocessor directive?
        {
            char * hash = ptr;
            const char * tag = ptr + 1;
            const char ** shaderSrcPtr = nullptr;

            // See if we found one of our custom tags:
            if (strStartsWith(tag, "glsl_vertex"))
            {
                shaderSrcPtr = &splitSources[static_cast<int>(GlslShaderStage::Id::Vertex)];
            }
            else if (strStartsWith(tag, "glsl_fragment"))
            {
                shaderSrcPtr = &splitSources[static_cast<int>(GlslShaderStage::Id::Fragment)];
            }
            else if (strStartsWith(tag, "glsl_geometry"))
            {
                shaderSrcPtr = &splitSources[static_cast<int>(GlslShaderStage::Id::Geometry)];
            }
            else if (strStartsWith(tag, "glsl_compute"))
            {
                shaderSrcPtr = &splitSources[static_cast<int>(GlslShaderStage::Id::Compute)];
            }
            else if (strStartsWith(tag, "glsl_tess_ctrl"))
            {
                shaderSrcPtr = &splitSources[static_cast<int>(GlslShaderStage::Id::TessellationCtrl)];
            }
            else if (strStartsWith(tag, "glsl_tess_eval"))
            {
                shaderSrcPtr = &splitSources[static_cast<int>(GlslShaderStage::Id::TessellationEval)];
            }

            // Advance the ptr to the end of the line or the end of the string, whichever is first
            while (ptr[0] != '\0' && ptr[0] != '\n' && ptr[0] != '\r')
            {
                ++ptr;
            }

            if (shaderSrcPtr != nullptr)
            {
                (*shaderSrcPtr) = ptr;
                ++numStagesFound;

                // If it is a valid tag, then null out the leading # to close the previous shader.
                hash[0] = '\0';
            }
        }
    }

    // Probably missing the shader type tags!
    if (numStagesFound == 0)
    {
        return false;
    }

    int failedCount = 0;
    int stageCount  = 0;

    for (int s = 0; s < GlslShaderStage::MaxStages; ++s)
    {
        if (splitSources[s] == nullptr)
        {
            continue;
        }

        const auto sourceStart  = static_cast<int>(splitSources[s] - tempSrc.get());
        const auto sourceLength = static_cast<int>(std::strlen(splitSources[s]));

        VkShaderModule moduleHandle = createShaderModule(vkContext, splitSources[s], sourceLength);
        if (moduleHandle != VK_NULL_HANDLE)
        {
            GlslShaderStage & stage = (*outStages)[stageCount++];
            stage.id                = GlslShaderStage::Id(s);
            stage.sourceStart       = sourceStart;
            stage.sourceLength      = sourceLength;
            stage.moduleHandle      = moduleHandle;
        }
        else
        {
            ++failedCount;
        }
    }

    // Throw everything away and return false if a shader stage failed.
    if (failedCount != 0)
    {
        WeakHandle<VkDevice> device = vkContext.getDevice();
        WeakRef<const VkAllocationCallbacks> allocator = vkContext.getAllocationCallbacks();

        for (int s = 0; s < stageCount; ++s)
        {
            vkDestroyShaderModule(device, (*outStages)[s].moduleHandle, allocator);
        }

        outStages->fill({});
        (*outStageCount) = 0;
        return false;
    }

    (*outStageCount) = stageCount;
    return true;
}

// ========================================================
// GlslShaderPreproc:
// ========================================================

namespace GlslShaderPreproc
{

// ========================================================
// Globals:

static str256 s_globalShaderIncPath;
static std::vector<Define> s_globalDefines;

// ========================================================
// Internal helpers:

static Define * findDefineInternal(const char * const name)
{
    assert(name != nullptr);
    assert(name[0] != '\0');
    
    for (Define & def : s_globalDefines)
    {
        if (def.name == name)
        {
            return &def;
        }
    }
    return nullptr;
}

static Define * setDefineInternal(const char * const name)
{
    assert(name != nullptr);
    assert(name[0] != '\0');

    Define newDefine;
    newDefine.name  = name;
    newDefine.index = getDefinesCount();

    s_globalDefines.push_back(newDefine);
    return &s_globalDefines.back();
}

// ========================================================
// Shader defines/macros:

int setDefine(const char * const name)
{
    Define * def = findDefineInternal(name);
    if (def != nullptr)
    {
        def->name = name;
    }
    else
    {
        def = setDefineInternal(name);
    }
    def->value.clear();
    return def->index;
}

int setDefine(const char * const name, const int value)
{
    Define * def = findDefineInternal(name);
    if (def != nullptr)
    {
        def->name = name;
    }
    else
    {
        def = setDefineInternal(name);
    }
    def->value.setf("%i", value);
    return def->index;
}

int setDefine(const char * const name, const float value)
{
    Define * def = findDefineInternal(name);
    if (def != nullptr)
    {
        def->name = name;
    }
    else
    {
        def = setDefineInternal(name);
    }
    def->value.setf("%f", value);
    return def->index;
}

int setDefine(const char * const name, const char * const value)
{
    Define * def = findDefineInternal(name);
    if (def != nullptr)
    {
        def->name = name;
    }
    else
    {
        def = setDefineInternal(name);
    }
    def->value = value;
    return def->index;
}

const Define * findDefine(const char * const name)
{
    return findDefineInternal(name);
}

const Define * getDefine(const int index)
{
    if (index < 0 || index > getDefinesCount())
    {
        return nullptr;
    }
    return &s_globalDefines[index];
}

int getDefinesCount()
{
    return narrowCast<int>(s_globalDefines.size());
}

// ========================================================
// Shader include path:

void setShaderIncludePath(const char * const pathEndingWithSlash)
{
    assert(pathEndingWithSlash != nullptr);
    s_globalShaderIncPath = pathEndingWithSlash;
}

const str & getShaderIncludePath()
{
    return s_globalShaderIncPath;
}

// ========================================================

} // namespace GlslShaderPreproc
} // namespace VkToolbox
