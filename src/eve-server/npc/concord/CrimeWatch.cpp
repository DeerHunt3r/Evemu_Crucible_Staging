/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Crime detection
 *****************************************************************************/

#include "eve-server.h"
#include "npc/concord/CrimeWatch.h"

#include "system/SystemEntity.h"
#include "Client.h"

namespace ConcordV2
{

const char* GetCrimeTypeName(CrimeType type)
{
    switch (type)
    {
    case CrimeType::None:              return "None";
    case CrimeType::IllegalAggression: return "IllegalAggression";
    case CrimeType::IllegalAssistance: return "IllegalAssistance";
    case CrimeType::Suspect:           return "Suspect";
    case CrimeType::Criminal:          return "Criminal";
    default:                           return "Unknown";
    }
}

CrimeWatch& CrimeWatch::Instance()
{
    static CrimeWatch s_instance;
    return s_instance;
}

CrimeType CrimeWatch::ClassifyCrime(const CrimeEvent& event)
{
    // Basic safety checks.
    if (event.offender == nullptr || event.victim == nullptr)
        return CrimeType::None;

    // For now we assume the caller (Damage.cpp) only notifies us when:
    //  - system sec >= 0.5
    //  - both entities have pilots
    //  - attacker != victim
    //
    // That means we can treat every such case as a generic illegal aggression
    // in high-sec. We'll refine this later with module checks, flags, etc.
    if (event.systemSecurity >= 0.5)
    {
        // Future: differentiate suspect vs criminal, remote reps, loot theft, etc.
        CONCORD_LOG_DEBUG(
            "CrimeWatch::ClassifyCrime: high-sec aggression in system %u (sec=%.2f). Returning %s.",
            event.solarSystemID, event.systemSecurity, GetCrimeTypeName(CrimeType::IllegalAggression)
        );
        return CrimeType::IllegalAggression;
    }

    // Anything else is currently ignored.
    CONCORD_LOG_DEBUG(
        "CrimeWatch::ClassifyCrime: event in system %u (sec=%.2f) not considered Concord-worthy. Returning %s.",
        event.solarSystemID, event.systemSecurity, GetCrimeTypeName(CrimeType::None)
    );
    return CrimeType::None;
}

} // namespace ConcordV2

