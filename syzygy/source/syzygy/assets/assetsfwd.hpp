#pragma once

#include <functional>

namespace syzygy
{
template <typename T> struct Asset;
template <typename T> using AssetRef = std::reference_wrapper<Asset<T> const>;
} // namespace syzygy