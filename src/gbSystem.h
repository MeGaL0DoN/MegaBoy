#pragma once
#include <cstdint>

enum GBSystemPreference
{
    PreferGBC,
    PreferDMG,
    ForceDMG,
};

enum class GBSystem : uint8_t
{
    DMG,
    GBC
};

namespace System
{
    GBSystem Current();
    void Set(GBSystem sys);
}