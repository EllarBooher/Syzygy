#pragma once

#include <glm/vec2.hpp>

struct UIRectangle
{
    glm::vec2 min{};
    glm::vec2 max{};

    glm::vec2 pos() const { return min; }
    glm::vec2 size() const { return max - min; }

    static UIRectangle fromPosSize(glm::vec2 const pos, glm::vec2 const size)
    {
        return UIRectangle{
            .min{pos},
            .max{pos + size},
        };
    }

    UIRectangle clampToMin() const
    {
        return UIRectangle{
            .min{min},
            .max{glm::max(min, max)},
        };
    }

    UIRectangle shrink(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{min + margins},
            .max{max - margins},
        };
    }
    UIRectangle shrinkMin(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{min + margins},
            .max{max},
        };
    }
    UIRectangle shrinkMax(glm::vec2 const margins) const
    {
        return UIRectangle{
            .min{min},
            .max{max - margins},
        };
    }
};