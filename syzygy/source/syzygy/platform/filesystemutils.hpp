#pragma once

#include <filesystem>

namespace syzygy
{
auto ensureAbsolutePath(
    std::filesystem::path const& path,
    std::filesystem::path const& root = std::filesystem::current_path()
) -> std::filesystem::path;
} // namespace syzygy
