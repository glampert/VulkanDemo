
// ================================================================================================
// File: VkToolbox/ResourceManager.cpp
// Author: Guilherme R. Lampert
// Created on: 18/02/17
// Brief: Explicit template instantiations of the known resource managers.
// ================================================================================================

#include "ResourceManager.hpp"

namespace VkToolbox
{

// ========================================================

template<>
void ResourceManager<Texture>::beginResourceLoad()
{
    // TODO begin mainTextureStagingCmdBuffer
}

template<>
void ResourceManager<Texture>::endResourceLoad()
{
    // TODO finish and submit mainTextureStagingCmdBuffer
    // release all staging images
}

// ========================================================

template class ResourceManager<GlslShader>;
template class ResourceManager<Texture>;

// ========================================================

} // namespace VkToolbox
