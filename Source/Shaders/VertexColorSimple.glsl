//
// Vertex position and color only.
//

// ----------------------------------------------------------------------------
#glsl_vertex
// ----------------------------------------------------------------------------

#include <VSCommon.glsl>

layout(location = VS_IN_LOCATION_POSITION) in vec3 inVertexPosition;
layout(location = VS_IN_LOCATION_COLOR)    in vec4 inVertexColor;

layout(location = 0) out vec4 outVertexColor;

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
    outVertexColor = inVertexColor;
}

// ----------------------------------------------------------------------------
#glsl_fragment
// ----------------------------------------------------------------------------

layout(location = 0) in  vec4 inVertexColor;
layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = inVertexColor;
}

// ----------------------------------------------------------------------------
