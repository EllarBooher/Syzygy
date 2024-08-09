#include "helpers.hpp"

auto szg_utils::ensureAbsolutePath(
    std::filesystem::path const& path, std::filesystem::path const& root
) -> std::filesystem::path
{
    if (path.is_absolute())
    {
        return path;
    }

    return root / path;
}
