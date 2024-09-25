#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shading_language_include : require

#include "../types/vertex.glsl"

layout(buffer_reference, std430) readonly buffer ProjViewBuffer
{
    mat4 matrices[];
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer ModelBuffer
{
    mat4 models[];
};

layout(push_constant) uniform PushConstant
{
    VertexBuffer vertexBuffer;
    ModelBuffer modelBuffer;
    ProjViewBuffer projViewBuffer;
    uint projViewIndex;
} pushConstant;

void main()
{
    mat4 model = pushConstant.modelBuffer.models[gl_InstanceIndex];

    Vertex vertex = pushConstant.vertexBuffer.vertices[gl_VertexIndex];
    mat4 projView = pushConstant.projViewBuffer.matrices[pushConstant.projViewIndex];

    gl_Position = projView * model * vec4(vertex.position, 1.0f);
}