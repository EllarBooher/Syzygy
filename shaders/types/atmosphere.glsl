// See gputypes.hpp for documentation

struct Atmosphere
{
    vec3 directionToSun;
    float earthRadiusMeters;

    vec3 scatteringCoefficientRayleigh;
    float altitudeDecayRayleigh;

    vec3 scatteringCoefficientMie;
    float altitudeDecayMie;

    vec3 ambientColor;
    float atmosphereRadiusMeters;

    vec3 sunlightColor;
    float padding0;

    // Not used anywhere.
    vec3 groundColor;
    float padding1;
};