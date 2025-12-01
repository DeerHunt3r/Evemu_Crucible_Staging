/************************************************************************************
 * ConcordSpawner.h
 *
 * Reactive CONCORD response for high-sec criminal acts.
 *
 * Usage:
 *   - Call ConcordSpawner::OnCriminalAct(system, attacker)
 *     when an illegal aggression occurs in a 0.5+ system.
 *
 *   - This will spawn one or more CONCORD ships near the attacker and
 *     have them immediately target and engage the attacker.
 ************************************************************************************/

#ifndef __EVE_CONCORD_SPAWNER_H__
#define __EVE_CONCORD_SPAWNER_H__

#include "EVEServerConfig.h"

class SystemManager;
class Client;

namespace ConcordSpawner
{
    // Call this when an illegal act (criminal-level aggression) occurs in a 0.5+ system.
    void OnCriminalAct(SystemManager& system, Client* attacker);
}

#endif  // __EVE_CONCORD_SPAWNER_H__

