struct LightDirectional {
	vec4 color;

	vec4 forward;

	mat4 projection;

	mat4 view;

	float strength;
};

struct LightSpot {
	vec4 color;

	vec4 forward;

	mat4 projection;

	mat4 view;

	vec4 position;

	float strength;
	float falloffFactor;
	float falloffDistance;
};