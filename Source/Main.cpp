
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
*/

#include <cstdio>
#include <cassert>
#include <iostream>
#include <array>

#include "External/External.hpp"

#include "VkToolbox/ResourceManager.hpp"
#include "VkToolbox/StringRegistry.hpp"
#include "VkToolbox/Log.hpp"
using namespace VkToolbox;

Hash32 get_h32() { return Hash32{ "Hello world" }; }
Hash64 get_h64() { return Hash64{ "Hello world" }; }
Hash32 get_h32z() { return Hash32{ "" }; }
Hash64 get_h64z() { return Hash64{ "" }; }

enum class Foo
{
    Bar, Baz, Fooz,
    Count
};
void enum_helpers_sample()
{
    //
    // enum_iterator:
    //

    enum_iterator<Foo> iter;

    for (auto c : iter)
    {
        std::cout << "Foo = " << static_cast<int>(c) << "\n";
    }

    std::cout << "\n";

    for (auto c = iter.begin(); c != iter.end(); ++c)
    {
        std::cout << "Foo = " << static_cast<int>(*c) << "\n";
    }

    std::cout << "\n";

    auto a = std::begin(iter);
    auto b = std::begin(iter);

    ENUM_HELPERS_ASSERT(a == b);
    ENUM_HELPERS_ASSERT(*(a++) == Foo::Bar);
    ENUM_HELPERS_ASSERT(*(++b) == Foo::Baz);

    //
    // enum_array:
    //

    const enum_array<Foo, std::string> FooStrings{ { "Bar", "Baz", "Fooz" } };

    // Keys give you the constants associated with each array index.
    for (auto k : FooStrings.keys())
    {
        std::cout << "Foo = " << static_cast<int>(k) << "\n";
    }

    std::cout << "\n";

    // Using iterators or [] give the array values.
    for (const auto & s : FooStrings)
    {
        std::cout << "Foo as string = " << s.c_str() << "\n";
    }

    std::cout << "\n";

    for (auto i = FooStrings.rbegin(); i != FooStrings.rend(); ++i)
    {
        std::cout << "Foo as string (reverse iteration) = " << (*i).c_str() << "\n";
    }

    auto x = std::begin(FooStrings);
    ENUM_HELPERS_ASSERT(*x == "Bar");
    ++x;
    ENUM_HELPERS_ASSERT(*x == "Baz");
    ++x;
    ENUM_HELPERS_ASSERT(*x == "Fooz");

    ENUM_HELPERS_ASSERT(FooStrings[Foo::Bar]  == "Bar");
    ENUM_HELPERS_ASSERT(FooStrings[Foo::Baz]  == "Baz");
    ENUM_HELPERS_ASSERT(FooStrings[Foo::Fooz] == "Fooz");
}

// ================================================================================================

#include <vulkan/vulkan.h>
#include <utility>
#include <vector>

#define VKTB_CHECK(expr)                                                                    \
    do {                                                                                    \
        const VkResult errorCode = expr;                                                    \
        if (errorCode != VK_SUCCESS)                                                        \
        {                                                                                   \
            VkToolbox::Log::fatalF("Vulkan error %s in: %s - %s(%i)",                       \
                                   vkResultToString(errorCode), #expr, __FILE__, __LINE__); \
        }                                                                                   \
    } while (0,0)

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

// ================================================================================================

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
        VkLayerProperties                  properties;
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

    // Instance data:

    // API global context.
    OwnedHandle<VkInstance> m_instance;

    // User provided allocations callbacks or nullptr.
    WeakRef<const VkAllocationCallbacks> m_allocationCallbacks;

    // Layers and extensions available for the VK Instance.
    std::vector<LayerProperties> m_instanceLayerProperties;

    // Which kind of instance did we create: Debug or Release?
    ValidationMode m_validationMode;

    // Device data:

    // The rendering device we are going to use (always GPU 0 for now).
    OwnedHandle<VkDevice> m_device;

    // Handles to the "Physical Devices", AKA the GPUs. These handles are owned by the VK instance.
    std::vector<WeakHandle<VkPhysicalDevice>> m_gpus;

    // Information about the rendering and compute queues available in the device.
    std::vector<VkQueueFamilyProperties> m_gpu0QueueProperties;

    // Information bits about the graphics hardware physical limitations. We only care about the first GPU right now.
    VkPhysicalDeviceFeatures         m_gpu0Features;
    VkPhysicalDeviceProperties       m_gpu0Properties;
    VkPhysicalDeviceMemoryProperties m_gpu0MemoryProperties;
};

// ================================================================================================

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

// ================================================================================================

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

// ================================================================================================

int main()
{
    VulkanContext vkCtx{ "VulkanDemo", 1, VulkanContext::Debug };
    vkCtx.logInstanceLayerProperties();

    //Log::setVerbosityLevel(Log::VerbosityLevel::Error);
    Log::debugF("A debug message");
    Log::warningF("A warning message");
    Log::errorF("An error message");
    //Log::fatalF("Fatal Fury!");

    {
        Hash32 hs32{ 0 };
        Hash32 hs32_2{ 0xF0F0 };
        Hash32 hss{ "hello" };

        Hash64 hs64{ 0 };
        Hash64 hs642{ 42 };
        Hash64 hs643{ "hello" };

        char buf1[64];
        std::printf("h=%s\n", hs643.toString(buf1));

        char buf2[5];
        std::printf("h2=%s\n", hs643.toString(buf2));
    }

    StringRegistry str_reg;
    {
        auto a = str_reg.get("foo");
        auto b = str_reg.get("bar");
        
        auto d = str_reg.get("foo");
        auto e = str_reg.get("bar");

        assert(a == d);
        assert(b == e);

        str_reg.shutdown();
    }

    VkInstanceCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = nullptr;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = nullptr;

    VkInstance instance = VK_NULL_HANDLE;

    VKTB_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

    GlslShaderManager shrdMgr{ nullptr };
    TextureManager txdMgr{ nullptr };

    vkDestroyInstance(instance, nullptr);

    enum_helpers_sample();

    char buffer[64];
    std::printf("hash32: %s\n",  get_h32().toString(buffer));
    std::printf("hash64: %s\n",  get_h64().toString(buffer));
    std::printf("hash32z: %s\n", get_h32z().toString(buffer));
    std::printf("hash64z: %s\n", get_h64z().toString(buffer));
    assert(get_h32() == get_h32());
    assert(get_h64() == get_h64());
    assert(get_h32() != get_h32z());
    assert(get_h64() != get_h64z());

    hash_index<> hi;
    hi.set_granularity(10);

    hash_index<> hi2{hi};
    (void)hi2;

    Vector4 v4{4.4f};
    print(v4);

    std::printf("sizeof(ResourceId) = %zu\n", sizeof(ResourceId));

    str16 s16{"hello"};
    std::printf("%s world\n", s16.c_str());
    (void)getchar();
}
