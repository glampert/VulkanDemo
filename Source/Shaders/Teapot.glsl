//
// Shader used to render the teapot in VkAppTeapotModel.
//

// ----------------------------------------------------------------------------
#glsl_vertex
// ----------------------------------------------------------------------------

#include <VSCommon.glsl>

layout(location = VS_IN_LOCATION_POSITION)  in vec3 inVertexPosition;
layout(location = VS_IN_LOCATION_TEXCOORDS) in vec2 inVertexTexCoords;
layout(location = VS_IN_LOCATION_COLOR)     in vec4 inVertexColor;
layout(location = VS_IN_LOCATION_NORMAL)    in vec3 inVertexNormal;
layout(location = VS_IN_LOCATION_TANGENT)   in vec3 inVertexTangent;
layout(location = VS_IN_LOCATION_BITANGENT) in vec3 inVertexBitangent;

layout(location = 0) out vec4 outVertexColor;
layout(location = 1) out vec2 outVertexTexCoords;
layout(location = 2) out vec3 outVertexNormal;
layout(location = 3) out vec3 outVertexTangent;
layout(location = 4) out vec3 outVertexBitangent;

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
    outVertexTangent   = inVertexTangent;
    outVertexBitangent = inVertexBitangent;
}

// ----------------------------------------------------------------------------
#glsl_fragment
// ----------------------------------------------------------------------------

layout(location = 0) in vec4 inVertexColor;
layout(location = 1) in vec2 inVertexTexCoords;
layout(location = 2) in vec3 inVertexNormal;
layout(location = 3) in vec3 inVertexTangent;
layout(location = 4) in vec3 inVertexBitangent;

layout(binding = 1)
uniform sampler2D textureSampler;

layout(location = 0) out vec4 outFragColor;

void main()
{
    //outFragColor = inVertexColor;
    //outFragColor = vec4(inVertexTexCoords, 0.0, 1.0);
    //outFragColor = vec4(inVertexNormal, 1.0);
    //outFragColor = vec4(inVertexBitangent, 1.0);
}

// ----------------------------------------------------------------------------
