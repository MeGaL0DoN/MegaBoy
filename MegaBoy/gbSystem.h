#pragma once

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