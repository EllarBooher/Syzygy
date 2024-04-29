#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_ARB_shading_language_include : require

#include "../types/atmosphere.glsl"

layout(set = 0, binding = 1) uniform sampler2D shadowMap;

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 normalInterpolated;
layout (location = 3) in vec3 positionInterpolated;
layout (location = 4) in vec4 shadowCoord;

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

float sampleShadowMap(vec4 shadowCoord)
{
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st ).r;
		if ( shadowCoord.w > 0.0 && dist > (shadowCoord.z + 0.005) ) 
		{
			return 0.0;
		}
	}
	return 1.0;
}

float filterShadowmap(vec4 shadowCoord)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += sampleShadowMap(shadowCoord + vec4(dx*x, dy*y, 0.0, 0.0));
			count++;
		}
	
	}
	return shadowFactor / count;
}

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

	const vec4 shadowCoord = vec4(shadowCoord.xyz / shadowCoord.w, shadowCoord.w);
	const float attenuationShadow = filterShadowmap(shadowCoord / shadowCoord.w);

	outFragColor = vec4(
		ambientContribution 
		+ attenuationShadow * (diffuseContribution + specularContribution)
		, 1.0
	);
}