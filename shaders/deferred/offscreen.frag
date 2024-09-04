#version 460

// ABeautifulGame Khronos example uses these conventions
// TODO: generalize for other asset types
#define WITH_NORMAL_MAP_UNSIGNED
#define WITH_NORMAL_MAP_GREEN_UP

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outDiffuseColor;
layout(location = 1) out vec4 outSpecularColor;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outWorldPosition;
layout(location = 4) out vec4 outOcclusionRoughnessMetallic;

layout(set = 3, binding = 0) uniform sampler2D color;
layout(set = 3, binding = 1) uniform sampler2D normal;
layout(set = 3, binding = 2) uniform sampler2D ORM;

// tangent space computation method taken with comments from 
// http://www.thetenthplanet.de/archives/1180

mat3 cotangentFrame( vec3 N, vec3 p, vec2 uv ) { 
	// get edge vectors of the pixel triangle 
	vec3 dp1 = dFdx( p ); 
	vec3 dp2 = dFdy( p ); 
	vec2 duv1 = dFdx( uv ); 
	vec2 duv2 = dFdy( uv );  
	
	// solve the linear system 
	vec3 dp2perp = cross( dp2, N ); 
	vec3 dp1perp = cross( N, dp1 ); 
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x; 
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;   

	// construct a scale-invariant frame 
	float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) ); 
	return mat3( T * invmax, B * invmax, N ); 
}

vec3 perturbNormal( vec3 N, vec3 V, vec2 texcoord ) { 
	// assume N, the interpolated vertex normal and 
	// V, the view vector (vertex to eye) 
	vec3 map = texture( normal, texcoord ).xyz; 

#ifdef WITH_NORMAL_MAP_UNSIGNED
	map = map * 255./127. - 128./127.;
#endif
#ifdef WITH_NORMAL_MAP_GREEN_UP
	map.y = -map.y;
#endif

	mat3 TBN = cotangentFrame( N, -V, texcoord ); 
	return normalize( TBN * map ); 
}

void main()
{
	outWorldPosition = vec4(inWorldPosition, 1.0);

	vec3 perturbedNormal = perturbNormal(inNormal, inWorldPosition, inTexCoord);

	outNormal = vec4(perturbedNormal, 0.0);

	vec4 sampledColor = texture(color, inTexCoord); 

	outDiffuseColor = vec4(sampledColor.rgb, 1.0);
	
	// TODO: decide specular color separately
	outSpecularColor = vec4(sampledColor.rgb, 1.0);

	vec4 sampledORM = texture(ORM, inTexCoord);

	outOcclusionRoughnessMetallic = vec4(sampledORM.rgb, 1.0);
}