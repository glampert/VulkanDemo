//
// Test shader that draws a built-in triangle.
//
// The vertices are hardcoded in the VS so you
// don't need to set up a vertex buffer to draw
// the triangle, just issuing a vkCmdDraw is enough
// to get something on screen for quick testing.
//

// ----------------------------------------------------------------------------
#glsl_vertex
// ----------------------------------------------------------------------------

#include <VSCommon.glsl>

layout(location = 0) out vec3 outVertexColor;

const vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5,  0.5),
    vec2(-0.5, 0.5)
);

const vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outVertexColor = colors[gl_VertexIndex];
}

// ----------------------------------------------------------------------------
#glsl_fragment
// ----------------------------------------------------------------------------

layout(location = 0) in  vec3 inVertexColor;
layout(location = 0) out vec4 outFragColor;

// Test for array of uniform buffers, indexed by a uniform variable.
// This will allocate 5 descriptors, 4 for ubo[] and 1 for ubi[].
layout(binding = 0)
uniform UniformBufferObject
{
    vec4 tintColor;
} ubo[4];

layout(binding = 1)
uniform UniformBufferIndex
{
    int index;
} ubi;

void main()
{
    outFragColor = vec4(inVertexColor, 1.0) * ubo[ubi.index].tintColor;
}

// ----------------------------------------------------------------------------
