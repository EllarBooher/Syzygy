struct Camera
{
    mat4 projection;

    mat4 inverseProjection;

    mat4 view;

    mat4 viewInverseTranspose;

    mat4 rotation;

    mat4 projViewInverse;

    vec4 forwardWorld;

    vec4 position;
};