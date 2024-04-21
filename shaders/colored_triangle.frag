#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

void main()
{
	outFragColor = vec4(inColor,1.0);
}