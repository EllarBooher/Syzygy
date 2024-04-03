#version 450

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

layout( push_constant, std430 ) uniform ColorConstant
{
	layout(offset = 96) vec4 tint;
} colorConstant;

void main()
{
	outFragColor = vec4(inColor, 1.0f) * colorConstant.tint;
}