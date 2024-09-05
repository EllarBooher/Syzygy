#include "geometrytypes.hpp"

namespace syzygy
{
auto Ray::create(glm::vec3 from, glm::vec3 to) -> Ray
{
    return {.position = from, .direction = to - from};
}
} // namespace syzygy