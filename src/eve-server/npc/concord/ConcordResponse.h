/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Response fleet spawning
 *****************************************************************************/

#ifndef EVE_NPC_CONCORD_RESPONSE_H
#define EVE_NPC_CONCORD_RESPONSE_H

#include "eve-server.h"
#include "npc/concord/CrimeWatch.h"
#include "npc/concord/ConcordLog.h"

class SystemManager;

namespace ConcordV2
{

// High-level description of what Concord would spawn in response to a crime.
struct ResponsePlan
{
    uint32 solarSystemID      = 0;
    double systemSecurity     = 1.0;
    double responseDelaySec   = -1.0;

    // Ship typeIDs (filled from ConcordShips) and counts.
    uint32 commanderTypeID    = 0;
    uint32 captainTypeID      = 0;
    uint32 droneTypeID        = 0;

    uint8  commanderCount     = 0;
    uint8  captainCount       = 0;
    uint8  droneCount         = 0;
};

// ConcordResponse is responsible for building a response plan and, later,
// actually spawning the fleet when we are ready to enable it.
class ConcordResponse
{
public:
    static ConcordResponse& Instance();

    // Build a response plan for a given crime. Returns false if no response
    // should occur (e.g. unsupported crime type, system security, etc.).
    bool BuildResponsePlan(const CrimeEvent& event, double delaySec, ResponsePlan& outPlan) const;

    // Main entry: respond to a classified crime in a given system.
    // NOTE: This will remain a stub until we are ready to actually spawn ships.
    void HandleCrime(SystemManager& system, const CrimeEvent& event, double delaySec);

private:
    ConcordResponse() = default;
};

} // namespace ConcordV2

#endif // EVE_NPC_CONCORD_RESPONSE_H

