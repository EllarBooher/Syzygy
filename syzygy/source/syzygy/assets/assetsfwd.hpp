#pragma once

#include <functional>
#include <memory>

namespace syzygy
{
template <typename T> struct Asset;
template <typename T> using AssetPtr = std::weak_ptr<Asset<T> const>;
template <typename T> using AssetShared = std::shared_ptr<Asset<T> const>;
template <typename T> using AssetRef = std::reference_wrapper<Asset<T> const>;
} // namespace syzygy