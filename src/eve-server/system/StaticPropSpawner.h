/************************************************************************************
 * @name   StaticPropSpawner.h
 * @brief  Spawns static gate environment props (billboards, sentry guns, etc.)
 *         for a single solar system, based purely on C++ templates.
 *
 * This spawner:
 *   - Has NO dependency on spawns / spawnGroups / spawnGroupEntries / spawnBounds.
 *   - Uses SystemManager::GetGates() to find stargates in the current system.
 *   - Spawns a fixed layout of props around each gate.
 ************************************************************************************/

#ifndef EVEMU_SYSTEM_STATIC_PROP_SPAWNER_H
#define EVEMU_SYSTEM_STATIC_PROP_SPAWNER_H

class SystemManager;

/**
 * StaticPropSpawner
 *
 * Stateless helper used by SystemManager to populate "gate environments"
 * (billboards, sentry guns, civilian ships) when a system is booted.
 */
class StaticPropSpawner
{
public:
    StaticPropSpawner() = default;
    ~StaticPropSpawner() = default;

    /**
     * Spawns static gate props for the given system using C++ templates.
     *
     * Returns true on success or if there is simply nothing to spawn.
     * Returns false only on hard failures (e.g. DB query failures).
     */
    bool SpawnForSystem(SystemManager& system);
};

#endif // EVEMU_SYSTEM_STATIC_PROP_SPAWNER_H

