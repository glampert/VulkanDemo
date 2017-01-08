
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

 Updating the submodules:
   git submodule update --recursive --remote
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
// ================================================================================================

int main()
{
    //Log::setVerbosityLevel(Log::VerbosityLevel::Error);
    Log::debugF("A debug message");
    Log::warningF("A warning message");
    Log::errorF("An error message");

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

    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    assert(res == VK_SUCCESS);

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
