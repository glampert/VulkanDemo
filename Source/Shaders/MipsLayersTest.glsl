//
// Vertex position and texture - used by VkAppMipMapsAndLayers.
//

// ----------------------------------------------------------------------------
#glsl_vertex
// ----------------------------------------------------------------------------

#include <VSCommon.glsl>

layout(location = VS_IN_LOCATION_POSITION)  in vec3 inVertexPosition;
layout(location = VS_IN_LOCATION_TEXCOORDS) in vec2 inVertexTexCoords;
layout(location = VS_IN_LOCATION_COLOR)     in vec4 inVertexColor;
layout(location = VS_IN_LOCATION_NORMAL)    in vec3 inVertexNormal;

layout(location = 0) out vec4 outVertexColor;
layout(location = 1) out vec2 outVertexTexCoords;
layout(location = 2) out vec3 outVertexNormal;

layout(binding = 0)
uniform Matrices
{
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 mvp;
} matrices;

void main()
{
    gl_Position = matrices.mvp * vec4(inVertexPosition, 1.0);

    outVertexColor     = inVertexColor;
    outVertexTexCoords = inVertexTexCoords;
    outVertexNormal    = inVertexNormal;
}

// ----------------------------------------------------------------------------
#glsl_fragment
// ----------------------------------------------------------------------------

layout(location = 0) in vec4 inVertexColor;
layout(location = 1) in vec2 inVertexTexCoords;
layout(location = 2) in vec3 inVertexNormal;

layout(location = 0) out vec4 outFragColor;

layout(binding = 1)
uniform sampler2D mipTextureSampler;

layout(binding = 2)
uniform sampler2DArray arrayTextureSampler;

void main()
{
    // Reuse the vertex normal to encode the texture layer index.
    // < 0, use the mipmap texture. >= 0 use the array texture.
    float layerIndex = inVertexNormal.x;
    if (layerIndex < 0.0)
    {
        int mipNum = gl_PrimitiveID / 2; // switch mips every 2 triangles
        outFragColor = textureLod(mipTextureSampler, inVertexTexCoords, mipNum) * inVertexColor;
    }
    else
    {
        outFragColor = texture(arrayTextureSampler, vec3(inVertexTexCoords, layerIndex)) * inVertexColor;
    }
}

// ----------------------------------------------------------------------------
