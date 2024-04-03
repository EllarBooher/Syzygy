#version 450

layout(location = 0) out vec3 outColor;

layout( push_constant, std430 ) uniform VertexConstant
{
	mat3x4 verts;
	mat3x4 colors;
} vertexConstant;

void main()
{
	// breaks with more than 1 triangle / 3 vertices
	gl_Position = vertexConstant.verts[gl_VertexIndex];
	outColor = vertexConstant.colors[gl_VertexIndex].rgb;
}