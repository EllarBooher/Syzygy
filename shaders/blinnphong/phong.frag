#version 460

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 normalInterpolated;

layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = vec4(normalInterpolated, 1.0);
}