/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Main manager / façade
 *
 * Design intent:
 *  - Reflect Crucible-era CONCORD behavior as closely as possible.
 *  - High-sec only (>= 0.5) for now, with response delays matching
 *    Crucible-style timings.
 *  - Low-sec and null will be handled separately (Faction Police, etc.).
 *****************************************************************************/

#include "eve-server.h"
#include "npc/concord/ConcordManager.h"

#include "npc/concord/ConcordLog.h"
#include "npc/concord/CrimeWatch.h"
#include "npc/concord/ConcordTimers.h"
#include "npc/concord/ConcordResponse.h"
#include "npc/concord/ConcordShips.h"

#include "system/SystemManager.h"

namespace ConcordV2
{

ConcordManager& ConcordManager::Instance()
{
    static ConcordManager s_instance;
    return s_instance;
}

void ConcordManager::Initialize()
{
    if (m_initialized)
        return;

    // Eventually we can load ship types, config, etc.
    ConcordShips::Instance().LoadFromConfig();

    CONCORD_LOG_INFO("ConcordManager initialized (V2 scaffolding only; no active spawns yet).");
    m_initialized = true;
}


bool ConcordManager::IsOffender(const SystemEntity* entity) const
{
    if (entity == nullptr)
        return false;

    return (m_handledOffenders.find(entity) != m_handledOffenders.end());
}


void ConcordManager::OnPossibleCrime(SystemManager& system, const CrimeEvent& event)
{
    if (!m_initialized)
        Initialize();

    // We take a copy so we can annotate the CrimeEvent with the classified type
    // before handing it off to the response planner.
    CrimeEvent crime = event;

    // Classify the crime based on Crucible-style rules (high-sec aggression, etc.).
    CrimeType type = CrimeWatch::Instance().ClassifyCrime(crime);
    crime.type = type;

    if (type == CrimeType::None)
    {
        CONCORD_LOG_DEBUG(
            "OnPossibleCrime: event in system %u classified as %s. Ignoring.",
            crime.solarSystemID, GetCrimeTypeName(type)
        );
        return;
    }

    // Make sure we actually know who the offender is.
    if (crime.offender == nullptr)
    {
        CONCORD_LOG_WARN(
            "OnPossibleCrime: crimeType=%s in system %u has null offender; cannot schedule Concord.",
            GetCrimeTypeName(type), crime.solarSystemID
        );
        return;
    }

    // Dedup: each offender only triggers a single Concord response per server run.
    if (m_handledOffenders.find(crime.offender) != m_handledOffenders.end())
    {
        CONCORD_LOG_INFO(
            "OnPossibleCrime: crimeType=%s in system %u from offender already handled; "
            "suppressing additional Concord response.",
            GetCrimeTypeName(type), crime.solarSystemID
        );
        return;
    }

    m_handledOffenders.insert(crime.offender);

    // Calculate timing using Crucible-style response bands.
    double delay = ConcordTimers::Instance().GetResponseDelay(crime);
    if (delay < 0.0)
    {
        CONCORD_LOG_DEBUG(
            "OnPossibleCrime: crimeType=%s in system %u -> delay=%.2f (no Concord response scheduled).",
            GetCrimeTypeName(type), crime.solarSystemID, delay
        );
        return;
    }

    // Log what the timing model decided.
    CONCORD_LOG_INFO(
        "OnPossibleCrime: crimeType=%s in system %u will be handled by ConcordV2 with delay=%.2f sec.",
        GetCrimeTypeName(type), crime.solarSystemID, delay
    );

    // Hand off to the response planner, which may spawn ships depending on
    // kEnableConcordSpawns in ConcordResponse.cpp.
    ConcordResponse::Instance().HandleCrime(system, crime, delay);
}


} // namespace ConcordV2

