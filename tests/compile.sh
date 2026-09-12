#!/bin/sh
set -eu
BAREC="$1"
RUNTIME="$2"
DIR="$(dirname "$0")/compile"
CC="${CC:-cc}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

failed=0
for schema in "$DIR"/*.bare; do
  [ -e "$schema" ] || continue
  name="$(basename "$schema" .bare)"
  out="$TMP/$name"
  conf="$DIR/$name.conf"
  set -- generate "$schema" -o "$out" -n "$name"
  if [ -f "$conf" ]; then
    set -- "$@" -c "$conf"
  fi
  if ! "$BAREC" "$@" > "$TMP/log" 2>&1 ||
    ! "$CC" -std=gnu23 -Wall -Wextra -Werror -I "$RUNTIME" -c "$out/$name.c" -o "$out/$name.o" >> "$TMP/log" 2>&1; then
    echo "FAIL $name"
    cat "$TMP/log"
    failed=1
  fi
done

printf 'type Msg struct { label: str }\n' > "$TMP/lit.bare"
"$BAREC" generate "$TMP/lit.bare" -o "$TMP/lit" -n lit
FITS="0123456789012345678901234567890123456789012345678901234567890123"
cat > "$TMP/lit/ok.c" << EOF
#include "lit.h"
Msg fits(void) {
  Msg m = {.label = BARE_STR64("$FITS")};
  BARE_STR_LIT(&m.label, "$FITS");
  return m;
}
EOF
for std in gnu23 gnu99; do
  if ! "$CC" -std="$std" -Wall -Wextra -Werror -I "$RUNTIME" -I "$TMP/lit" -c "$TMP/lit/ok.c" \
    -o "$TMP/lit/ok.o" > "$TMP/log" 2>&1; then
    echo "FAIL str_lit_fits ($std)"
    cat "$TMP/log"
    failed=1
  fi
done
cat > "$TMP/lit/over_init.c" << EOF
#include "lit.h"
Msg over(void) {
  Msg m = {.label = BARE_STR64("x$FITS")};
  return m;
}
EOF
cat > "$TMP/lit/over_lit.c" << EOF
#include "lit.h"
Msg over(void) {
  Msg m = {0};
  BARE_STR_LIT(&m.label, "x$FITS");
  return m;
}
EOF
for std in gnu23 gnu99; do
  for name in over_init over_lit; do
    if "$CC" -std="$std" -I "$RUNTIME" -I "$TMP/lit" -c "$TMP/lit/$name.c" \
      -o "$TMP/lit/$name.o" > /dev/null 2>&1; then
      echo "FAIL $name ($std): over-long literal accepted"
      failed=1
    fi
  done
done
exit "$failed"
