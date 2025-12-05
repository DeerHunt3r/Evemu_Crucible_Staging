/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Ship definitions (header)
 *****************************************************************************/

#ifndef EVE_NPC_CONCORD_SHIPS_H
#define EVE_NPC_CONCORD_SHIPS_H

#include "eve-server.h"

namespace ConcordV2
{

// Simple container for the key CONCORD ship typeIDs used by Concord V2.
struct ConcordShipTypes
{
    // Crucible-era high-sec response ships:
    //
    //  11125  CONCORD Police Commander
    //   3885  CONCORD Police Captain
    //  16104  CONCORD Surveillance Drone
    //
    // These IDs are set in ConcordShips::ConcordShips() in the .cpp file.
    uint32 policeCommanderTypeID;
    uint32 policeCaptainTypeID;
    uint32 policeDroneTypeID;

    ConcordShipTypes()
        : policeCommanderTypeID(0),
          policeCaptainTypeID(0),
          policeDroneTypeID(0)
    { }
};

// Singleton responsible for holding the current CONCORD ship typeIDs.
// In the future we can load these from DB or config if desired.
class ConcordShips
{
public:
    static ConcordShips& Instance();

    const ConcordShipTypes& GetTypes() const { return m_types; }

    // Will log whether things look correctly configured.
    void LoadFromConfig();

private:
    ConcordShips();

    ConcordShipTypes m_types;
};

} // namespace ConcordV2

#endif // EVE_NPC_CONCORD_SHIPS_H

