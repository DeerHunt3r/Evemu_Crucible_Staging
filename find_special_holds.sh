#!/usr/bin/env bash
set -euo pipefail

# Search from repo root (or current directory)
ROOT="${1:-.}"

# Where to write results
OUT="${2:-special_hold_report_$(date +%Y%m%d_%H%M%S).txt}"

# Adjust these if your repo uses different include paths/extensions.
INCLUDE_GLOBS=(
  "--glob=*.h"
  "--glob=*.hpp"
  "--glob=*.c"
  "--glob=*.cc"
  "--glob=*.cpp"
  "--glob=*.inl"
  "--glob=*.py"
  "--glob=*.sql"
  "--glob=*.xml"
  "--glob=*.xmlp"
)

# High-signal patterns for "special holds" behavior in EvEmu / Crucible-era logic.
PATTERNS=(
  # Flags / holds / inventory routing
  "flagOreHold"
  "OreHold"
  "ore hold"
  "Special.*Hold"
  "holdFlag"
  "flagCargoHold"
  "flag.*Hold"
  "GetOreHold"
  "Get.*HoldFlag"
  "CargoFull"
  "IsFull"
  "AddItem"
  "MoveItem"
  "PutItem"
  "AddToHold"
  "AddToCargo"

  # Attributes (common naming styles)
  "Attr.*Hold.*Capacity"
  "HoldCapacity"
  "Special.*Hold.*Capacity"
  "OreHoldCapacity"
  "MiningHold"
  "Mining.*Hold"
  "SalvageHoldCapacity"
  "ShipMaintenanceBay"
  "FleetHangar"
  "AmmoHold"
  "DroneBay"

  # Data tables / constants often involved
  "invFlags"
  "invGroups"
  "invTypes"
  "invVolumes"
  "attributeID"
  "dogmaAttributes"
  "dgmAttributeTypes"
  "dgmTypeAttributes"
)

{
  echo "=== Specialized Hold Search Report ==="
  echo "Root: $ROOT"
  echo "Generated: $(date)"
  echo

  if ! command -v rg >/dev/null 2>&1; then
    echo "ERROR: ripgrep (rg) not found. Install it and re-run."
    exit 1
  fi

  echo "== Top-level quick hits (case-insensitive) =="
  rg -n -S -i "${INCLUDE_GLOBS[@]}" \
    "orehold|ore hold|special.*hold|holdcapacity|attr.*hold.*capacity|flagorehold|get.*holdflag|mininghold|fleet hangar|drone bay|ammo hold" \
    "$ROOT" || true
  echo

  echo "== Pattern-by-pattern results =="
  for p in "${PATTERNS[@]}"; do
    echo
    echo "---- PATTERN: $p ----"
    rg -n -S "${INCLUDE_GLOBS[@]}" "$p" "$ROOT" || true
  done

  echo
  echo "== Candidate 'routing' code (where items are moved into holds) =="
  rg -n -S "${INCLUDE_GLOBS[@]}" \
    "flagCargoHold|flagOreHold|holdFlag|Move\(|MoveItem|AddItem|PutItem|CargoFull|IsFull" \
    "$ROOT" || true

  echo
  echo "== Candidate 'attribute lookup' code (capacity decisions) =="
  rg -n -S "${INCLUDE_GLOBS[@]}" \
    "GetAttribute\(|Attr.*Hold.*Capacity|HoldCapacity|Capacity" \
    "$ROOT" || true

  echo
  echo "=== End Report ==="
} | tee "$OUT"

echo
echo "Wrote report to: $OUT"

