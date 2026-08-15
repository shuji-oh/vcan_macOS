#!/bin/sh
set -eu
root=$(mktemp -d /tmp/macos-vcan-test.XXXXXX)
cleanup() {
  kill "$daemon" 2>/dev/null || true
  wait "$daemon" 2>/dev/null || true
  rm -rf "$root"
}
trap cleanup EXIT INT TERM
./build/macos-vcand --socket-dir "$root" &
daemon=$!
i=0
while [ ! -S "$root/vcan0.sock" ]; do
  i=$((i + 1))
  [ "$i" -lt 50 ] || { echo 'daemon did not start' >&2; exit 1; }
  sleep 0.02
done
MACOS_VCAN_DIR="$root" ./build/vcan-dump >"$root/dump" &
dumper=$!
sleep 0.05
MACOS_VCAN_DIR="$root" ./build/vcan-send 123 DEADBEEF
i=0
while ! grep -q '^123 \[4\] DE AD BE EF$' "$root/dump" 2>/dev/null; do
  i=$((i + 1))
  [ "$i" -lt 50 ] || { cat "$root/dump" >&2; exit 1; }
  sleep 0.02
done
kill "$dumper" 2>/dev/null || true
wait "$dumper" 2>/dev/null || true
echo 'integration test passed'
