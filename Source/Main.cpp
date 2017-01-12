
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

#include "VkToolbox/Log.hpp"
#include "VkToolbox/StringRegistry.hpp"
#include "VkToolbox/ResourceManager.hpp"
#include "VkToolbox/OSWindow.hpp"
#include "VkToolbox/VulkanContext.hpp"
using namespace VkToolbox;

int main()
{
    VulkanContext vkCtx{ "VulkanDemo", 1, VulkanContext::Debug };
    vkCtx.logInstanceLayerProperties();

    OSWindow osWin{ { 1024, 768, "VulkanDemo", false } };

    osWin.onResize = [](OSWindow &, const Size2D newSize)
    {
        std::printf("Resize{w=%d, h=%d}\n", newSize.width, newSize.height);
    };

    osWin.runEventLoop();

    Log::debugF("A debug message");
    Log::warningF("A warning message");
    Log::errorF("An error message");

    StringRegistry strReg;
    {
        auto a = strReg.get("foo");
        auto b = strReg.get("bar");
        
        auto d = strReg.get("foo");
        auto e = strReg.get("bar");

        assert(a == d);
        assert(b == e);

        strReg.shutdown();
    }

    (void)getchar();
}
