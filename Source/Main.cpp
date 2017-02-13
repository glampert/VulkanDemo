
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

#include "VkToolbox/Log.hpp"
#include "VkToolbox/OSWindow.hpp"
#include "VkToolbox/VulkanContext.hpp"

using namespace VkToolbox;

int main()
{
    OSWindow window{ { 1024, 768, "VulkanDemo", false } };

    window.onResize = [](OSWindow &, const Size2D newSize)
    {
        Log::debugF("Resize[w=%i, h=%i]", newSize.width, newSize.height);
    };

    VulkanContext::appName    = "VulkanDemo";
    VulkanContext::appVersion = 1;

    VulkanContext vkCtx{ &window };
    vkCtx.logInstanceLayerProperties();

    window.runEventLoop();
}
