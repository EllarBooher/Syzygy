#pragma once

#include "syzygy/core/uuid.hpp"
#include <functional>
#include <memory>
#include <string>

namespace syzygy
{
struct AssetMetadata
{
    std::string displayName{};
    std::string fileLocalPath{};
    syzygy::UUID id{};
};

template <typename T> struct Asset
{
    AssetMetadata metadata{};
    std::shared_ptr<T> data{};
};

template <typename T> using AssetPtr = std::weak_ptr<Asset<T> const>;
template <typename T> using AssetShared = std::shared_ptr<Asset<T> const>;

template <typename T> using AssetRef = std::reference_wrapper<Asset<T> const>;
} // namespace syzygy