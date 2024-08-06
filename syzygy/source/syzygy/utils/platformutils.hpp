#pragma once

#include "syzygy/editor/window.hpp"
#include <filesystem>
#include <string>

namespace szg_utils
{
// Format the filter like ".TXT;.DOC;.BAK" to filter out files that can be
// selected
auto openFile(PlatformWindow const& parent)
    -> std::optional<std::filesystem::path>;
auto openFiles(PlatformWindow const& parent)
    -> std::vector<std::filesystem::path>;
auto openDirectory(PlatformWindow const& parent)
    -> std::optional<std::filesystem::path>;
auto openDirectories(PlatformWindow const& parent)
    -> std::vector<std::filesystem::path>;
} // namespace szg_utils