#!/bin/sh
set -eu
ROOT="$HOME/Games/infra-arcana-zh"
mkdir -p "$ROOT/qa-game" "$ROOT/qa-data" "$ROOT/verification"
for p in "$ROOT"/game/*; do ln -sfn "$p" "$ROOT/qa-game/$(basename "$p")"; done
rm "$ROOT/qa-game/user_data.ini"
printf '[paths]\nuser_data=%s/qa-data\n' "$ROOT" > "$ROOT/qa-game/user_data.ini"
if ! systemctl --user is-active --quiet infra-arcana-qa-xvfb; then
 systemd-run --user --unit=infra-arcana-qa-xvfb --collect Xvfb :99 -screen 0 3840x2160x24 -nolisten tcp
fi
systemd-run --user --unit=infra-arcana-qa-game --collect --working-directory="$ROOT/qa-game" --setenv=DISPLAY=:99 --setenv=SDL_AUDIODRIVER=dummy --setenv=SDL_RENDER_DRIVER=software --setenv=IA_ZH_TRACE="$ROOT/verification/display-trace.txt" "$ROOT/qa-game/launch.sh"
