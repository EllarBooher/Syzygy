#pragma once

#include "syzygy/platform/integer.hpp"
#include <type_traits>

namespace syzygy
{
struct UUID
{
public:
    static auto createNew() -> UUID;
    [[nodiscard]] auto valid() const -> bool;

    operator uint64_t() const;

private:
    uint64_t m_uuid{0};
};
} // namespace syzygy

namespace std
{
template <> struct hash<syzygy::UUID>
{
    auto operator()(syzygy::UUID const& uuid) const -> ::size_t
    {
        return static_cast<::uint64_t>(uuid);
    }
};

} // namespace std