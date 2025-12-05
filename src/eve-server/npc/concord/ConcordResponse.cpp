/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Response fleet spawning
 *****************************************************************************/

#include "eve-server.h"

#include "npc/concord/ConcordResponse.h"
#include "npc/concord/ConcordShips.h"

#include "inventory/ItemFactory.h"
#include "StaticDataMgr.h"

#include "system/SystemManager.h"
#include "system/SystemBubble.h"
#include "system/DestinyManager.h"
#include "npc/NPC.h"

// Master safety switch: keep this false while we're still testing.
// When set to true, ConcordV2 will actually spawn ships on crimes.
static const bool kEnableConcordSpawns = true;

namespace ConcordV2
{

ConcordResponse& ConcordResponse::Instance()
{
    static ConcordResponse s_instance;
    return s_instance;
}

bool ConcordResponse::BuildResponsePlan(const CrimeEvent& event, double delaySec, ResponsePlan& outPlan) const
{
    // Only handle high-sec for now; low-sec / null will be added later (if at all).
    if (event.systemSecurity < 0.5)
    {
        CONCORD_LOG_DEBUG(
            "ConcordResponse::BuildResponsePlan: system %u (sec=%.2f) below 0.5, no Concord response.",
            event.solarSystemID, event.systemSecurity
        );
        return false;
    }

    // Only react to clear illegal aggression for now.
    if (event.type != CrimeType::IllegalAggression)
    {
        CONCORD_LOG_DEBUG(
            "ConcordResponse::BuildResponsePlan: crimeType=%s not handled yet. No response.",
            GetCrimeTypeName(event.type)
        );
        return false;
    }

    // Pull the configured Crucible ship typeIDs.
    const ConcordShipTypes& types = ConcordShips::Instance().GetTypes();

    ResponsePlan plan;
    plan.solarSystemID    = event.solarSystemID;
    plan.systemSecurity   = event.systemSecurity;
    plan.responseDelaySec = delaySec;

    plan.commanderTypeID  = types.policeCommanderTypeID;
    plan.captainTypeID    = types.policeCaptainTypeID;
    plan.droneTypeID      = types.policeDroneTypeID;

    // Simple initial composition based on system security:
    //  1.0–0.9: 1 commander, 2 captains, 4 drones
    //  0.9–0.8: 1 commander, 3 captains, 6 drones
    //  0.8–0.7: 1 commander, 4 captains, 8 drones
    //  0.7–0.6: 2 commanders, 4 captains, 10 drones
    //  0.6–0.5: 2 commanders, 5 captains, 12 drones
    double sec = event.systemSecurity;
    if (sec >= 0.9)
    {
        plan.commanderCount = 1;
        plan.captainCount   = 2;
        plan.droneCount     = 4;
    }
    else if (sec >= 0.8)
    {
        plan.commanderCount = 1;
        plan.captainCount   = 3;
        plan.droneCount     = 6;
    }
    else if (sec >= 0.7)
    {
        plan.commanderCount = 1;
        plan.captainCount   = 4;
        plan.droneCount     = 8;
    }
    else if (sec >= 0.6)
    {
        plan.commanderCount = 2;
        plan.captainCount   = 4;
        plan.droneCount     = 10;
    }
    else    // 0.5 <= sec < 0.6
    {
        plan.commanderCount = 2;
        plan.captainCount   = 5;
        plan.droneCount     = 12;
    }

    outPlan = plan;

    CONCORD_LOG_INFO(
        "ConcordResponse::BuildResponsePlan: system %u (sec=%.2f) -> delay=%.2f, cmd=%u cap=%u drn=%u.",
        plan.solarSystemID,
        plan.systemSecurity,
        plan.responseDelaySec,
        static_cast<unsigned>(plan.commanderCount),
        static_cast<unsigned>(plan.captainCount),
        static_cast<unsigned>(plan.droneCount)
    );

    return true;
}

// Internal helper: spawn a single Concord NPC and warp it toward the offender.
static NPC* SpawnConcordShip(SystemManager& system,
                             const CrimeEvent& event,
                             uint32 shipTypeID,
                             const FactionData& factionData,
                             const GPoint& offensePos)
{
    if (shipTypeID == 0)
    {
        CONCORD_LOG_WARN("SpawnConcordShip: shipTypeID is 0. Skipping spawn.");
        return nullptr;
    }

    // Starting point: some distance away from the offense position,
    // similar to how belt rats are spawned off-bubble and warp in.
    GPoint startPos(offensePos);
    startPos.MakeRandomPointOnSphere(MakeRandomInt(10, 15) * 100000); // 1–1.5M m away

    // Slight randomization of warp-in target around the offender.
    GPoint warpToPos(offensePos);
    warpToPos.MakeRandomPointOnSphere(MakeRandomInt(1, 5) * 1000); // 1–5 km scatter

    // NOTE: ownerID/corpID here should ideally reflect the actual CONCORD corp.
    // For now, we use factionData.ownerID as provided.
    ItemData idata(shipTypeID,
                   factionData.ownerID,
                   system.GetID(),
                   flagNone,
                   "CONCORD",
                   startPos);

    InventoryItemRef iRef = sItemFactory.SpawnItem(idata);
    if (!iRef)
    {
        CONCORD_LOG_ERROR("SpawnConcordShip: Failed to spawn Concord item type %u.", shipTypeID);
        return nullptr;
    }

    CONCORD_LOG_INFO("SpawnConcordShip: Spawning Concord NPC type %u (itemID=%u) in system %u.",
                     shipTypeID, iRef->itemID(), system.GetID());

    // Build the NPC entity.
    NPC* pNPC = new NPC(iRef, system.GetServiceMgr(), &system, factionData, nullptr);
    if (pNPC == nullptr)
        return nullptr;

        if (!pNPC->Load())
    {
        CONCORD_LOG_ERROR("SpawnConcordShip: Failed to load NPC data for item %u type %u, depopping.",
                          pNPC->GetID(), pNPC->GetSelf()->typeID());
        pNPC->Delete();
        return nullptr;
    }

    system.AddNPC(pNPC);

    // Tell the Concord AI who the criminal is so it can focus fire correctly.
    pNPC->SetConcordPrimaryTarget(event.offender);

    // Set starting position and warp in toward the offender.
    pNPC->DestinyMgr()->SetPosition(startPos);

    // Warp with a small distance offset.
    double warpOffset = MakeRandomInt(-5, 10) * 1000.0; // -5km to +10km
    pNPC->DestinyMgr()->WarpTo(warpToPos, warpOffset);

    return pNPC;
}

void ConcordResponse::HandleCrime(SystemManager& system, const CrimeEvent& event, double delaySec)
{
    ResponsePlan plan;
    if (!BuildResponsePlan(event, delaySec, plan))
    {
        CONCORD_LOG_DEBUG(
            "ConcordResponse::HandleCrime: no valid response plan for system %u (sec=%.2f).",
            event.solarSystemID, event.systemSecurity
        );
        return;
    }

    // Always log the plan, even if we don't actually spawn yet.
    CONCORD_LOG_INFO(
        "ConcordResponse::HandleCrime: plan for system %u in %.2f sec [cmd=%u cap=%u drn=%u].",
        plan.solarSystemID,
        plan.responseDelaySec,
        static_cast<unsigned>(plan.commanderCount),
        static_cast<unsigned>(plan.captainCount),
        static_cast<unsigned>(plan.droneCount)
    );

    if (!kEnableConcordSpawns)
    {
        CONCORD_LOG_INFO(
            "ConcordResponse::HandleCrime: kEnableConcordSpawns=false, not spawning Concord fleet."
        );
        return;
    }

    if (event.offender == nullptr)
    {
        CONCORD_LOG_WARN(
            "ConcordResponse::HandleCrime: offender is null, cannot determine warp target. No spawn."
        );
        return;
    }

    // Build FactionData for Concord. NOTE: We may refine this later to use the
    // exact CONCORD faction/corp IDs from DB for perfect Crucible fidelity.
    FactionData fData = FactionData();
    fData.ownerID        = plan.solarSystemID;   // TEMP placeholder; TODO: use real CONCORD corpID
    fData.corporationID  = 0;
    fData.allianceID     = 0;
    fData.factionID      = 0;

    // Use the offender's current position as the center of the warp-in.
    const GPoint& offensePos = event.offender->GetPosition();

    // Spawn commanders.
    for (uint8 i = 0; i < plan.commanderCount; ++i)
    {
        SpawnConcordShip(system, event, plan.commanderTypeID, fData, offensePos);
    }

    // Spawn captains.
    for (uint8 i = 0; i < plan.captainCount; ++i)
    {
        SpawnConcordShip(system, event, plan.captainTypeID, fData, offensePos);
    }

    // Spawn drones.
    for (uint8 i = 0; i < plan.droneCount; ++i)
    {
        SpawnConcordShip(system, event, plan.droneTypeID, fData, offensePos);
    }
}

} // namespace ConcordV2

