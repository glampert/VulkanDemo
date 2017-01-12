#pragma once

// ================================================================================================
// -*- C++ -*-
// File: VkToolbox/VulkanContext.hpp
// Author: Guilherme R. Lampert
// Created on: 11/01/17
// Brief: Main Vulkan context and device class.
// ================================================================================================

#include <vulkan/vulkan.h>
#include <vector>
#include "Utils.hpp"

namespace VkToolbox
{

class VulkanContext final
{
public:

    enum ValidationMode
    {
        Debug, Release
    };

    // Initialization.
    VulkanContext(const char * appName, int appVersion, ValidationMode validation,
                  WeakRef<const VkAllocationCallbacks> allocCbs = nullptr);

    // Not copyable.
    VulkanContext(const VulkanContext &) = delete;
    VulkanContext & operator = (const VulkanContext &) = delete;

    // Shutdown.
    ~VulkanContext();

    // Prints layer and extension information to the log.
    void logInstanceLayerProperties() const;

    // Mode queries:
    bool isDebug() const;
    bool isRelease() const;

    // Device queries:
    WeakHandle<VkDevice> getDevice() const;
    const VkPhysicalDeviceFeatures & getDeviceFeatures() const;
    const VkPhysicalDeviceProperties & getDeviceProperties() const;
    const VkPhysicalDeviceMemoryProperties & getDeviceMemoryProperties() const;

private:

    struct LayerProperties
    {
        VkLayerProperties properties;
        std::vector<VkExtensionProperties> extensions;
    };

    // Instance creation helpers:
    void initInstanceLayerProperties();
    void initInstanceExtensionProperties(LayerProperties & layerProps);
    void initInstance(const char * appName, int appVersion, ValidationMode validation,
                      WeakRef<const VkAllocationCallbacks> allocCbs);

    // Device creation helpers:
    void initDevice();
    void enumerateGpus();

private:

    //
    // Instance data:
    //

    // API global context.
    OwnedHandle<VkInstance> m_instance;

    // User provided allocations callbacks or nullptr.
    WeakRef<const VkAllocationCallbacks> m_allocationCallbacks;

    // Layers and extensions available for the VK Instance.
    std::vector<LayerProperties> m_instanceLayerProperties;

    // Which kind of instance did we create: Debug or Release?
    ValidationMode m_validationMode;

    //
    // Device data:
    //

    // The rendering device we are going to use (always GPU 0 for now).
    OwnedHandle<VkDevice> m_device;

    // Handles to the "Physical Devices", AKA the GPUs. These handles are owned by the VK instance.
    // Vulkan allows explicitly selecting the device you want to use, on systems with more than one
    // GPU (e.g. SLI, Crossfire, etc). We will be sticking to a single GPU in this demo for simplicity.
    std::vector<WeakHandle<VkPhysicalDevice>> m_gpus;

    // Information about the rendering and compute queues available in the device.
    std::vector<VkQueueFamilyProperties> m_gpu0QueueProperties;

    // Information bits about the graphics hardware physical limits. We only care about the first GPU for now.
    VkPhysicalDeviceFeatures         m_gpu0Features;
    VkPhysicalDeviceProperties       m_gpu0Properties;
    VkPhysicalDeviceMemoryProperties m_gpu0MemoryProperties;
};

// ========================================================
// VulkanContext inline methods:
// ========================================================

inline bool VulkanContext::isDebug() const
{
    return m_validationMode == Debug;
}

inline bool VulkanContext::isRelease() const
{
    return m_validationMode == Release;
}

inline WeakHandle<VkDevice> VulkanContext::getDevice() const
{
    return m_device;
}

inline const VkPhysicalDeviceMemoryProperties & VulkanContext::getDeviceMemoryProperties() const
{
    return m_gpu0MemoryProperties;
}

inline const VkPhysicalDeviceProperties & VulkanContext::getDeviceProperties() const
{
    return m_gpu0Properties;
}

inline const VkPhysicalDeviceFeatures & VulkanContext::getDeviceFeatures() const
{
    return m_gpu0Features;
}

// ========================================================
// Helpers free functions / macros:
// ========================================================

// Checks a VkResult returned from the expression and logs a fatal error if it is not VK_SUCCESS.
#define VKTB_CHECK(expr) \
    do { \
        const VkResult errorCode = expr; \
        if (errorCode != VK_SUCCESS) \
        { \
            VkToolbox::Log::fatalF("Vulkan error %s in: %s - %s(%i)", \
                                   VkToolbox::vkResultToString(errorCode), \
                                   #expr, __FILE__, __LINE__); \
        } \
    } while (0,0)

// Printable null-terminated C-string from the VkResult error code.
// Note: The returned string is statically allocated and must not be freed.
const char * vkResultToString(const VkResult res);

} // namespace VkToolbox
