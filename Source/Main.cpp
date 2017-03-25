
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

Vk demo idea 3:
 - Virtual Texturing revisited!
 - 256 page size with inner border (or outer?)
 - Trilinear sw filter to save memory?
 - Use compute shaders to process page id feedback buffer.
 - Explore virtual decals?
 - Compress the indirection texture to save mem?
*/

#include <direct.h>

#include "VkToolbox/Log.hpp"
#include "VkToolbox/OSWindow.hpp"
#include "VkToolbox/Image.hpp"
#include "VkToolbox/ResourceManager.hpp"
#include "VkToolbox/VulkanContext.hpp"

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

    VulkanContext::sm_appName = "VulkanDemo";
    VulkanContext::sm_appVersion = 1;
    //VulkanContext::sm_validationMode = VulkanContext::Release;

    VulkanContext vkCtx{ &window, { 1024, 768 } };
    vkCtx.logInstanceLayerProperties();

    // Test Shader:
    /*
    {
        GlslShader::initClass();

        GlslShaderPreproc::setVersion(450, true, true);
        GlslShaderPreproc::setExtension("GL_ARB_separate_shader_objects",  "enable");
        GlslShaderPreproc::setExtension("GL_ARB_shading_language_420pack", "enable");
        GlslShaderPreproc::setExtension("GL_GOOGLE_include_directive",     "enable");

        GlslShaderPreproc::setDefine("POS_LOC",       0);
        GlslShaderPreproc::setDefine("COLOR_IN_LOC",  1);
        GlslShaderPreproc::setDefine("COLOR_OUT_LOC", 0);

        GlslShaderPreproc::setShaderIncludePath("test_includes/sys/");

        str32 shdrName = "test.glsl";
        GlslShader shdr{ &vkCtx, ResourceId{ &shdrName, Hash64{ shdrName.c_str() } } };
        shdr.load();

        //TODO: need to query the uniform variables from the shader compiler!

        GlslShader::shutdownClass();
    }
    */

    // Test Image:
    /*
    {
        //Image::sm_loadOptions.forceRGBA = true;
        //Image::sm_loadOptions.flipVerticallyOnLoad = true;
        //Image::sm_loadOptions.roundDownToPowerOfTwo = true;
        //Image::sm_loadOptions.roundUpToPowerOfTwo = true;

        Image img0{ "cppreference.jpg" };
        img0.generateMipmapSurfaces();
        img0.writeAllSurfacesToFiles("img0", "png");

        Image img1{{32,32}, Color32{0,0,255}};
        img1.writeToFile("img1.png");

        Image img2;
        img2.initWithCheckerPattern({64,64}, 4);
        img2.writeToFile("img2.png");

        Image img3;
        img3.initWithCheckerPattern({256,256}, 4);
        img3.generateMipmapSurfaces();
        img3.writeAllSurfacesToFiles("img3", "png");
    }
    */

    // Test Texture:
    {
        std::printf("sizeof(Texture) = %zu", sizeof(Texture));
    }

    // - render targets/buffers
    // - index/vertex buffers
    // - shader uniform buffers
    // - model loading (assimp?)
    // - debug draw (bring up dd lib?)

    window.runEventLoop();
}
