#!/bin/bash
# Launch the MacCode relay proxy. With no flags it serves the directory you run it from
# as the Claude project, on port 4242. Any flags (--project/--port/--model/--echo) pass
# through and override the defaults. Start this BEFORE launching MacCode in the emulator.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INVOKE_DIR="$(pwd)"

has_flag() { local f="$1"; shift; for a in "$@"; do [ "$a" = "$f" ] && return 0; done; return 1; }

args=("$@")
has_flag --project "$@" || args=(--project "$INVOKE_DIR" "${args[@]}")
has_flag --port    "$@" || args=(--port 4242 "${args[@]}")

# First run after a clone needs the proxy's node modules.
if [ ! -d "$ROOT/proxy/node_modules" ]; then
    echo "Installing proxy dependencies (first run)…" >&2
    npm --prefix "$ROOT/proxy" install
fi

echo "Starting the MacCode proxy — leave it running, then launch MacCode in the emulator." >&2
exec npm --prefix "$ROOT/proxy" --silent start -- "${args[@]}"
