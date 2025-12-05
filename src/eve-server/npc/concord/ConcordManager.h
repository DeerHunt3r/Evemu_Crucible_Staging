#ifndef EVE_NPC_CONCORD_MANAGER_H
#define EVE_NPC_CONCORD_MANAGER_H

#include "eve-server.h"
#include "npc/concord/CrimeWatch.h"
#include <set>

class SystemManager;
class SystemEntity;

namespace ConcordV2
{

class ConcordManager
{
public:
    static ConcordManager& Instance();

    // Called during server startup to initialize anything we need.
    void Initialize();

    // System reports a possible crime.
    void OnPossibleCrime(SystemManager& system, const CrimeEvent& event);

    // Query: has this entity already been recorded as an offender that
    // triggered a CONCORD response in this server run?
    bool IsOffender(const SystemEntity* entity) const;

private:
    ConcordManager() = default;

private:
    bool m_initialized = false;

    // Once we have responded to a given offender, we do not spawn additional
    // CONCORD waves for that offender again during this server run.
    std::set<const SystemEntity*> m_handledOffenders;
};

} // namespace ConcordV2

#endif // EVE_NPC_CONCORD_MANAGER_H

