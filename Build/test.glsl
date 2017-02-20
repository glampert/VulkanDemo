
/*
 * test shader
 */

// ----------------------------------------------------------------------------
#glsl_vertex
// ----------------------------------------------------------------------------

//#version 450
//#define POS_LOC 0
//#define COLOR_IN_LOC 1
//#define COLOR_OUT_LOC 0
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_420pack : enable

layout(std140, binding = 0) uniform bufferVals
{
    mat4 mvp;
}
myBufferVals;
layout(location = POS_LOC) in vec4 pos;
layout(location = COLOR_IN_LOC) in vec4 inColor;
layout(location = COLOR_OUT_LOC) out vec4 outColor;
out gl_PerVertex
{
    vec4 gl_Position;
};
void main()
{
    outColor = inColor;
    gl_Position = myBufferVals.mvp * pos;
}

// ----------------------------------------------------------------------------
#glsl_fragment
// ----------------------------------------------------------------------------

//#version 450
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec4 color;
layout(location = 0) out vec4 outColor;
void main()
{
    outColor = color;
}

// ----------------------------------------------------------------------------
