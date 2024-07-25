#pragma once

enum GBSystemPreference
{
    PreferGBC,
    PreferDMG,
    ForceDMG,
};

enum class GBSystem
{
    DMG,
    GBC
};

namespace System
{
    GBSystem Current();
    void Set(GBSystem sys);
}