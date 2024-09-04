#version 450
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shading_language_include : require

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outNormal;

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

layout(buffer_reference, std430) readonly buffer ModelInverseTransposeBuffer{
	mat4 modelInverseTransposes[];
};

layout( push_constant ) uniform PushConstant
{
	VertexBuffer vertexBuffer;
	ModelBuffer modelBuffer;
	ModelInverseTransposeBuffer modelInverseTransposeBuffer;
	CameraBuffer cameraBuffer;
	uint cameraIndex;
} pushConstant;

void main()
{
	mat4 model = pushConstant.modelBuffer.models[gl_InstanceIndex];
	mat4 modelInverseTranspose = pushConstant.modelInverseTransposeBuffer.modelInverseTransposes[gl_InstanceIndex];
	Vertex vertex = pushConstant.vertexBuffer.vertices[gl_VertexIndex];
	Camera camera = pushConstant.cameraBuffer.cameras[pushConstant.cameraIndex];

	vec4 position = model * vec4(vertex.position, 1.0);
	outWorldPosition = position.xyz;

	gl_Position = camera.projection * camera.view * position;

	outNormal = normalize((modelInverseTranspose * vec4(vertex.normal, 0.0)).xyz);

	outTexCoord = vec2(vertex.uv_x, vertex.uv_y);
}