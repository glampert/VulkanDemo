//
// Vertex position, color and texture. This shader is used by the textured cubes sample.
// Assumes two instances of the geometry, indexing with the VK extension 'gl_InstanceIndex'.
//

// ----------------------------------------------------------------------------
#glsl_vertex
// ----------------------------------------------------------------------------

#include <VSCommon.glsl>

layout(location = VS_IN_LOCATION_POSITION)  in vec3 inVertexPosition;
layout(location = VS_IN_LOCATION_TEXCOORDS) in vec2 inVertexTexCoords;
layout(location = VS_IN_LOCATION_COLOR)     in vec4 inVertexColor;

layout(location = 0) out vec4 outVertexColor;
layout(location = 1) out vec2 outVertexTexCoords;
layout(location = 2) out flat int outTextureIndex;

layout(binding = 0)
uniform Matrices
{
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 mvp;
} matrices[2];

void main()
{
    gl_Position        = matrices[gl_InstanceIndex].mvp * vec4(inVertexPosition, 1.0);
    outVertexColor     = inVertexColor;
    outVertexTexCoords = inVertexTexCoords;
    outTextureIndex    = gl_InstanceIndex;
}

// ----------------------------------------------------------------------------
#glsl_fragment
// ----------------------------------------------------------------------------

layout(location = 0) in vec4 inVertexColor;
layout(location = 1) in vec2 inVertexTexCoords;
layout(location = 2) in flat int inTextureIndex;

layout(binding = 1)
uniform sampler2D textureSampler[2];

layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(textureSampler[inTextureIndex], inVertexTexCoords) * inVertexColor;
}

// ----------------------------------------------------------------------------
