#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-.}"

echo "== A) Where OnGodmaShipEffect is defined (DogmaIM.xmlp) =="
grep -nR --line-number "Notify_OnGodmaShipEffect\|GodmaEnvironment" "$ROOT/src/eve-common/packets/DogmaIM.xmlp" || true
echo

echo "== B) Who encodes/sends OnGodmaShipEffect =="
grep -nR --line-number "Notify_OnGodmaShipEffect\|OnGodmaShipEffect" \
  "$ROOT/src/eve-server/ship/modules" \
  "$ROOT/src/eve-server/inventory" \
  "$ROOT/src/eve-server/system" \
  "$ROOT/src/eve-server/Client.cpp" 2>/dev/null || true
echo

echo "== C) How OnMultiEvent is sent (Client.cpp) =="
grep -nR --line-number "SendNotification(\"OnMultiEvent\"\|Notify_OnMultiEvent\|m_destinyEventQueue" \
  "$ROOT/src/eve-server/Client.cpp" 2>/dev/null || true
echo

echo "== D) ShowEffect entry point(s) (ActiveModule/GenericModule) =="
grep -nR --line-number "ShowEffect\(" \
  "$ROOT/src/eve-server/ship/modules/ActiveModule.cpp" \
  "$ROOT/src/eve-server/ship/modules/GenericModule.cpp" 2>/dev/null || true
echo

echo "DONE"
