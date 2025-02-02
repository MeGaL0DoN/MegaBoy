#pragma once
#include <cstdint>

enum GBSystemPreference
{
    PreferCGB,
    ForceCGB,
    PreferDMG,
    ForceDMG,
};

enum class GBSystem : uint8_t
{
    DMG,
    CGB,
    DMGCompatMode
};

namespace System
{
    GBSystem Current();
    void Set(GBSystem sys);

    constexpr bool IsCGBDevice(GBSystem sys) { return sys == GBSystem::CGB || sys == GBSystem::DMGCompatMode; }
}