#include "geometrytests.hpp"

#include "syzygy/core/log.hpp"
#include <glm/gtx/string_cast.hpp>

namespace
{
float constexpr TEST_EPSILON{3.0F * glm::epsilon<float>()};

auto eulerAnglesTestInverse(
    glm::vec3 const unnormalizedForward, bool const quiet = false
) -> bool
{
    glm::vec3 const forward{glm::normalize(unnormalizedForward)};
    glm::vec3 const eulers{syzygy::eulersFromForward(forward)};
    glm::vec3 const reconstructedForward{syzygy::forwardFromEulers(eulers)};

    if (glm::epsilonEqual(forward, reconstructedForward, TEST_EPSILON)
        != glm::bvec3(true))
    {
        if (!quiet)
        {
            SZG_ERROR(
                "Failed geometry test - eulerAnglesTestInverse \n"
                " - start {} \n"
                " - middle {} \n"
                " - end {}",
                glm::to_string(forward),
                glm::to_string(eulers),
                glm::to_string(reconstructedForward)
            );
        }

        return false;
    }

    return true;
}

auto eulerAnglesTestInverseCombinations(
    glm::vec3 const a,
    glm::vec3 const b,
    glm::vec3 const c,
    bool const quiet = false
) -> bool
{
    bool success{true};

    success &= eulerAnglesTestInverse(a, quiet);
    success &= eulerAnglesTestInverse(b, quiet);
    success &= eulerAnglesTestInverse(c, quiet);

    success &= eulerAnglesTestInverse(a + b, quiet);
    success &= eulerAnglesTestInverse(b + c, quiet);
    success &= eulerAnglesTestInverse(c + a, quiet);

    success &= eulerAnglesTestInverse(a + b + c, quiet);

    return success;
}

auto eulerAnglesTestEquality(
    glm::vec3 const unnormalizedForward,
    glm::vec3 const expectedAngles,
    bool const quiet = false
) -> bool
{
    glm::vec3 const forward{glm::normalize(unnormalizedForward)};
    glm::vec3 const eulers{syzygy::eulersFromForward(forward)};

    if (glm::epsilonEqual(expectedAngles, eulers, TEST_EPSILON)
        != glm::bvec3(true))
    {
        if (!quiet)
        {
            SZG_ERROR(
                "Failed geometry test - eulerAnglesTestEquality \n"
                " - forward {} \n"
                " - eulers {} \n"
                " - expected {}",
                glm::to_string(forward),
                glm::to_string(eulers),
                glm::to_string(expectedAngles)
            );
        }

        return false;
    }

    return true;
}

auto eulerAnglesTests() -> bool
{
    bool success{true};

    success &= eulerAnglesTestInverse(glm::vec3{1.0F, 0.0F, 0.0F});
    success &= eulerAnglesTestInverse(glm::vec3{0.0F, 1.0F, 0.0F});
    success &= eulerAnglesTestInverse(glm::vec3{0.0F, 0.0F, 1.0F});

    success &= eulerAnglesTestInverseCombinations(
        syzygy::WORLD_FORWARD, syzygy::WORLD_RIGHT, syzygy::WORLD_UP
    );

    success &= eulerAnglesTestInverseCombinations(
        -1.0F * syzygy::WORLD_FORWARD,
        -1.0F * syzygy::WORLD_RIGHT,
        -1.0F * syzygy::WORLD_UP
    );

    success &= eulerAnglesTestEquality(syzygy::WORLD_FORWARD, glm::vec3{0.0F});
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_FORWARD, glm::vec3{0.0F, 0.0F, -glm::pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        syzygy::WORLD_RIGHT, glm::vec3{0.0F, 0.0F, glm::half_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_RIGHT, glm::vec3{0.0F, 0.0F, -glm::half_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        syzygy::WORLD_UP, glm::vec3{glm::half_pi<float>(), 0.0F, 0.0F}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_UP, glm::vec3{-glm::half_pi<float>(), 0.0F, 0.0F}
    );

    success &= eulerAnglesTestEquality(
        syzygy::WORLD_FORWARD + syzygy::WORLD_UP,
        glm::vec3{glm::quarter_pi<float>(), 0.0F, 0.0F}
    );
    success &= eulerAnglesTestEquality(
        syzygy::WORLD_FORWARD - syzygy::WORLD_UP,
        glm::vec3{-glm::quarter_pi<float>(), 0.0F, 0.0F}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_FORWARD - syzygy::WORLD_UP,
        glm::vec3{-glm::quarter_pi<float>(), 0.0F, glm::pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_FORWARD + syzygy::WORLD_UP,
        glm::vec3{glm::quarter_pi<float>(), 0.0F, glm::pi<float>()}
    );

    success &= eulerAnglesTestEquality(
        syzygy::WORLD_UP + syzygy::WORLD_RIGHT,
        glm::vec3{glm::quarter_pi<float>(), 0.0F, glm::half_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        syzygy::WORLD_UP - syzygy::WORLD_RIGHT,
        glm::vec3{glm::quarter_pi<float>(), 0.0F, -glm::half_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_UP - syzygy::WORLD_RIGHT,
        glm::vec3{-glm::quarter_pi<float>(), 0.0F, -glm::half_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_UP + syzygy::WORLD_RIGHT,
        glm::vec3{-glm::quarter_pi<float>(), 0.0F, glm::half_pi<float>()}
    );

    success &= eulerAnglesTestEquality(
        syzygy::WORLD_RIGHT + syzygy::WORLD_FORWARD,
        glm::vec3{0.0F, 0.0F, glm::quarter_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        syzygy::WORLD_RIGHT - syzygy::WORLD_FORWARD,
        glm::vec3{0.0F, 0.0F, 3 * glm::quarter_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_RIGHT - syzygy::WORLD_FORWARD,
        glm::vec3{0.0F, 0.0F, -3 * glm::quarter_pi<float>()}
    );
    success &= eulerAnglesTestEquality(
        -syzygy::WORLD_RIGHT + syzygy::WORLD_FORWARD,
        glm::vec3{0.0F, 0.0F, -glm::quarter_pi<float>()}
    );

    // We expect precision errors with larger vectors to cause issues when
    // converting back and forth, so we test that to see how bad it is
    float precisionFactor{1.0F};

    bool precisionSuccess{true};
    while (precisionSuccess)
    {
        precisionSuccess &= eulerAnglesTestInverseCombinations(
            precisionFactor * syzygy::WORLD_FORWARD,
            precisionFactor * syzygy::WORLD_RIGHT,
            precisionFactor * syzygy::WORLD_UP,
            true
        );

        precisionSuccess &= eulerAnglesTestInverseCombinations(
            -precisionFactor * syzygy::WORLD_FORWARD,
            -precisionFactor * syzygy::WORLD_RIGHT,
            -precisionFactor * syzygy::WORLD_UP,
            true
        );

        precisionFactor *= 2;
    }

    SZG_INFO(
        "Euler Angles precision test - magnitudes up to {} still pass.",
        precisionFactor
    );

    return success;
}
} // namespace

auto syzygy_tests::runTests() -> bool
{
    SZG_INFO("Running geometry tests.");

    bool success{true};

    success &= eulerAnglesTests();

    return success;
}
