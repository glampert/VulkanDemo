
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

 In the shell
   git submodule add git@github.com:glampert/foo.git
   git commit -am 'added foo module'
   git push
*/

#include <cstdio>
#include <cassert>

#include "External/str/str.hpp"
#include "External/hash_index/hash_index.hpp"
#include "External/vectormath/vectormath.hpp"

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
    assert(get_h32() == get_h32());
    assert(get_h64() == get_h64());
    assert(get_h32() != get_h32z());
    assert(get_h64() != get_h64z());

    hash_index<> hi;
    hi.set_granularity(10);

    Vector4 v4{4.4f};
    print(v4);

    str16 s16{"hello"};
    std::printf("%s world\n", s16.c_str());
    getchar();
}
