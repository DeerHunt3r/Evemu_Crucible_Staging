#include "eve-server.h"
#include "npc/concord/ConcordTimers.h"
#include "npc/concord/ConcordLog.h"

namespace ConcordV2
{

ConcordTimers& ConcordTimers::Instance()
{
    static ConcordTimers s_instance;
    return s_instance;
}

double ConcordTimers::GetResponseDelay(const CrimeEvent& event) const
{
    const double sec = event.systemSecurity;

    // Below 0.5 we currently do not spawn CONCORD (reserved for Faction Police, etc.).
    if (sec < 0.5)
    {
        CONCORD_LOG_DEBUG(
            "ConcordTimers::GetResponseDelay: system %u (sec=%.2f) below 0.5, no CONCORD response.",
            event.solarSystemID, sec
        );
        return -1.0;
    }

    double delaySec = 0.0;

    // Simple Crucible-style banding:
    //  1.0–0.9: ~2s
    //  0.9–0.8: ~3s
    //  0.8–0.7: ~4s
    //  0.7–0.6: ~5s
    //  0.6–0.5: ~6s
    if (sec >= 0.9)
        delaySec = 2.0;
    else if (sec >= 0.8)
        delaySec = 3.0;
    else if (sec >= 0.7)
        delaySec = 4.0;
    else if (sec >= 0.6)
        delaySec = 5.0;
    else    // 0.5 <= sec < 0.6
        delaySec = 6.0;

    CONCORD_LOG_INFO(
        "ConcordTimers::GetResponseDelay: system %u (sec=%.2f) -> response delay = %.2f sec.",
        event.solarSystemID, sec, delaySec
    );

    return delaySec;
}

} // namespace ConcordV2

