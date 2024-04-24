#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shading_language_include : require

#include "../types/atmosphere.glsl"

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 normalInterpolated;
layout (location = 3) in vec3 positionInterpolated;

layout (location = 0) out vec4 outFragColor;

layout(buffer_reference, std430) readonly buffer AtmosphereBuffer{
	Atmosphere atmospheres[];
};

layout (push_constant) uniform PushConstant
{
	layout(offset = 64) vec4 lightDirectionViewSpace;

	vec4 diffuseColor;

	vec4 specularColor;

	AtmosphereBuffer atmosphereBuffer;
	uint atmosphereIndex;
	float shininess;
} pushConstant;

void main()
{
	//See James F. Blinn. 1977. "Models of light reflection for computer synthesized pictures."
	
	// TODO: do light calculations in world space
	const vec3 lightDirection = pushConstant.lightDirectionViewSpace.xyz;
	
	const vec3 normal = normalize(normalInterpolated);
	const vec3 viewDirection = normalize(vec3(0.0) - positionInterpolated);

	const float lambertian = max(dot(lightDirection, normal), 0.0);

	// Blinn noted that reflected light is less likely to be intercepted by randomly distributed surface facets
	// when the angle of reflection is as small as possible.
	const vec3 halfwayDirection = normalize(lightDirection + viewDirection);
	// specularCosine is a measure of the probability that these surface imperfection facets are oriented
	// with the normal, which would maximize the light reflected.
	const float specularCosine = clamp(dot(halfwayDirection, normal), 0.0,1.0);

	// Higher shininess means speculars peak faster and are smaller
	const float shininess = pushConstant.shininess;

	// This arbitrary fade factor smoothes out cases where the view vector is near 90 degrees from the normal.
	// This avoids a specular highlight snapping in, which seems to be an issue with parallel rays.
	// TODO: investigate how to better handle directional lighting with this shader
	const float fade = smoothstep(0.0,0.01,lambertian);

	// Phong facet distribution function. 
	const float specular = fade * pow(specularCosine, shininess);

	const Atmosphere atmosphere = pushConstant.atmosphereBuffer.atmospheres[pushConstant.atmosphereIndex];

	const vec3 ambientContribution = inColor * atmosphere.ambientColor;
	const vec3 diffuseContribution = lambertian * pushConstant.diffuseColor.rgb * inColor * atmosphere.sunlightColor;
	const vec3 specularContribution = specular * pushConstant.specularColor.rgb * atmosphere.sunlightColor;

	outFragColor = vec4(ambientContribution + diffuseContribution + specularContribution, 1.0);
}