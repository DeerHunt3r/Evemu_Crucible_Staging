#pragma once
#include <cstdint>
#include <string>

struct Vec3 { double x,y,z; }; // replace with your engine vector if you have one

class SolarSystem;  // forward decl
class SystemManager; // forward decl

namespace GateDecorators {

struct Config {
    // Billboard on gates
    int      typeID_Billboard    = 11136;     // CONCORD Billboard
    double   minOffsetMeters     = 15000.0;   // 15 km
    double   maxOffsetMeters     = 30000.0;   // 30 km
    double   dupRadiusMeters     = 40000.0;   // avoid near-duplicates per gate

    // Gate patrols
    bool     enablePatrols       = true;
    int      typeID_Patrol       = 11125;     // CONCORD Police Commander
    int      patrolsPerGate      = 1;
    double   patrolSpawnOffset   = 22000.0;
    double   patrolOrbitRadius   = 18000.0;
    double   patrolOrbitSpeed    = 900.0;

    // Belt patrols
    bool     enableBeltPatrols   = true;
    int      typeID_BeltPatrol   = 11125;     // same as patrol by default
    int      patrolsPerBelt      = 1;
    double   beltSpawnOffset     = 18000.0;
    double   beltOrbitRadius     = 15000.0;
    double   beltOrbitSpeed      = 800.0;

    // Guards/caps
    int      maxBeltPatrolsPerSystem = 8;
    double   patrolDupeRadius    = 30000.0;
    bool     skipWormholes       = true;      // skip J-space by default
    bool     onlyHighSec         = false;     // set true if you want belts only in >=0.5 sec
    double   minSecForBelts      = 0.5;
};

// These are no-ops for now; Step 2 will add the real logic.
void DecorateGates(SystemManager& sys, const Config& cfg);
void DecorateBelts(SystemManager& sys, const Config& cfg);

} // namespace GateDecorators

