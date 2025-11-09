// src/eve-server/decorators/GateDecorator_Billboards.cpp
#include "decorators/GateDecorator_Billboards.h"

#include "system/SystemManager.h"
#include "system/SystemEntity.h"      // StaticSystemEntity
#include "system/BubbleManager.h"

#include "inventory/ItemFactory.h"
#include "inventory/InventoryItem.h"
#include "inventory/ItemRef.h"

#include "log/LogNew.h"

#include <cmath>
#include <random>
#include <string>
#include <vector>

// ---------- small helpers ----------
struct V3 { double x, y, z; };
static inline double dist2(const V3& a, const V3& b) {
    double dx=a.x-b.x, dy=a.y-b.y, dz=a.z-b.z; return dx*dx+dy*dy+dz*dz;
}
struct UnitRng {
    std::mt19937_64 rng;
    explicit UnitRng(uint64_t seed){ rng.seed(seed ^ 0x9E3779B97F4A7C15ULL); }
    double u01(){ return std::uniform_real_distribution<double>(0.0,1.0)(rng); }
    double range(double a,double b){ return std::uniform_real_distribution<double>(a,b)(rng); }
};
static V3 rand_unit(UnitRng& R){
    double u = 2.0*R.u01()-1.0;
    double t = 2.0*M_PI*R.u01();
    double s = std::sqrt(1.0-u*u);
    return { s*std::cos(t), s*std::sin(t), u };
}
static V3 add(const V3& a, const V3& b, double s){ return {a.x+b.x*s, a.y+b.y*s, a.z+b.z*s}; }

// ---------- light views ----------
struct GateInfo { uint32 itemID; std::string name; V3 pos; SystemEntity* ent; };
struct BeltInfo { uint32 itemID; std::string name; V3 pos; SystemEntity* ent; };
struct Thing   { int typeID; V3 pos; };

// ---------- forward local helpers ----------
static std::vector<GateInfo> GetStargates(SystemManager& sys);
static std::vector<BeltInfo> GetBelts(SystemManager& sys);
static std::vector<Thing>    SnapshotEntities(SystemManager& sys);
static bool                  anyOfTypeNear(const std::vector<Thing>& all, const V3& p, int typeID, double withinM);
static SystemEntity*         SpawnStatic(SystemManager& sys, int typeID, const V3& p, const std::string& name);

// ============================================================================
//                                   API
// ============================================================================
namespace GateDecorators {

void DecorateGates(SystemManager& sys, const Config& cfg)
{
    const auto gates   = GetStargates(sys);
    if (gates.empty()) return;

    const auto present = SnapshotEntities(sys);  // for dupe checks

    for (const auto& gate : gates) {
        UnitRng R(gate.itemID);

        // Billboard: one per gate, 15–30 km, skip if one already nearby
        if (cfg.typeID_Billboard > 0) {
            if (!anyOfTypeNear(present, gate.pos, cfg.typeID_Billboard, cfg.dupRadiusMeters)) {
                V3 pos = add(gate.pos, rand_unit(R), R.range(cfg.minOffsetMeters, cfg.maxOffsetMeters));
                SpawnStatic(sys, cfg.typeID_Billboard, pos, "Billboard @ " + gate.name);
            }
        }

        // Optional: gate patrol(s)
        if (cfg.enablePatrols && cfg.typeID_Patrol > 0 && cfg.patrolsPerGate > 0) {
            for (int i=0; i<cfg.patrolsPerGate; ++i) {
                V3 start = add(gate.pos, rand_unit(R), cfg.patrolSpawnOffset);
                SystemEntity* npc = SpawnStatic(sys, cfg.typeID_Patrol, start, "CONCORD Police " + gate.name);
                (void)npc; // TODO: hook AI to orbit gate.ent using your AI/nav layer
            }
        }
    }
}

void DecorateBelts(SystemManager& sys, const Config& cfg)
{
    if (!cfg.enableBeltPatrols || cfg.typeID_BeltPatrol <= 0) return;

    const auto belts = GetBelts(sys);
    if (belts.empty()) return;

    const auto present = SnapshotEntities(sys);
    int spawned = 0;

    for (const auto& belt : belts) {
        if (spawned >= cfg.maxBeltPatrolsPerSystem) break;

        if (anyOfTypeNear(present, belt.pos, cfg.typeID_BeltPatrol, cfg.patrolDupeRadius))
            continue;

        UnitRng R(belt.itemID ^ 0xB3117B77ULL); // stable per-belt seed
        V3 start = add(belt.pos, rand_unit(R), cfg.beltSpawnOffset);

        for (int i=0; i<cfg.patrolsPerBelt && spawned<cfg.maxBeltPatrolsPerSystem; ++i) {
            SystemEntity* npc = SpawnStatic(sys, cfg.typeID_BeltPatrol, start, "CONCORD Belt Patrol @ " + belt.name);
            (void)npc; // TODO: AI orbit + engage rats
            ++spawned;
        }
    }
}

} // namespace GateDecorators

// ============================================================================
//                              local helpers
// ============================================================================
static std::vector<GateInfo> GetStargates(SystemManager& sys)
{
    std::vector<GateInfo> out;
    auto ents = sys.GetEntities();  // public API returns a copy
    out.reserve(ents.size());
    for (const auto& kv : ents) {
        SystemEntity* e = kv.second; if (!e) continue;
        if (e->IsGateSE()) {
            auto p = e->GetPosition();
            std::string nm = e->GetSelf() ? e->GetSelf()->itemName() : "Stargate";
            out.push_back({ e->GetID(), nm, {p.x,p.y,p.z}, e });
        }
    }
    return out;
}

static std::vector<BeltInfo> GetBelts(SystemManager& sys)
{
    std::vector<BeltInfo> out;
    auto ents = sys.GetEntities();
    for (const auto& kv : ents) {
        SystemEntity* e = kv.second; if (!e) continue;
        if (e->IsBeltSE()) {
            auto p = e->GetPosition();
            std::string nm = e->GetSelf() ? e->GetSelf()->itemName() : "Asteroid Belt";
            out.push_back({ e->GetID(), nm, {p.x,p.y,p.z}, e });
        }
    }
    return out;
}

static std::vector<Thing> SnapshotEntities(SystemManager& sys)
{
    std::vector<Thing> out;
    auto ents = sys.GetEntities();
    out.reserve(ents.size());
    for (const auto& kv : ents) {
        SystemEntity* e = kv.second; if (!e) continue;
        auto p = e->GetPosition();
        out.push_back({ e->GetTypeID(), {p.x,p.y,p.z} });
    }
    return out;
}

static bool anyOfTypeNear(const std::vector<Thing>& all, const V3& p, int typeID, double withinM)
{
    const double r2 = withinM*withinM;
    for (const auto& t : all)
        if ((typeID<=0 || t.typeID==typeID) && dist2(t.pos, p) < r2)
            return true;
    return false;
}

static SystemEntity* SpawnStatic(SystemManager& sys, int typeID, const V3& pos, const std::string& name)
{
    // Build minimal item data; ItemFactory takes care of persistence
    ItemData idata(
        typeID,
        1,                      // ownerID (EVE System)
        sys.GetID(),            // locationID = current solarSystemID
        flagNone,               // inventory flag
        name.c_str(),
        1.0
    );
    idata.position = GPoint(pos.x, pos.y, pos.z); // GPoint is available via includes transitively

    InventoryItemRef itemRef = sItemFactory.SpawnItem(idata);
    if (!itemRef) {
        _log(SERVER__INIT, "SpawnStatic: SpawnItem failed for typeID %d in system %u", typeID, sys.GetID());
        return nullptr;
    }

    // Wrap in a static SE, put on grid, and register using the public API
    auto* sse = new StaticSystemEntity(itemRef, sys.GetServiceMgr(), &sys);
    sBubbleMgr.Add(sse);               // on-grid
    sys.AddEntity(sse, false);         // register; no extra anomaly signal

    // Keep behavior consistent with other statics
    sse->LoadExtras();
    sys.SendStaticBall(sse);

    return sse;
}

