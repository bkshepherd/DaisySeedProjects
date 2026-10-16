#!/usr/bin/env bash
# Starts the effect selection tool (tools/effects.py). Arguments are passed on to it.
cd "$(dirname "$0")" || exit 1
exec python3 tools/effects.py "$@"
