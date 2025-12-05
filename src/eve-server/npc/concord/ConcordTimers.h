#include "eve-server.h"
#include "npc/concord/ConcordLog.h"
#include "npc/concord/CrimeWatch.h"

namespace ConcordV2
{

// Simple helper to decide how fast Concord should respond.
// Later we can drive this by security status & crime type.
class ConcordTimers
{
public:
    static ConcordTimers& Instance();

    // Returns response delay in seconds.
    double GetResponseDelay(const CrimeEvent& event) const;

private:
    ConcordTimers() = default;
};

} // namespace ConcordV2

