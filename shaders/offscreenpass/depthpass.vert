#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shading_language_include : require

#include "../types/camera.glsl"
#include "../types/vertex.glsl"

layout(buffer_reference, std430) readonly buffer CameraBuffer{
	Camera cameras[];
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer ModelBuffer{
	mat4 models[];
};

layout( push_constant ) uniform PushConstant
{
	VertexBuffer vertexBuffer;
	ModelBuffer modelBuffer;
	CameraBuffer cameraBuffer;
	uint cameraIndex;
} pushConstant;

void main()
{
	mat4 model = pushConstant.modelBuffer.models[gl_InstanceIndex];

	Vertex vertex = pushConstant.vertexBuffer.vertices[gl_VertexIndex];
	Camera camera = pushConstant.cameraBuffer.cameras[pushConstant.cameraIndex];

	gl_Position = camera.projection * camera.view * model * vec4(vertex.position, 1.0f);
}