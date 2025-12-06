/************************************************************************************
 * StaticPropSpawner.cpp
 *
 * Pure C++ static environment spawner for:
 *  - Gate environments (billboards + sentry guns + optional civilians)
 *  - Station environments (sentry guns + optional civilians)
 *  - Subspace beacons near gates
 *
 * NO dependency on:
 *  - spawns
 *  - spawnGroups
 *  - spawnGroupEntries
 *  - spawnBounds
 *
 * All placement logic is template-driven here in C++.
 ************************************************************************************/

#include "eve-server.h"

#include "system/SystemManager.h"
#include "system/StaticPropSpawner.h"
#include "inventory/ItemFactory.h"
#include "inventory/InventoryItem.h"
#include "system/SystemEntity.h"
#include "system/BubbleManager.h"
#include "EVEServerConfig.h"
#include "StaticDataMgr.h"
#include "npc/Sentry.h"
#include "system/Celestial.h"

// For FACTION_* ids and region → faction lookups
//#include "EVEDB/InvTypes.h"

namespace
{
    // -------------------------------------------------------------------------
    //  CONFIG
    // -------------------------------------------------------------------------

    // Static prop typeIDs
    static const uint32 kBillboardTypeID        = 11136;   // Concord billboard
    static const uint32 kDefaultSentryGunTypeID = 3742;    // Gallente sentry gun as fallback
    static const uint32 kCivilianTypeID         = 0;       // 0 = disabled for now

    // Owner for all gate/station props (can refine later to empire corps etc.)
    static const uint32 kPropOwnerID = 1;                  // "system" / generic NPC owner

    // Subspace beacon type
    static const uint32 kSubspaceBeaconTypeID   = 30391;   // Subspace Beacon

    // -------------------------------------------------------------------------
    //  Environment template
    // -------------------------------------------------------------------------

    struct EnvironmentTemplate
    {
        std::vector<GPoint> billboardOffsets;
        std::vector<GPoint> sentryOffsets;
        std::vector<GPoint> civilianOffsets;
    };

    // Gates:
    //  - 1 billboard ~30km ahead of gate on +X
    //  - 8 sentries in a "box" 50km from gate center:
    //      * 4 "top" at +Y, spaced on X/Z
    //      * 4 "bottom" at -Y, spaced on X/Z
    EnvironmentTemplate GetGateEnvironmentTemplate()
    {
        EnvironmentTemplate tpl;

        // Billboard ~30km in front of gate (arbitrary +X direction)
        tpl.billboardOffsets.push_back(GPoint(30000.0, 0.0, 0.0));

        // Sentry box for gates:
        // Distance from gate center in Y (vertical) and X/Z (horizontal)
        const double kGateSentryRadius       = 50000.0; // 50 km
        const double kGateSentryVerticalHalf = 35000.0; // +/- 35 km
        const double kGateSentryHorizRadiusSq =
            (kGateSentryRadius * kGateSentryRadius) -
            (kGateSentryVerticalHalf * kGateSentryVerticalHalf);

        const double kGateSentryHorizRadius =
            (kGateSentryHorizRadiusSq > 0.0)
                ? std::sqrt(kGateSentryHorizRadiusSq)
                : 0.0;

        const double d = kGateSentryHorizRadius;
        const double h = kGateSentryVerticalHalf;

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

        // Civilians (disabled unless kCivilianTypeID != 0)
        tpl.civilianOffsets.push_back(GPoint( 20000.0,  5000.0,  5000.0));
        tpl.civilianOffsets.push_back(GPoint(-20000.0, -5000.0, -5000.0));

        return tpl;
    }

    // Stations:
    //  - No billboard at stations (for now)
    //  - 8 sentries in a similar box around the station
    EnvironmentTemplate GetStationEnvironmentTemplate()
    {
        EnvironmentTemplate tpl;

        // No billboards at stations for now; just guns + optional civilians.

        const double kSentryRadius        = 50000.0;  // 50 km from station
        const double kSentryVerticalHalf  = 35000.0;  // +/- 35 km
        const double kSentryHorizRadiusSq =
            (kSentryRadius * kSentryRadius) -
            (kSentryVerticalHalf * kSentryVerticalHalf);

        const double kSentryHorizRadius =
            (kSentryHorizRadiusSq > 0.0)
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

        // Civilians (same pattern as gates for now)
        tpl.civilianOffsets.push_back(GPoint( 20000.0,  5000.0,  5000.0));
        tpl.civilianOffsets.push_back(GPoint(-20000.0, -5000.0, -5000.0));

        return tpl;
    }

    // -------------------------------------------------------------------------
    //  Faction → sentry gun mapping
    // -------------------------------------------------------------------------

// -------------------------------------------------------------------------
//  Faction → sentry gun mapping (no EVEDB/InvTypes.h needed)
// -------------------------------------------------------------------------
uint32 ResolveSentryTypeForFaction(uint32 factionID)
{
    // Ask StaticDataMgr for the faction name (e.g. "Caldari State")
    std::string fname = sDataMgr.GetFactionName(factionID);

    // Normalize a bit just in case (optional)
    // (If you want you can lowercase this string, but not strictly required
    //  if your faction names are the usual "Caldari", "Amarr", etc.)

    if (fname.find("Caldari") != std::string::npos)
    {
        // Caldari sentry guns I/II/III (random)
        static const uint32 caldariTypes[3] = { 3740, 3741, 3739 };
        int idx = MakeRandomInt(0, 2);
        return caldariTypes[idx];
    }

    if (fname.find("Amarr") != std::string::npos)
    {
        return 1194;        // Amarr sentry gun
    }

    if (fname.find("Gallente") != std::string::npos)
    {
        return 3742;        // Gallente sentry gun
    }

    if (fname.find("Minmatar") != std::string::npos)
    {
        return 3743;        // Minmatar sentry gun
    }

    // Unknown / non-empire faction -> generic fallback
    return kDefaultSentryGunTypeID;
}

    // -------------------------------------------------------------------------
    //  Helpers to actually spawn entities
    // -------------------------------------------------------------------------

    // Plain static object (billboards, debris, *static* civilians, etc.)
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
        fData.factionID     = factionID;   // empire faction (Amarr/Caldari/etc.)
        fData.corporationID = ownerID;     // tie corp to owner for now
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
                 itemRef->itemID(), typeID, factionID,
                 position.x, position.y, position.z, systemID);
    }

    // -------------------------------------------------------------------------
    //  Gate environments
    // -------------------------------------------------------------------------

    // Use SystemManager::GetGates() to place billboards & sentries.
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
            uint32        gateItemID = it->first;
            SystemEntity* gate       = it->second;
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

    // -------------------------------------------------------------------------
    //  Station environments
    // -------------------------------------------------------------------------

    // Scan all entities and pick those that are StationSE.
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

    // -------------------------------------------------------------------------
    //  Subspace Beacons near gates
    // -------------------------------------------------------------------------

void SpawnGateBeaconsForSystem(SystemManager& system)
{
    const uint32 systemID   = system.GetID();
    const char*  systemName = system.GetName();

    std::map<uint32, SystemEntity*> gates = system.GetGates();
    if (gates.empty())
    {
        sLog.Log("StaticPropSpawner",
                 "SpawnGateBeaconsForSystem(): system %u (%s) has no gates; skipping beacons.",
                 systemID, systemName);
        return;
    }

    sLog.Log("StaticPropSpawner",
             "SpawnGateBeaconsForSystem(): system %u (%s) has %zu gates; spawning Subspace Beacons.",
             systemID, systemName, gates.size());

    for (std::map<uint32, SystemEntity*>::const_iterator it = gates.begin();
         it != gates.end(); ++it)
    {
        SystemEntity* gateSE = it->second;
        if (!gateSE)
            continue;

        GPoint gatePos = gateSE->GetPosition();

        // Place beacon ~15km "in front" of gate (same idea as billboards)
        GPoint beaconPos(
            gatePos.x + 15000.0,
            gatePos.y,
            gatePos.z);

        // Use the same static-prop path as billboards/sentries
        SpawnStaticPropAt(system, systemID, kSubspaceBeaconTypeID, kPropOwnerID, beaconPos);

        sLog.Log("StaticPropSpawner",
                 "SpawnGateBeaconsForSystem(): requested Subspace Beacon type %u at (%.0f, %.0f, %.0f) "
                 "near gate %u in system %u (%s).",
                 kSubspaceBeaconTypeID,
                 beaconPos.x, beaconPos.y, beaconPos.z,
                 gateSE->GetID(),
                 systemID,
                 systemName);
    }
  } 
}

// -----------------------------------------------------------------------------
//  Public entry point
// -----------------------------------------------------------------------------

bool StaticPropSpawner::SpawnForSystem(SystemManager& system)
{
    const uint32 systemID   = system.GetID();
    const char*  systemName = system.GetName();

    // Determine dominant faction for this system from its region.
    uint32 regionID  = system.GetRegionID();
    uint32 factionID = sDataMgr.GetRegionFaction(regionID);

    sLog.Log("StaticPropSpawner",
             "SpawnForSystem() [GATE+STATION TEMPLATE v4 — PURE C++, FACTION SENTRY NPC + BEACONS] "
             "called for system %u (%s), region %u (factionID=%u).",
             systemID, systemName, regionID, factionID);

    // Gates
    SpawnGateEnvironmentForSystem(system, systemID, factionID);

    // Stations
    SpawnStationEnvironmentForSystem(system, systemID, factionID);

    // Subspace beacons near gates
    SpawnGateBeaconsForSystem(system);

    sLog.Log("StaticPropSpawner",
             "SpawnForSystem(): finished C++ template spawns for system %u (%s).",
             systemID, systemName);

    return true;
}

