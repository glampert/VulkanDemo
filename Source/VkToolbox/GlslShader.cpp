
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/GlslShader.cpp
// Author: Guilherme R. Lampert
// Created on: 03/01/17
// Brief: GLSL-syntax Vulkan shader program.
// ================================================================================================

#include "GlslShader.hpp"

// https://github.com/KhronosGroup/glslang
#include "glslang/glslang/Public/ShaderLang.h"
#include "glslang/SPIRV/GlslangToSpv.h"

namespace VkToolbox
{

// ========================================================
// GLSL to SPIR-V compilation helpers:
// ========================================================

// #version 150 assumed if unspecified - forward compatible only.
static constexpr int  GlslDefaultVersion = 150;
static constexpr bool GlslFwdCompatible  = true;

static const TBuiltInResource * GlslGetBuiltInResources()
{
    static TBuiltInResource s_resources{};
    static bool s_initialized = false;

    if (!s_initialized)
    {
        s_resources.maxLights = 32;
        s_resources.maxClipPlanes = 6;
        s_resources.maxTextureUnits = 32;
        s_resources.maxTextureCoords = 32;
        s_resources.maxVertexAttribs = 64;
        s_resources.maxVertexUniformComponents = 4096;
        s_resources.maxVaryingFloats = 64;
        s_resources.maxVertexTextureImageUnits = 32;
        s_resources.maxCombinedTextureImageUnits = 80;
        s_resources.maxTextureImageUnits = 32;
        s_resources.maxFragmentUniformComponents = 4096;
        s_resources.maxDrawBuffers = 32;
        s_resources.maxVertexUniformVectors = 128;
        s_resources.maxVaryingVectors = 8;
        s_resources.maxFragmentUniformVectors = 16;
        s_resources.maxVertexOutputVectors = 16;
        s_resources.maxFragmentInputVectors = 15;
        s_resources.minProgramTexelOffset = -8;
        s_resources.maxProgramTexelOffset = 7;
        s_resources.maxClipDistances = 8;
        s_resources.maxComputeWorkGroupCountX = 65535;
        s_resources.maxComputeWorkGroupCountY = 65535;
        s_resources.maxComputeWorkGroupCountZ = 65535;
        s_resources.maxComputeWorkGroupSizeX = 1024;
        s_resources.maxComputeWorkGroupSizeY = 1024;
        s_resources.maxComputeWorkGroupSizeZ = 64;
        s_resources.maxComputeUniformComponents = 1024;
        s_resources.maxComputeTextureImageUnits = 16;
        s_resources.maxComputeImageUniforms = 8;
        s_resources.maxComputeAtomicCounters = 8;
        s_resources.maxComputeAtomicCounterBuffers = 1;
        s_resources.maxVaryingComponents = 60;
        s_resources.maxVertexOutputComponents = 64;
        s_resources.maxGeometryInputComponents = 64;
        s_resources.maxGeometryOutputComponents = 128;
        s_resources.maxFragmentInputComponents = 128;
        s_resources.maxImageUnits = 8;
        s_resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
        s_resources.maxCombinedShaderOutputResources = 8;
        s_resources.maxImageSamples = 0;
        s_resources.maxVertexImageUniforms = 0;
        s_resources.maxTessControlImageUniforms = 0;
        s_resources.maxTessEvaluationImageUniforms = 0;
        s_resources.maxGeometryImageUniforms = 0;
        s_resources.maxFragmentImageUniforms = 8;
        s_resources.maxCombinedImageUniforms = 8;
        s_resources.maxGeometryTextureImageUnits = 16;
        s_resources.maxGeometryOutputVertices = 256;
        s_resources.maxGeometryTotalOutputComponents = 1024;
        s_resources.maxGeometryUniformComponents = 1024;
        s_resources.maxGeometryVaryingComponents = 64;
        s_resources.maxTessControlInputComponents = 128;
        s_resources.maxTessControlOutputComponents = 128;
        s_resources.maxTessControlTextureImageUnits = 16;
        s_resources.maxTessControlUniformComponents = 1024;
        s_resources.maxTessControlTotalOutputComponents = 4096;
        s_resources.maxTessEvaluationInputComponents = 128;
        s_resources.maxTessEvaluationOutputComponents = 128;
        s_resources.maxTessEvaluationTextureImageUnits = 16;
        s_resources.maxTessEvaluationUniformComponents = 1024;
        s_resources.maxTessPatchComponents = 120;
        s_resources.maxPatchVertices = 32;
        s_resources.maxTessGenLevel = 64;
        s_resources.maxViewports = 16;
        s_resources.maxVertexAtomicCounters = 0;
        s_resources.maxTessControlAtomicCounters = 0;
        s_resources.maxTessEvaluationAtomicCounters = 0;
        s_resources.maxGeometryAtomicCounters = 0;
        s_resources.maxFragmentAtomicCounters = 8;
        s_resources.maxCombinedAtomicCounters = 8;
        s_resources.maxAtomicCounterBindings = 1;
        s_resources.maxVertexAtomicCounterBuffers = 0;
        s_resources.maxTessControlAtomicCounterBuffers = 0;
        s_resources.maxTessEvaluationAtomicCounterBuffers = 0;
        s_resources.maxGeometryAtomicCounterBuffers = 0;
        s_resources.maxFragmentAtomicCounterBuffers = 1;
        s_resources.maxCombinedAtomicCounterBuffers = 1;
        s_resources.maxAtomicCounterBufferSize = 16384;
        s_resources.maxTransformFeedbackBuffers = 4;
        s_resources.maxTransformFeedbackInterleavedComponents = 64;
        s_resources.maxCullDistances = 8;
        s_resources.maxCombinedClipAndCullDistances = 8;
        s_resources.maxSamples = 4;
        s_resources.limits.nonInductiveForLoops = 1;
        s_resources.limits.whileLoops = 1;
        s_resources.limits.doWhileLoops = 1;
        s_resources.limits.generalUniformIndexing = 1;
        s_resources.limits.generalAttributeMatrixVectorIndexing = 1;
        s_resources.limits.generalVaryingIndexing = 1;
        s_resources.limits.generalSamplerIndexing = 1;
        s_resources.limits.generalVariableIndexing = 1;
        s_resources.limits.generalConstantMatrixVectorIndexing = 1;

        s_initialized = true;
        Log::debugF("GLSL TBuiltInResource instance initialized.");
    }

    return &s_resources;
}

static EShLanguage GlslFindLanguage(const VkShaderStageFlagBits shaderType)
{
    switch (shaderType)
    {
    case VK_SHADER_STAGE_VERTEX_BIT                  : return EShLangVertex;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT    : return EShLangTessControl;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT : return EShLangTessEvaluation;
    case VK_SHADER_STAGE_GEOMETRY_BIT                : return EShLangGeometry;
    case VK_SHADER_STAGE_FRAGMENT_BIT                : return EShLangFragment;
    case VK_SHADER_STAGE_COMPUTE_BIT                 : return EShLangCompute;
    default : Log::fatalF("Invalid VK shader type enum %u", static_cast<unsigned>(shaderType));
    } // switch
}

template<typename T>
static void GlslPrintWarnings(T & shdr, const char * const typeName, const char * const shaderDebugName)
{
    if (shdr.getInfoLog()[0] != '\0')
    {
        str512 msg{ shdr.getInfoLog() };
        msg.trim();

        // This warning is really noisy and is emitted for every shader compilation.
        // Not a very elegant way of silencing it, but... does the job in the lack
        // of any other option to selectively disable warnings...
        if (msg.starts_with("Warning, version") &&
            msg.ends_with("is not yet complete; most version-specific features are present, but some are missing."))
        {
            return;
        }

        Log::warningF("Info log for GLSL %s '%s':", typeName, shaderDebugName);
        Log::warningF("%s", msg.c_str());
    }
}

static bool GlslToSPIRV(const VkShaderStageFlagBits shaderType, const char * const shaderDebugName,
                        const array_view<const char *> glslSourceStrings, std::vector<std::uint32_t> * outSpirVBinary)
{
    assert(shaderDebugName   != nullptr);
    assert(glslSourceStrings != nullptr);
    assert(outSpirVBinary    != nullptr);

    const EShLanguage stage = GlslFindLanguage(shaderType);
    glslang::TShader  shader(stage);
    glslang::TProgram program;

    // Enable SPIR-V and Vulkan rules when parsing GLSL.
    const auto messages = EShMessages(EShMsgSpvRules | EShMsgVulkanRules);

    // Add the source code strings:
    shader.setStrings(glslSourceStrings.data(), narrowCast<int>(glslSourceStrings.size()));

    // Pre-process and parse:
    if (!shader.parse(GlslGetBuiltInResources(), GlslDefaultVersion, GlslFwdCompatible, messages))
    {
        Log::errorF("*** Failed to compile GLSL shader '%s'. Error log: ***", shaderDebugName);
        Log::errorF("%s", str512(shader.getInfoLog()).trim().c_str());
        Log::errorF("%s", str512(shader.getInfoDebugLog()).trim().c_str());
        Log::errorF("---------------------------------------------------");
        return false;
    }
    GlslPrintWarnings(shader, "shader", shaderDebugName);

    // Link the shader program:
    program.addShader(&shader);
    if (!program.link(messages))
    {
        Log::errorF("*** Failed to link GLSL program '%s'. Error log: ***", shaderDebugName);
        Log::errorF("%s", str512(program.getInfoLog()).trim().c_str());
        Log::errorF("%s", str512(program.getInfoDebugLog()).trim().c_str());
        Log::errorF("---------------------------------------------------");
        return false;
    }
    GlslPrintWarnings(program, "program", shaderDebugName);

    glslang::GlslangToSpv(*program.getIntermediate(stage), *outSpirVBinary);
    return true;
}

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
    , m_stages{ std::move(other.m_stages) }
{
    other.clear();
}

GlslShader & GlslShader::operator = (GlslShader && other)
{
    GlslShader::shutdown();

    m_vkContext  = other.m_vkContext;
    m_resId      = other.m_resId;
    m_sourceCode = std::move(other.m_sourceCode);
    m_stages     = std::move(other.m_stages);

    other.clear();
    return *this;
}

void GlslShader::setSourceCode(const char * const glslSource)
{
    // We will never modify the string except when deleting it.
    char * nonConstPtr = const_cast<char *>(glslSource);
    m_sourceCode.reset(nonConstPtr);
}

bool GlslShader::reloadCurrent()
{
    const char * const name = getId().getName();
    if (isShutdown())
    {
        Log::warningF("Resource %s is already shutdown and cannot be loaded!", name);
        return false;
    }

    GlslShaderStageArray newStages{};
    const auto sourceLen = std::strlen(m_sourceCode.get());
    if (!createShaderStages(getVkContext(), m_sourceCode.get(), sourceLen, &newStages, name))
    {
        Log::warningF("Failed to create stages for shader '%s'", name);
        return false;
    }

    auto oldSourceCode = std::move(m_sourceCode);

    GlslShader::unload();

    m_sourceCode = std::move(oldSourceCode);
    m_stages     = std::move(newStages);

    return true;
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

    GlslShaderStageArray newStages{};
    if (!createShaderStages(getVkContext(), newSourceCode.get(), newSourceSize, &newStages, name))
    {
        Log::warningF("Failed to create stages for shader '%s'", name);
        return false;
    }

    GlslShader::unload();

    m_sourceCode = std::move(newSourceCode);
    m_stages     = std::move(newStages);

    return true;
}

void GlslShader::unload()
{
    // Release the old stage modules if needed:
    if (!m_stages.empty())
    {
        WeakHandle<VkDevice> device = getVkContext().getDevice();
        WeakRef<const VkAllocationCallbacks> allocator = getVkContext().getAllocationCallbacks();
        for (int s = 0; s < m_stages.size(); ++s)
        {
            vkDestroyShaderModule(device, m_stages[s].moduleHandle, allocator);
        }
        m_stages.clear();
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
    m_stages.clear();
}

int GlslShader::getVkPipelineStages(array_view<VkPipelineShaderStageCreateInfo> outStages) const
{
    assert(outStages != nullptr);
    assert(outStages.size() >= GlslShaderStage::MaxStages);
    assert(m_stages.size()  <= GlslShaderStage::MaxStages);

    for (int s = 0; s < m_stages.size(); ++s)
    {
        outStages[s].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        outStages[s].pNext  = nullptr;
        outStages[s].flags  = 0;
        outStages[s].stage  = GlslShaderStage::VkShaderStageFlags[m_stages[s].id];
        outStages[s].module = m_stages[s].moduleHandle;
        outStages[s].pName  = "main";
        outStages[s].pSpecializationInfo = nullptr;
    }
    return m_stages.size();
}

OwnedHandle<VkShaderModule> GlslShader::createShaderModule(const VulkanContext & vkContext,
                                                           const VkShaderStageFlagBits shaderType,
                                                           const char * const shaderDebugName,
                                                           const array_view<const char *> glslSourceStrings)
{
    std::vector<std::uint32_t> spirvBinary;
    if (!GlslToSPIRV(shaderType, shaderDebugName, glslSourceStrings, &spirvBinary))
    {
        Log::errorF("GLSL to SPIR-V compilation failed for shader '%s'", shaderDebugName);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo shaderModuleInfo = {};
    shaderModuleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleInfo.pNext    = nullptr;
    shaderModuleInfo.flags    = 0;
    shaderModuleInfo.codeSize = (spirvBinary.size() * sizeof(std::uint32_t)); // size in bytes!
    shaderModuleInfo.pCode    = spirvBinary.data();

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
                                    const char * const shaderDebugName)
{
    assert(sourceCode != nullptr);
    assert(outStages  != nullptr);

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
    for (int s = 0; s < GlslShaderStage::MaxStages; ++s)
    {
        if (splitSources[s] == nullptr)
        {
            continue;
        }

        // Default macros just get appended to the beginning as an extra source string.
        const char * srcAndMacros[] = { GlslShaderPreproc::getAllDefinesString(), splitSources[s] };
        array_view<const char *> glslSourceStrings{ srcAndMacros };

        const auto sourceStart  = static_cast<int>(splitSources[s] - tempSrc.get());
        const auto sourceLength = static_cast<int>(std::strlen(splitSources[s]));

        VkShaderModule moduleHandle = createShaderModule(vkContext, GlslShaderStage::VkShaderStageFlags[s],
                                                         shaderDebugName, glslSourceStrings);

        if (moduleHandle != VK_NULL_HANDLE)
        {
            GlslShaderStage stage;
            stage.id           = GlslShaderStage::Id(s);
            stage.sourceStart  = sourceStart;
            stage.sourceLength = sourceLength;
            stage.moduleHandle = moduleHandle;
            outStages->push(stage);
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

        for (int s = 0; s < outStages->size(); ++s)
        {
            vkDestroyShaderModule(device, (*outStages)[s].moduleHandle, allocator);
        }

        outStages->clear();
        return false;
    }

    return true;
}

// ========================================================
// Static initialization and shutdown:
// ========================================================

void GlslShader::initClass()
{
    Log::debugF("---- GlslShader::initClass ----");

    glslang::InitializeProcess();
    GlslGetBuiltInResources(); // Create the shared instance.
}

void GlslShader::shutdownClass()
{
    Log::debugF("---- GlslShader::shutdownClass ----");

    GlslShaderPreproc::shutdown();
    glslang::FinalizeProcess();
}

// ========================================================
// GlslShaderPreproc:
// ========================================================

namespace GlslShaderPreproc
{

// ========================================================
// Globals:

static int  s_glslVersionUsed       = GlslDefaultVersion;
static bool s_allDefinesStrUpToDate = true;

static str512 s_allDefinesString;
static str256 s_globalShaderIncPath;

static std::vector<Define> s_globalDefines;
static std::vector<str128> s_glslExtensions;

// ========================================================
// Internal helpers:

static Define * findDefineInternal(const char * const name, const bool mutating = true)
{
    assert(name != nullptr);
    assert(name[0] != '\0');
    
    for (Define & def : s_globalDefines)
    {
        if (def.name == name)
        {
            s_allDefinesStrUpToDate = !mutating;
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

    s_allDefinesStrUpToDate = false;

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

void setExtension(const char * extName, const char * state)
{
    str128 ext;
    ext.setf("#extension %s : %s", extName, state);
    s_glslExtensions.emplace_back(std::move(ext));
}

void setVersion(const int version)
{
    s_glslVersionUsed = version;
}

int getVersion()
{
    return s_glslVersionUsed;
}

const Define * findDefine(const char * const name)
{
    return findDefineInternal(name, false);
}

const Define * getDefine(const int index)
{
    if (index < 0 || index > getDefinesCount())
    {
        return nullptr;
    }
    return &s_globalDefines[index];
}

const char * getAllDefinesString()
{
    if (!s_allDefinesStrUpToDate)
    {
        s_allDefinesString.clear_no_free();
        s_allDefinesString.setf("#version %i\n", s_glslVersionUsed);

        for (const str128 & ext : s_glslExtensions)
        {
            s_allDefinesString += ext;
            s_allDefinesString += "\n";
        }

        for (const Define & def : s_globalDefines)
        {
            s_allDefinesString += "#define ";
            s_allDefinesString += def.name;
            s_allDefinesString += " ";
            s_allDefinesString += def.value;
            s_allDefinesString += "\n";
        }

        s_allDefinesStrUpToDate = true;
    }

    return s_allDefinesString.c_str();
}

int getDefinesCount()
{
    return narrowCast<int>(s_globalDefines.size());
}

void shutdown()
{
    s_globalDefines.clear();
    s_globalDefines.shrink_to_fit();

    s_glslExtensions.clear();
    s_glslExtensions.shrink_to_fit();

    s_allDefinesString.clear();
    s_globalShaderIncPath.clear();

    s_glslVersionUsed       = GlslDefaultVersion;
    s_allDefinesStrUpToDate = true;
}

// ========================================================
// Shader include path:

void setShaderIncludePath(const char * const pathEndingWithSlash)
{
    assert(pathEndingWithSlash != nullptr);
    s_globalShaderIncPath = pathEndingWithSlash;
}

const char * getShaderIncludePath()
{
    return s_globalShaderIncPath.c_str();
}

// ========================================================

} // namespace GlslShaderPreproc
} // namespace VkToolbox
