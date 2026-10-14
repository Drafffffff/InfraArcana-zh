#!/bin/sh
set -eu
GAME=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$GAME"
export LD_LIBRARY_PATH="$GAME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_RENDER_SCALE_QUALITY=0
exec "$GAME/ia" "$@"
