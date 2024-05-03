#version 460

layout(location = 0) in vec3 inDiffuseColor;
layout(location = 1) in vec3 inSpecularColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inWorldPosition;

layout(location = 0) out vec4 outDiffuseColor;
layout(location = 1) out vec4 outSpecularColor;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outWorldPosition;

void main()
{
	outWorldPosition = vec4(inWorldPosition, 1.0);
	outNormal = vec4(inNormal, 1.0);
	outDiffuseColor = vec4(inDiffuseColor, 1.0);
	outSpecularColor = vec4(inSpecularColor, 1.0);
}