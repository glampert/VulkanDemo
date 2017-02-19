
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

#include <direct.h>

#include "VkToolbox/Log.hpp"
#include "VkToolbox/OSWindow.hpp"
#include "VkToolbox/VulkanContext.hpp"
#include "VkToolbox/ResourceManager.hpp"

using namespace VkToolbox;

int main()
{
    char cwd[512];
    _getcwd(cwd, sizeof(cwd));
    Log::debugF("CWD = %s", cwd);

    OSWindow window{ { 1024, 768, "VulkanDemo", false } };

    window.onResize = [](OSWindow &, const Size2D newSize)
    {
        Log::debugF("Resize[w=%i, h=%i]", newSize.width, newSize.height);
    };

    VulkanContext::sm_appName    = "VulkanDemo";
    VulkanContext::sm_appVersion = 1;
//    VulkanContext::sm_validationMode = VulkanContext::Release;

    VulkanContext vkCtx{ &window, { 1024, 768 } };
    vkCtx.logInstanceLayerProperties();

    // Test shader
    {
        str32 shdrName = "simple2.glsl";
        GlslShader shdr{ &vkCtx, ResourceId{ &shdrName, Hash64{ shdrName.c_str() } } };
        shdr.load();
    }

    window.runEventLoop();
}
