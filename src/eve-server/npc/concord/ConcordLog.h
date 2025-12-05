/******************************************************************************
 * EVE: Eve Online Emulator
 * Concord V2 - Logging helpers
 *****************************************************************************/

#ifndef EVE_NPC_CONCORD_LOG_H
#define EVE_NPC_CONCORD_LOG_H

#include "eve-server.h"

// We'll log using the NewLog system (sLog).
// Source tag "ConcordV2" makes it easy to grep in console/logs.
#define _CONCORD_LOG_CATEGORY "ConcordV2"

// Info-level / general messages
#define CONCORD_LOG_INFO(fmt, ...)   sLog.Log(_CONCORD_LOG_CATEGORY, fmt, ##__VA_ARGS__)

// Warnings and errors
#define CONCORD_LOG_WARN(fmt, ...)   sLog.Warning(_CONCORD_LOG_CATEGORY, fmt, ##__VA_ARGS__)
#define CONCORD_LOG_ERROR(fmt, ...)  sLog.Error(_CONCORD_LOG_CATEGORY, fmt, ##__VA_ARGS__)

// Debug output (NewLog::Debug already checks is_log_enabled(DEBUG__DEBUG))
#define CONCORD_LOG_DEBUG(fmt, ...)  sLog.Debug(_CONCORD_LOG_CATEGORY, fmt, ##__VA_ARGS__)

#endif // EVE_NPC_CONCORD_LOG_H

