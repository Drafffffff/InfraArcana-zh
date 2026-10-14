#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOST=${IA_SSH_HOST:-bazzite}
cd "$ROOT"
rsync -acz --no-times --omit-dir-times --exclude=.git --exclude=compile_commands.json source/ "$HOST:Games/infra-arcana-zh/source/"
ssh "$HOST" 'podman start infra-arcana-zh-builder >/dev/null; podman exec -w /work infra-arcana-zh-builder cmake -S source -B build; podman exec -w /work infra-arcana-zh-builder cmake --build build --target ia -j6'
