struct LightDirectional
{
    vec4 color;

    vec3 forward;
    float forwardW;

    mat4 projection;

    mat4 view;

    float strength;

    float angularRadius;
};

struct LightSpot
{
    vec4 color;

    vec4 forward;

    mat4 projection;

    mat4 view;

    vec4 position;

    float strength;
    float falloffFactor;
    float falloffDistance;
};