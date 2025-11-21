#!/usr/bin/env bash
set -euo pipefail

# Always work from repo root
cd "$(git rev-parse --show-toplevel)"

echo "Removing backup/script files..."

rm -rf \
  apply_notepad_fix.sh \
  rollback_backups.sh \
  script_fix.sh \
  build.err \
  backup_20*/ \
  rescue_20*/ \
  src/eve-server/character/CharMgrService.cpp.bak.* \
  src/eve-server/character/CharMgrService.cpp.undo.* \
  src/eve-server/character/CharMgrService.h.bak.* \
  src/eve-server/character/CharMgrService.h.undo.* \
  src/eve-server/character/CharacterDB.cpp.bak.* \
  src/eve-server/character/CharacterDB.cpp.fixed \
  src/eve-server/character/CharacterDB.cpp.undo.* \
  src/eve-server/character/CharacterDB.h.bak.* \
  src/eve-server/character/CharacterDB.h.undo.*

echo "Done. Now run: git status"
