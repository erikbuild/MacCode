#!/bin/bash
# Launch the MacCode relay proxy. With no flags it serves the directory you run it from
# as the Claude project, on port 4242. Any flags (--project/--port/--model/--echo) pass
# through and override the defaults. Start this BEFORE launching MacCode in the emulator.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INVOKE_DIR="$(pwd)"

# First run after a clone needs the proxy's node modules.
if [ ! -d "$ROOT/proxy/node_modules" ]; then
    echo "Installing proxy dependencies (first run)…" >&2
    npm --prefix "$ROOT/proxy" install
fi

# Default the Claude project to the directory you ran this from, unless --project was given.
# (The proxy already defaults --port to 4242.) npm runs the start script with cwd set to the
# proxy package, so we must pass --project explicitly to point Claude at INVOKE_DIR.
project_given=0
for a in "$@"; do [ "$a" = "--project" ] && project_given=1; done

echo "Starting the MacCode proxy — leave it running, then launch MacCode in the emulator." >&2

if [ "$project_given" -eq 0 ]; then
    exec npm --prefix "$ROOT/proxy" --silent start -- --project "$INVOKE_DIR" "$@"
else
    exec npm --prefix "$ROOT/proxy" --silent start -- "$@"
fi
