#pragma once

#include "syzygy/core/integer.hpp"

namespace szg
{
struct UUID
{
public:
    static auto createNew() -> UUID;
    auto valid() const -> bool;

    operator uint64_t() const;

private:
    uint64_t m_uuid{0};
};
} // namespace szg

namespace std
{
template <typename T> struct hash;

template <> struct hash<szg::UUID>
{
    auto operator()(szg::UUID const& uuid) const -> std::size_t
    {
        return static_cast<uint64_t>(uuid);
    }
};

} // namespace std