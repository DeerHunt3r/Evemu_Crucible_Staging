/************************************************************************************
 * StaticPropSpawner.cpp
 *
 * Pure C++ gate + station environment spawner (billboards + sentry guns).
 *
 * - NO dependency on spawns / spawnGroups / spawnGroupEntries / spawnBounds tables.
 * - For each system, when SpawnForSystem() is called:
 *      - Uses SystemManager::GetGates()    to find all stargates in that system.
 *      - Uses SystemManager::GetEntities() and IsStationSE() to find stations.
 *      - Applies C++ "environment templates" around each anchor:
 *          - Gates: billboards + sentry rings (+ optional civilians)
 *          - Stations: sentry rings (+ optional civilians)
 *
 * - Only the CURRENT system is touched; no universe-wide spawning.
 * - Sentry gun types are chosen based on the owning faction of the region
 *   (Amarr, Caldari, Gallente, Minmatar) using StaticDataMgr.
 ************************************************************************************/

#include "eve-server.h"

#include "system/SystemManager.h"
#include "system/StaticPropSpawner.h"
#include "inventory/ItemFactory.h"
#include "inventory/InventoryItem.h"
#include "system/SystemEntity.h"
#include "npc/Sentry.h"
#include "StaticDataMgr.h"
#include "EVEServerConfig.h"

namespace
{
    // ---- CONFIG: typeIDs for environment props ---------------------------------

    // Billboard: CONCORD billboard.
    static const uint32 kBillboardTypeID        = 11136;

    // Default generic sentry (used as fallback and also for Gallente).
    static const uint32 kDefaultSentryGunTypeID = 3742;

    // Civilian ship type (0 = disabled for now).
    static const uint32 kCivilianTypeID         = 0;

    // Owner for static gate/station props. You can change this later to a factionID/system owner/etc.
    static const uint32 kPropOwnerID            = 1;  // 1 is commonly used NPC/system owner

    // ---- Faction IDs (EVE static data) -----------------------------------------

    static const uint32 FACTION_CALDARI   = 500001;
    static const uint32 FACTION_MINMATAR  = 500002;
    static const uint32 FACTION_AMARR     = 500003;
    static const uint32 FACTION_GALLENTE  = 500004;

    // ---- Generic environment template ------------------------------------------

    struct EnvironmentTemplate
    {
        std::vector<GPoint> billboardOffsets;
        std::vector<GPoint> sentryOffsets;
        std::vector<GPoint> civilianOffsets;
    };

    /**
     * Layout tuned to approximate TQ:
     *
     *  - Billboard:
     *      ~30 km from the anchor along +X.
     *
     *  - Sentry guns:
     *      - 8 guns total, 4 "top" and 4 "bottom".
     *      - Each gun is ~50 km from the anchor.
     *      - Top ring is ~70 km above bottom ring.
     *      - Guns in each ring are ~50 km apart from their neighbors.
     *
     *  We achieve this by:
     *      vertical half-gap h = 35 km
     *      radial distance R   = 50 km
     *      horizontal radius d = sqrt(R^2 - h^2) ≈ 35.7 km
     *
     *  Top ring:    y = +35 km, circle radius d in X/Z
     *  Bottom ring: y = -35 km, circle radius d in X/Z
     */

    EnvironmentTemplate GetGateEnvironmentTemplate()
    {
        EnvironmentTemplate tpl;

        // Billboard: 30 km along +X from the gate
        const double kBillboardDist = 30000.0;  // 30 km
        tpl.billboardOffsets.push_back(GPoint(kBillboardDist, 0.0, 0.0));

        // Sentry layout parameters
        const double kSentryRadius        = 50000.0;  // 50 km from anchor
        const double kSentryVerticalHalf  = 35000.0;  // +/- 35 km => 70 km separation
        const double kSentryHorizRadiusSq = (kSentryRadius * kSentryRadius) -
                                            (kSentryVerticalHalf * kSentryVerticalHalf);
        const double kSentryHorizRadius   = (kSentryHorizRadiusSq > 0.0)
                                            ? std::sqrt(kSentryHorizRadiusSq)
                                            : 0.0;    // ~35.7 km

        const double d = kSentryHorizRadius;
        const double h = kSentryVerticalHalf;

        // Top ring (y = +h)
        tpl.sentryOffsets.push_back(GPoint( d,  h,  0));  // +X
        tpl.sentryOffsets.push_back(GPoint(-d,  h,  0));  // -X
        tpl.sentryOffsets.push_back(GPoint( 0,  h,  d));  // +Z
        tpl.sentryOffsets.push_back(GPoint( 0,  h, -d));  // -Z

        // Bottom ring (y = -h)
        tpl.sentryOffsets.push_back(GPoint( d, -h,  0));  // +X
        tpl.sentryOffsets.push_back(GPoint(-d, -h,  0));  // -X
        tpl.sentryOffsets.push_back(GPoint( 0, -h,  d));  // +Z
        tpl.sentryOffsets.push_back(GPoint( 0, -h, -d));  // -Z

        // Civilians (still disabled unless kCivilianTypeID != 0)
        tpl.civilianOffsets.push_back(GPoint( 20000.0,  5000.0,  5000.0));
        tpl.civilianOffsets.push_back(GPoint(-20000.0, -5000.0, -5000.0));

        return tpl;
    }

    EnvironmentTemplate GetStationEnvironmentTemplate()
    {
        EnvironmentTemplate tpl;

        // No billboards at stations for now; just guns + optional civilians.

        const double kSentryRadius        = 50000.0;  // 50 km from station
        const double kSentryVerticalHalf  = 35000.0;  // +/- 35 km
        const double kSentryHorizRadiusSq = (kSentryRadius * kSentryRadius) -
                                            (kSentryVerticalHalf * kSentryVerticalHalf);
        const double kSentryHorizRadius   = (kSentryHorizRadiusSq > 0.0)
                                            ? std::sqrt(kSentryHorizRadiusSq)
                                            : 0.0;

        const double d = kSentryHorizRadius;
        const double h = kSentryVerticalHalf;

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

        // Civilians (same pattern as gates, for now)
        tpl.civilianOffsets.push_back(GPoint( 20000.0,  5000.0,  5000.0));
        tpl.civilianOffsets.push_back(GPoint(-20000.0, -5000.0, -5000.0));

        return tpl;
    }

    // ---- Faction → sentry gun mapping -----------------------------------------

    uint32 ResolveSentryTypeForFaction(uint32 factionID)
    {
        switch (factionID)
        {
            case FACTION_CALDARI: {
                // Caldari sentry guns I/II/III
                static const uint32 caldariTypes[3] = { 3740, 3741, 3739 };
                int idx = MakeRandomInt(0, 2);   // random 0..2
                return caldariTypes[idx];
            }
            case FACTION_AMARR:
                return 1194;
            case FACTION_GALLENTE:
                return 3742;
            case FACTION_MINMATAR:
                return 3743;
            default:
                // Fallback: generic sentry
                return kDefaultSentryGunTypeID;
        }
    }

    // ---- Helpers to actually spawn things -------------------------------------

    // Plain static object (billboards, debris, civilians if we ever make them static).
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

        StaticSystemEntity* pSE =
            new StaticSystemEntity(itemRef, system.GetServiceMgr(), &system);

        if (!pSE)
        {
            sLog.Error("StaticPropSpawner",
                       "Failed creating StaticSystemEntity for item %u.", itemRef->itemID());
            return;
        }

        system.AddItemToInventory(itemRef);
        system.AddEntity(pSE);

        sLog.Log("StaticPropSpawner",
                 "Spawned STATIC itemID=%u typeID=%u at (%.0f, %.0f, %.0f) in system %u.",
                 itemRef->itemID(), typeID, position.x, position.y, position.z, systemID);
    }

    // Sentry gun as a real NPC (uses Sentry + SentryAI).
    void SpawnSentryGunAt(
        SystemManager& system,
        uint32 systemID,
        uint32 typeID,
        uint32 factionID,
        uint32 ownerID,
        const GPoint& position)
    {
        if (typeID == 0)
            return;

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
                         "Failed to create SENTRY item type %u at (%.0f, %.0f, %.0f) in system %u.",
                         typeID, position.x, position.y, position.z, systemID);
            return;
        }

        // FactionData is defined in the NPC system (used by NPC / Sentry).
        FactionData fData;
        fData.allianceID    = 0;
        fData.factionID     = factionID;   // empire faction (Amarr/Caldari/etc.) from StaticDataMgr
        fData.corporationID = ownerID;     // for now tie corp to owner (can refine later)
        fData.ownerID       = ownerID;     // NPC/system owner

        Sentry* pSE = new Sentry(itemRef, system.GetServiceMgr(), &system, fData);
        if (!pSE)
        {
            sLog.Error("StaticPropSpawner",
                       "Failed creating Sentry entity for item %u.", itemRef->itemID());
            return;
        }

        system.AddItemToInventory(itemRef);
        system.AddEntity(pSE);

        sLog.Log("StaticPropSpawner",
                 "Spawned SENTRY itemID=%u typeID=%u (factionID=%u) at (%.0f, %.0f, %.0f) in system %u.",
                 itemRef->itemID(), typeID, factionID, position.x, position.y, position.z, systemID);
    }

    // Gates: use SystemManager::GetGates()
    void SpawnGateEnvironmentForSystem(SystemManager& system,
                                       uint32 systemID,
                                       uint32 factionID)
    {
        EnvironmentTemplate tpl = GetGateEnvironmentTemplate();

        std::map<uint32, SystemEntity*> gates = system.GetGates();

        sLog.Log("StaticPropSpawner",
                 "SpawnGateEnvironmentForSystem(): system %u has %zu gates; factionID=%u.",
                 systemID, gates.size(), factionID);

        if (gates.empty())
            return;

        for (std::map<uint32, SystemEntity*>::const_iterator it = gates.begin();
             it != gates.end(); ++it)
        {
            uint32 gateItemID   = it->first;
            SystemEntity* gate  = it->second;
            if (gate == nullptr)
                continue;

            GPoint gatePos = gate->GetPosition();

            sLog.Log("StaticPropSpawner",
                     "Gate %u at (%.0f, %.0f, %.0f) in system %u — spawning gate environment.",
                     gateItemID, gatePos.x, gatePos.y, gatePos.z, systemID);

            // Billboards
            for (size_t b = 0; b < tpl.billboardOffsets.size(); ++b)
            {
                GPoint pos = gatePos + tpl.billboardOffsets[b];
                SpawnStaticPropAt(system, systemID, kBillboardTypeID, kPropOwnerID, pos);
            }

            // Sentry guns
            for (size_t s = 0; s < tpl.sentryOffsets.size(); ++s)
            {
                uint32 sentryTypeID = ResolveSentryTypeForFaction(factionID);
                GPoint pos          = gatePos + tpl.sentryOffsets[s];
                SpawnSentryGunAt(system, systemID, sentryTypeID, factionID, kPropOwnerID, pos);
            }

            // Civilians (if enabled)
            for (size_t c = 0; c < tpl.civilianOffsets.size(); ++c)
            {
                GPoint pos = gatePos + tpl.civilianOffsets[c];
                SpawnStaticPropAt(system, systemID, kCivilianTypeID, kPropOwnerID, pos);
            }
        }
    }

    // Stations: scan all entities and pick those that are StationSE
    void SpawnStationEnvironmentForSystem(SystemManager& system,
                                          uint32 systemID,
                                          uint32 factionID)
    {
        EnvironmentTemplate tpl = GetStationEnvironmentTemplate();

        std::map<uint32, SystemEntity*> entities = system.GetEntities();

        size_t stationCount = 0;
        for (std::map<uint32, SystemEntity*>::const_iterator it = entities.begin();
             it != entities.end(); ++it)
        {
            SystemEntity* ent = it->second;
            if (ent && ent->IsStationSE())
                ++stationCount;
        }

        sLog.Log("StaticPropSpawner",
                 "SpawnStationEnvironmentForSystem(): system %u has %zu total entities, %zu stations; factionID=%u.",
                 systemID, entities.size(), stationCount, factionID);

        if (stationCount == 0)
            return;

        for (std::map<uint32, SystemEntity*>::const_iterator it = entities.begin();
             it != entities.end(); ++it)
        {
            SystemEntity* station = it->second;
            if (!station || !station->IsStationSE())
                continue;

            uint32 stationItemID = station->GetID();
            GPoint stationPos    = station->GetPosition();

            sLog.Log("StaticPropSpawner",
                     "Station %u at (%.0f, %.0f, %.0f) in system %u — spawning station environment.",
                     stationItemID, stationPos.x, stationPos.y, stationPos.z, systemID);

            // Sentry guns only (no billboard at stations)
            for (size_t s = 0; s < tpl.sentryOffsets.size(); ++s)
            {
                uint32 sentryTypeID = ResolveSentryTypeForFaction(factionID);
                GPoint pos          = stationPos + tpl.sentryOffsets[s];
                SpawnSentryGunAt(system, systemID, sentryTypeID, factionID, kPropOwnerID, pos);
            }

            // Civilians (if enabled)
            for (size_t c = 0; c < tpl.civilianOffsets.size(); ++c)
            {
                GPoint pos = stationPos + tpl.civilianOffsets[c];
                SpawnStaticPropAt(system, systemID, kCivilianTypeID, kPropOwnerID, pos);
            }
        }
    }

} // anonymous namespace

// ---- Public entry point --------------------------------------------------------

bool StaticPropSpawner::SpawnForSystem(SystemManager& system)
{
    const uint32 systemID   = system.GetID();
    const char*  systemName = system.GetName();

    // Determine dominant faction for this system from its region.
    uint32 regionID  = system.GetRegionID();
    uint32 factionID = sDataMgr.GetRegionFaction(regionID);

    sLog.Log("StaticPropSpawner",
             "SpawnForSystem() [GATE+STATION TEMPLATE v4 — PURE C++, FACTION SENTRY NPC] called for system %u (%s), region %u (factionID=%u).",
             systemID, systemName, regionID, factionID);

    // Gates
    SpawnGateEnvironmentForSystem(system, systemID, factionID);

    // Stations
    SpawnStationEnvironmentForSystem(system, systemID, factionID);

    sLog.Log("StaticPropSpawner",
             "SpawnForSystem(): finished C++ template spawns for system %u (%s).",
             systemID, systemName);

    return true;
}

