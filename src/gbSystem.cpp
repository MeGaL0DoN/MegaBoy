#include "gbSystem.h"

namespace System
{
    static GBSystem system{ GBSystem::DMG }; 

    GBSystem Current()
    {
        return system;
    }

    void Set(GBSystem sys)
    {
        system = sys;
    }
}