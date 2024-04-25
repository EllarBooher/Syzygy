#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shading_language_include : require

#include "../types/vertex.glsl"
#include "../types/camera.glsl"

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;


layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer CameraBuffer{
	Camera cameras[];
};

layout(push_constant) uniform PushConstant
{
	VertexBuffer vertexBuffer;
	CameraBuffer cameraBuffer;
	uint cameraIndex;
} pushConstant;

void main()
{
	Vertex vertex = pushConstant.vertexBuffer.vertices[gl_VertexIndex];
	Camera camera = pushConstant.cameraBuffer.cameras[pushConstant.cameraIndex];

	gl_Position = camera.projection * camera.view * vec4(vertex.position, 1.0f);

	outColor = vec3(0.0,1.0,0.0);
	outUV.x = vertex.uv_x;
	outUV.y = vertex.uv_y;
}