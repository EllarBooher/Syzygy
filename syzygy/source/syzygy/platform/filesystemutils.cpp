#include "filesystemutils.hpp"

auto syzygy::ensureAbsolutePath(
    std::filesystem::path const& path, std::filesystem::path const& root
) -> std::filesystem::path
{
    if (path.is_absolute())
    {
        return path;
    }

    return root / path;
}
