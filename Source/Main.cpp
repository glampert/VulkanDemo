
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
#include "VkToolbox/ResourceManager.hpp"
using namespace VkToolbox;

Hash32 get_h32() { return Hash32{ "Hello world" }; }
Hash64 get_h64() { return Hash64{ "Hello world" }; }
Hash32 get_h32z() { return Hash32{ "" }; }
Hash64 get_h64z() { return Hash64{ "" }; }

int main()
{
    char buffer[64];
    std::printf("hash32: %s\n",  get_h32().toString(buffer));
    std::printf("hash64: %s\n",  get_h64().toString(buffer));
    std::printf("hash32z: %s\n", get_h32z().toString(buffer));
    std::printf("hash64z: %s\n", get_h64z().toString(buffer));

    std::printf("Hello world\n");
    getchar();
}
