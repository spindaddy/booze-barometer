#!/bin/zsh
# Browser-based serial monitor (no Tkinter required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pick_python() {
  local c
  for c in /usr/bin/python3 /opt/homebrew/bin/python3 /usr/local/bin/python3 python3; do
    if command -v "$c" >/dev/null 2>&1 && "$c" -c 'import sys; assert sys.version_info >= (3, 8)' 2>/dev/null; then
      echo "$c"
      return 0
    fi
  done
  return 1
}

PY="$(pick_python)" || {
  echo "No suitable python3 found." >&2
  exit 1
}

"$PY" -c 'import serial' 2>/dev/null || "$PY" -m pip install --user pyserial
exec "$PY" "$ROOT/scripts/monitor.py" "$@"
