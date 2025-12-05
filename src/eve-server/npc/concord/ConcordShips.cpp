/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Ship definitions
 *****************************************************************************/

#include "eve-server.h"
#include "npc/concord/ConcordShips.h"
#include "npc/concord/ConcordLog.h"

namespace ConcordV2
{

ConcordShips& ConcordShips::Instance()
{
    static ConcordShips s_instance;
    return s_instance;
}

ConcordShips::ConcordShips()
{
    // Crucible-era CONCORD ship typeIDs.
    // These should match the classic high-sec response NPCs:
    //
    //  11125  CONCORD Police Commander
    //   3885  CONCORD Police Captain
    //  16104  CONCORD Surveillance Drone
    //
    // If you ever change these, also update this comment so future-you knows
    // what they were intended to be.
    m_types.policeCommanderTypeID = 11125;  // CONCORD Police Commander
    m_types.policeCaptainTypeID   = 3885;   // CONCORD Police Captain
    m_types.policeDroneTypeID     = 16104;  // CONCORD Surveillance Drone

    CONCORD_LOG_DEBUG("ConcordShips initialized with Crucible CONCORD ship typeIDs.");
}

void ConcordShips::LoadFromConfig()
{
    CONCORD_LOG_DEBUG("ConcordShips::LoadFromConfig called.");

    bool missing = (m_types.policeCommanderTypeID == 0) ||
                   (m_types.policeCaptainTypeID   == 0) ||
                   (m_types.policeDroneTypeID     == 0);

    if (missing)
    {
        CONCORD_LOG_WARN(
            "ConcordShips: one or more Concord ship typeIDs are not configured "
            "(commander=%u, captain=%u, drone=%u). Using placeholder typeIDs=0. "
            "This should be updated to real Crucible ship types.",
            m_types.policeCommanderTypeID,
            m_types.policeCaptainTypeID,
            m_types.policeDroneTypeID
        );
    }
    else
    {
        CONCORD_LOG_INFO(
            "ConcordShips: loaded Concord ship types (commander=%u, captain=%u, drone=%u).",
            m_types.policeCommanderTypeID,
            m_types.policeCaptainTypeID,
            m_types.policeDroneTypeID
        );
    }
}

} // namespace ConcordV2

