/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Crime detection
 *****************************************************************************/

#ifndef EVE_NPC_CONCORD_CRIMEWATCH_H
#define EVE_NPC_CONCORD_CRIMEWATCH_H

#include "eve-server.h"
#include "npc/concord/ConcordLog.h"

class SystemEntity;
class Client;

namespace ConcordV2
{

// Very simple first-pass "what kind of crime is this?" model.
// We’ll expand this later as we wire to actual aggression / flags.
enum class CrimeType
{
    None = 0,
    IllegalAggression,
    IllegalAssistance,
    Suspect,
    Criminal
};

struct CrimeEvent
{
    CrimeType       type = CrimeType::None;
    SystemEntity*   offender = nullptr;    // ship / entity committing the crime
    SystemEntity*   victim = nullptr;      // ship / entity being attacked
    double          systemSecurity = 1.0;  // sec status of the solar system (0.0 - 1.0)
    uint32          solarSystemID = 0;
    double          timestamp = 0.0;       // game time or GetFileTimeNow() etc.
};

// CrimeWatch is responsible for deciding if something is Concord-worthy.
class CrimeWatch
{
public:
    static CrimeWatch& Instance();

    // Core entrypoint: given a potential event, classify it.
    CrimeType ClassifyCrime(const CrimeEvent& event);

private:
    CrimeWatch() = default;
};

// Helper to convert a CrimeType value to a human-readable name for logging.
const char* GetCrimeTypeName(CrimeType type);

} // namespace ConcordV2

#endif // EVE_NPC_CONCORD_CRIMEWATCH_H

