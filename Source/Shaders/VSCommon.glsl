//
// Common include for Vertex Shaders used by the demo applications.
//

#ifndef VS_COMMON_GLSL
#define VS_COMMON_GLSL

#ifndef NO_GL_PER_VERTEX_BLOCK
out gl_PerVertex
{
    vec4 gl_Position;
};
#endif // NO_GL_PER_VERTEX_BLOCK

// These must be in sync with the C++ declarations in Mesh.hpp!
#define VS_IN_LOCATION_POSITION   0
#define VS_IN_LOCATION_TEXCOORDS  1
#define VS_IN_LOCATION_COLOR      2
#define VS_IN_LOCATION_NORMAL     3
#define VS_IN_LOCATION_TANGENT    4
#define VS_IN_LOCATION_BITANGENT  5

#endif // VS_COMMON_GLSL
