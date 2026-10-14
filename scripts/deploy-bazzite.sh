#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOST=${IA_SSH_HOST:-bazzite}
cd "$ROOT"
rsync -az scripts/launch.sh "$HOST:Games/infra-arcana-zh/game/launch.sh"
rsync -az artwork/ "$HOST:Games/infra-arcana-zh/artwork/"
ssh "$HOST" 'set -eu; ROOT="$HOME/Games/infra-arcana-zh"; cp "$ROOT/build/ia" "$ROOT/game/ia.next"; chmod +x "$ROOT/game/ia.next"; mv "$ROOT/game/ia.next" "$ROOT/game/ia"; rsync -a --exclude=user_data.ini "$ROOT/source/installed_files/" "$ROOT/game/"'
