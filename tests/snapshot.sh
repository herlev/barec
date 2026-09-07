#!/bin/sh
set -eu
BAREC="$1"
DIR="$(dirname "$0")/snapshots"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

for conf in "$DIR"/*.conf; do
  name="$(basename "$conf" .conf)"
  "$BAREC" generate "$DIR/showcase.bare" -c "$conf" -o "$TMP/$name" -n showcase
  if [ "${UPDATE_SNAPSHOTS:-0}" = 1 ]; then
    cp "$TMP/$name/showcase.h" "$DIR/$name.h"
  else
    diff -u "$DIR/$name.h" "$TMP/$name/showcase.h"
  fi
done
