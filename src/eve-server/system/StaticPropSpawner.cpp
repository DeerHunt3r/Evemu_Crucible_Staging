/************************************************************************************
 * StaticPropSpawner.cpp
 *
 * Pure C++ gate environment spawner.
 *
 * - No dependency on spawns / spawnGroups / spawnGroupEntries / spawnBounds tables.
 * - For each system, when SpawnForSystem() is called:
 *      - Uses SystemManager::GetGates() to find all stargates in that system.
 *      - Applies a C++ "gate environment template" around each gate:
 *          billboards, sentry guns, civilian ships (if typeIDs are set).
 *
 * - Only the CURRENT system is touched; no universe-wide spawning.
 ************************************************************************************/

#include "eve-server.h"

#include "system/StaticPropSpawner.h"
#include "system/SystemManager.h"
#include "inventory/ItemFactory.h"
#include "inventory/InventoryItem.h"
#include "system/SystemEntity.h"
#include "EVEServerConfig.h"

namespace
{
    // ---- CONFIG: typeIDs for your gate environment props -----------------------

    // Rob's current choices:
    //   - Concord billboard  : typeID 11136
    //   - Sentry gun         : typeID 3742
    //   - Civilian ship      : not set yet (0 = disabled)
    static const uint32 kBillboardTypeID = 11136;
    static const uint32 kSentryGunTypeID = 3742;
    static const uint32 kCivilianTypeID  = 0;

    // Owner for static gate props. You can change this later to a factionID/system owner/etc.
    static const uint32 kGatePropOwnerID = 1;  // 1 is commonly used NPC/system owner

    // ---- Gate + template definitions ------------------------------------------

    struct GateEnvironmentTemplate
    {
        std::vector<GPoint> billboardOffsets;
        std::vector<GPoint> sentryOffsets;
        std::vector<GPoint> civilianOffsets;

        uint32 billboardTypeID;
        uint32 sentryTypeID;
        uint32 civilianTypeID;

        uint32 ownerID;
    };

    // Simple default layout:
    //  - 1 billboard ~15km "in front" of the gate (we just choose +X for now)
    //  - 4 sentry guns above the gate, 4 below (forming a loose box)
    //  - 2 civilian ships off to the sides
    GateEnvironmentTemplate GetDefaultGateEnvironmentTemplate()
    {
        GateEnvironmentTemplate tpl;

        tpl.billboardTypeID = kBillboardTypeID;
        tpl.sentryTypeID    = kSentryGunTypeID;
        tpl.civilianTypeID  = kCivilianTypeID;
        tpl.ownerID         = kGatePropOwnerID;

        // Billboard: 15 km along +X from the gate
        tpl.billboardOffsets.push_back(GPoint(15000.0, 0.0, 0.0));

        // Sentry box: 4 up, 4 down around the gate at ~10 km radius
        const double d = 10000.0;
        const double h = 8000.0;

        // Top ring
        tpl.sentryOffsets.push_back(GPoint( d,  h,  0));
        tpl.sentryOffsets.push_back(GPoint(-d,  h,  0));
        tpl.sentryOffsets.push_back(GPoint( 0,  h,  d));
        tpl.sentryOffsets.push_back(GPoint( 0,  h, -d));

        // Bottom ring
        tpl.sentryOffsets.push_back(GPoint( d, -h,  0));
        tpl.sentryOffsets.push_back(GPoint(-d, -h,  0));
        tpl.sentryOffsets.push_back(GPoint( 0, -h,  d));
        tpl.sentryOffsets.push_back(GPoint( 0, -h, -d));

        // A couple of civilian ships off to the sides (disabled until kCivilianTypeID != 0)
        tpl.civilianOffsets.push_back(GPoint( 20000.0,  5000.0,  5000.0));
        tpl.civilianOffsets.push_back(GPoint(-20000.0, -5000.0, -5000.0));

        return tpl;
    }

    // ---- Spawn helper ----------------------------------------------------------

    // Helper to actually spawn a static prop into the system at a given position.
    void SpawnStaticPropAt(
        SystemManager& system,
        uint32 systemID,
        uint32 typeID,
        uint32 ownerID,
        const GPoint& position)
    {
        if (typeID == 0)
            return; // typeID not configured; silently skip

        ItemData idata(
            uint16(typeID),
            ownerID,
            systemID,
            flagNone,
            "",
            position,
            "",
            false);

        InventoryItemRef itemRef = sItemFactory.SpawnItem(idata);
        if (!itemRef)
        {
            sLog.Warning("StaticPropSpawner",
                         "Failed to create item type %u at (%.0f, %.0f, %.0f) in system %u.",
                         typeID, position.x, position.y, position.z, systemID);
            return;
        }

        // StaticSystemEntity is declared in SystemEntity.h in this fork.
        StaticSystemEntity* pSE =
            new StaticSystemEntity(itemRef, system.GetServiceMgr(), &system);

        if (!pSE)
        {
            sLog.Error("StaticPropSpawner",
                       "Failed creating StaticSystemEntity for item %u.", itemRef->itemID());
            return;
        }

        // Register via SystemManager's public APIs.
        system.AddItemToInventory(itemRef);
        system.AddEntity(pSE);

        sLog.Log("StaticPropSpawner",
                 "Spawned itemID=%u typeID=%u at (%.0f, %.0f, %.0f) in system %u.",
                 itemRef->itemID(), typeID, position.x, position.y, position.z, systemID);
    }

    // Spawn the environment template around every gate provided by SystemManager::GetGates()
    void SpawnGateEnvironmentForSystem(SystemManager& system,
                                       uint32 systemID)
    {
        GateEnvironmentTemplate tpl = GetDefaultGateEnvironmentTemplate();

        if (tpl.billboardTypeID == 0 &&
            tpl.sentryTypeID == 0 &&
            tpl.civilianTypeID == 0)
        {
            sLog.Warning("StaticPropSpawner",
                         "SpawnGateEnvironmentForSystem(): "
                         "no typeIDs configured; nothing will be spawned.");
            return;
        }

        // Use SystemManager's gate map instead of querying the DB again.
        std::map<uint32, SystemEntity*> gates = system.GetGates();

        if (gates.empty())
        {
            sLog.Log("StaticPropSpawner",
                     "SpawnGateEnvironmentForSystem(): system %u has no stargates; nothing to do.",
                     systemID);
            return;
        }

        sLog.Log("StaticPropSpawner",
                 "SpawnGateEnvironmentForSystem(): system %u has %zu gates; spawning environment.",
                 systemID, gates.size());

        for (std::map<uint32, SystemEntity*>::const_iterator it = gates.begin();
             it != gates.end(); ++it)
        {
            uint32 gateItemID = it->first;
            SystemEntity* gateEnt = it->second;

            if (gateEnt == nullptr)
                continue;

            GPoint gatePos = gateEnt->GetPosition();

            sLog.Log("StaticPropSpawner",
                     "Spawning gate environment for gate %u at (%.0f, %.0f, %.0f) in system %u.",
                     gateItemID, gatePos.x, gatePos.y, gatePos.z, systemID);

            // Billboards
            for (size_t b = 0; b < tpl.billboardOffsets.size(); ++b)
            {
                GPoint pos = gatePos + tpl.billboardOffsets[b];
                SpawnStaticPropAt(system, systemID, tpl.billboardTypeID, tpl.ownerID, pos);
            }

            // Sentry guns
            for (size_t s = 0; s < tpl.sentryOffsets.size(); ++s)
            {
                GPoint pos = gatePos + tpl.sentryOffsets[s];
                SpawnStaticPropAt(system, systemID, tpl.sentryTypeID, tpl.ownerID, pos);
            }

            // Civilian ships (only if typeID is set)
            for (size_t c = 0; c < tpl.civilianOffsets.size(); ++c)
            {
                GPoint pos = gatePos + tpl.civilianOffsets[c];
                SpawnStaticPropAt(system, systemID, tpl.civilianTypeID, tpl.ownerID, pos);
            }
        }
    }

} // anonymous namespace

// ---- Public entry point --------------------------------------------------------

bool StaticPropSpawner::SpawnForSystem(SystemManager& system)
{
    const uint32 systemID = system.GetID();
    const char* systemName = system.GetName();

    sLog.Log("StaticPropSpawner",
             "SpawnForSystem() [GATE TEMPLATE v1 — NO SQL] called for system %u (%s).",
             systemID, systemName);

    SpawnGateEnvironmentForSystem(system, systemID);

    sLog.Log("StaticPropSpawner",
             "SpawnForSystem(): finished C++ template spawns for system %u (%s).",
             systemID, systemName);

    return true;
}

