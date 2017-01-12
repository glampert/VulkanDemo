
// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/VulkanContext.cpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: Main Vulkan context and device class.
// ================================================================================================

#include "VulkanContext.hpp"
#include "Log.hpp"

namespace VkToolbox
{

// ========================================================
// class VulkanContext:
// ========================================================

VulkanContext::VulkanContext(const char * const appName, const int appVersion, const ValidationMode validation,
                             WeakRef<const VkAllocationCallbacks> allocCbs)
    : m_instance{ VK_NULL_HANDLE }
    , m_allocationCallbacks{ nullptr }
    , m_validationMode{ Debug }
    , m_device{ VK_NULL_HANDLE }
    , m_gpu0Features{}
    , m_gpu0Properties{}
    , m_gpu0MemoryProperties{}
{
    Log::debugF("Initializing Vulkan API context...");

    initInstanceLayerProperties();
    initInstance(appName, appVersion, validation, allocCbs);
    initDevice();
}

VulkanContext::~VulkanContext()
{
    Log::debugF("Destroying Vulkan API context...");

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, m_allocationCallbacks);
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, m_allocationCallbacks);
    }
}

void VulkanContext::initInstanceLayerProperties()
{
    std::vector<VkLayerProperties> layerProperties;
    std::uint32_t instanceLayerCount;
    VkResult res;

    /*
     * It's possible, though very rare, that the number of
     * instance layers could change. For example, installing something
     * could include new layers that the loader would pick up
     * between the initial query for the count and the
     * request for VkLayerProperties. The loader indicates that
     * by returning a VK_INCOMPLETE status and will update the
     * the count parameter.
     * The count parameter will be updated with the number of
     * entries loaded into the data pointer - in case the number
     * of layers went down or is smaller than the size given.
     */
    do {
        instanceLayerCount = 0;
        res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        if (res != VK_SUCCESS || instanceLayerCount == 0)
        {
            return;
        }

        layerProperties.resize(instanceLayerCount);
        res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, layerProperties.data());
    } while (res == VK_INCOMPLETE);

    // Now gather the extension list for each instance layer:
    for (std::uint32_t l = 0; l < instanceLayerCount; ++l)
    {
        LayerProperties myLayerProps;
        myLayerProps.properties = layerProperties[l];

        initInstanceExtensionProperties(myLayerProps);
        m_instanceLayerProperties.emplace_back(std::move(myLayerProps));
    }
}

void VulkanContext::initInstanceExtensionProperties(LayerProperties & layerProps)
{
    VkResult res;
    do {
        const char * const layerName = layerProps.properties.layerName;
        std::uint32_t instanceExtensionCount = 0;

        res = vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount, nullptr);
        if (res != VK_SUCCESS || instanceExtensionCount == 0)
        {
            return;
        }

        layerProps.extensions.resize(instanceExtensionCount);
        auto instanceExtensions = layerProps.extensions.data();

        res = vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount, instanceExtensions);
    } while (res == VK_INCOMPLETE);
}

void VulkanContext::initInstance(const char * const appName, const int appVersion, const ValidationMode validation,
                                 WeakRef<const VkAllocationCallbacks> allocCbs)
{
    std::uint32_t instanceLayerCount;
    const char ** instanceLayerNames;
    static const char * instanceLayerNamesDebug[] = { "VK_LAYER_LUNARG_standard_validation" };

    if (validation == Debug)
    {
        Log::debugF("Creating VK Instance with debug validation...");
        instanceLayerNames = instanceLayerNamesDebug;
        instanceLayerCount = static_cast<std::uint32_t>(arrayLength(instanceLayerNamesDebug));
    }
    else
    {
        Log::debugF("Creating VK Instance without validation (Release)...");
        instanceLayerNames = nullptr;
        instanceLayerCount = 0;
    }

    VkApplicationInfo appInfo        = {};
    appInfo.sType                    = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext                    = nullptr;
    appInfo.pApplicationName         = appName;
    appInfo.applicationVersion       = appVersion;
    appInfo.pEngineName              = appName;
    appInfo.engineVersion            = appVersion;
    appInfo.apiVersion               = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo    = {};
    instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pNext                   = nullptr;
    instInfo.flags                   = 0;
    instInfo.pApplicationInfo        = &appInfo;
    instInfo.enabledLayerCount       = instanceLayerCount;
    instInfo.ppEnabledLayerNames     = instanceLayerNames;
    instInfo.enabledExtensionCount   = 0;
    instInfo.ppEnabledExtensionNames = nullptr;

    VKTB_CHECK(vkCreateInstance(&instInfo, allocCbs, &m_instance));
    assert(m_instance != VK_NULL_HANDLE);

    // Remember the set of callbacks for when destroying the instance.
    m_allocationCallbacks = allocCbs;
    m_validationMode      = validation;

    Log::debugF("VK Instance created!");
}

void VulkanContext::initDevice()
{
    enumerateGpus();

    //TODO

    Log::debugF("VK Device created for GPU 0!");
}

void VulkanContext::enumerateGpus()
{
    std::uint32_t gpuCount = 0;
    std::uint32_t queueFamilyCount = 0;

    VKTB_CHECK(vkEnumeratePhysicalDevices(m_instance, &gpuCount, nullptr));
    assert(gpuCount >= 1);

    m_gpus.resize(gpuCount);

    VKTB_CHECK(vkEnumeratePhysicalDevices(m_instance, &gpuCount, m_gpus.data()));
    assert(gpuCount >= 1);

    // For now we only care about GPU 0 - no support for multi-GPU systems.

    vkGetPhysicalDeviceQueueFamilyProperties(m_gpus[0], &queueFamilyCount, nullptr);
    assert(queueFamilyCount >= 1);

    m_gpu0QueueProperties.resize(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(m_gpus[0], &queueFamilyCount, m_gpu0QueueProperties.data());
    assert(queueFamilyCount >= 1);

    // This is as good a place as any to do this:
    vkGetPhysicalDeviceFeatures(m_gpus[0], &m_gpu0Features);
    vkGetPhysicalDeviceProperties(m_gpus[0], &m_gpu0Properties);
    vkGetPhysicalDeviceMemoryProperties(m_gpus[0], &m_gpu0MemoryProperties);

    // Minimal debug printing:
    Log::debugF("Found %i physical GPUs", gpuCount);
    Log::debugF("GPU 0 has %i queues", queueFamilyCount);
    Log::debugF("GPU 0 name: %s", m_gpu0Properties.deviceName);
}

void VulkanContext::logInstanceLayerProperties() const
{
    Log::debugF("------ VK Instance Layer properties ------");

    for (const LayerProperties & prop : m_instanceLayerProperties)
    {
        Log::debugF("VK Layer %s - \"%s\"", prop.properties.layerName, prop.properties.description);
        Log::debugF("Extensions for Layer %s:", prop.properties.layerName);

        if (!prop.extensions.empty())
        {
            for (const VkExtensionProperties & ext : prop.extensions)
            {
                Log::debugF("-> %s", ext.extensionName);
            }
        }
        else
        {
            Log::debugF("<no extensions>");
        }
    }

    Log::debugF("------------------------------------------");
}

// ========================================================
// Helpers free functions:
// ========================================================

const char * vkResultToString(const VkResult res)
{
    #define CASE_(x) case x : return #x
    switch (res)
    {
    CASE_(VK_SUCCESS);
    CASE_(VK_NOT_READY);
    CASE_(VK_TIMEOUT);
    CASE_(VK_EVENT_SET);
    CASE_(VK_EVENT_RESET);
    CASE_(VK_INCOMPLETE);
    CASE_(VK_ERROR_OUT_OF_HOST_MEMORY);
    CASE_(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CASE_(VK_ERROR_INITIALIZATION_FAILED);
    CASE_(VK_ERROR_DEVICE_LOST);
    CASE_(VK_ERROR_MEMORY_MAP_FAILED);
    CASE_(VK_ERROR_LAYER_NOT_PRESENT);
    CASE_(VK_ERROR_EXTENSION_NOT_PRESENT);
    CASE_(VK_ERROR_FEATURE_NOT_PRESENT);
    CASE_(VK_ERROR_INCOMPATIBLE_DRIVER);
    CASE_(VK_ERROR_TOO_MANY_OBJECTS);
    CASE_(VK_ERROR_FORMAT_NOT_SUPPORTED);
    CASE_(VK_ERROR_FRAGMENTED_POOL);
    CASE_(VK_ERROR_SURFACE_LOST_KHR);
    CASE_(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    CASE_(VK_SUBOPTIMAL_KHR);
    CASE_(VK_ERROR_OUT_OF_DATE_KHR);
    CASE_(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    CASE_(VK_ERROR_VALIDATION_FAILED_EXT);
    CASE_(VK_ERROR_INVALID_SHADER_NV);
    CASE_(VK_RESULT_RANGE_SIZE);
    default : return "UNKNOWN";
    } // switch
    #undef CASE_
}

} // namespace VkToolbox
