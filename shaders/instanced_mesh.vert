#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shading_language_include : require

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

#include "types/vertex.glsl"

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer TransformBuffer{
	mat4 transforms[];
};

layout( push_constant ) uniform RenderConstant
{
	mat4 renderMatrix;
	VertexBuffer vertexBuffer;
	TransformBuffer transformBuffer;
} renderConstant;

void main()
{
	mat4 transform = renderConstant.transformBuffer.transforms[gl_InstanceIndex];

	Vertex vertex = renderConstant.vertexBuffer.vertices[gl_VertexIndex];
	
	gl_Position = renderConstant.renderMatrix * transform * vec4(vertex.position, 1.0f);

	outColor = vertex.color.rgb;
	outUV.x = vertex.uv_x;
	outUV.y = vertex.uv_y;
}