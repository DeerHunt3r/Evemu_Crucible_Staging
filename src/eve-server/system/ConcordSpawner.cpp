/************************************************************************************
 * ConcordSpawner.cpp (stub)
 *
 * Temporary stub implementation so calls from Damage.cpp compile cleanly.
 * Currently this DOES NOT spawn any CONCORD ships. It only logs that it was called.
 *
 * Later we can incrementally implement real CONCORD logic that matches your
 * NPC / NPCAI / spawn pipeline, instead of the incompatible “big bang” version.
 ************************************************************************************/

#include "eve-server.h"

#include "system/ConcordSpawner.h"
#include "system/SystemManager.h"
#include "Client.h"

namespace ConcordSpawner
{

void OnCriminalAct(SystemManager& system, Client* attacker)
{
    if (attacker == nullptr)
        return;

    const uint32 systemID   = system.GetID();
    const char*  systemName = system.GetName();

    sLog.Log(
        "ConcordSpawner",
        "OnCriminalAct(): stub called for attacker charID=%u in system %u (%s). "
        "No CONCORD NPCs are being spawned yet.",
        attacker->GetCharacterID(),
        systemID,
        systemName
    );

    // NOTE:
    // This is intentionally a no-op for now. Once we are ready to implement
    // a Concord NPC that matches this codebase's NPC/NPCAI/Spawn patterns,
    // we will flesh this out in small, tested steps.
}

} // namespace ConcordSpawner

