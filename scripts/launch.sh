#!/bin/sh
set -eu
GAME=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$GAME"
export LD_LIBRARY_PATH="$GAME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_RENDER_SCALE_QUALITY=0
# IA reads printable commands via SDL text events; do not open Steam OSK.
export SDL_ENABLE_SCREEN_KEYBOARD=0
exec "$GAME/ia" "$@"
