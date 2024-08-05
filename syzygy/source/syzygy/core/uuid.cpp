#include "uuid.hpp"

#include <random>

auto szg::UUID::createNew() -> UUID
{
    static std::random_device randomDevice;
    static std::mt19937_64 rngEngine{randomDevice()};

    // Reserve 0 as the default/null value that shouldn't ever refer to an
    // entity.
    uint64_t constexpr UUID_MINIMUM{1};

    static std::uniform_int_distribution<uint64_t> distribution{UUID_MINIMUM};

    UUID uuid{};
    uuid.m_uuid = distribution(rngEngine);

    distribution.min();

    return uuid;
}

auto szg::UUID::valid() const -> bool { return m_uuid == 0; }

szg::UUID::operator uint64_t() const { return m_uuid; }