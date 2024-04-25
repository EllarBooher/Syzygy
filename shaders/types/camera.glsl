struct Camera {
	mat4 projection;

	mat4 inverseProjection;

	mat4 view;

	mat4 viewInverseTranspose;

	mat4 rotation;

	vec3 position;
};