#!/bin/sh
set -eu
BAREC="$1"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

"$BAREC" --help | grep -q "^Usage:"
"$BAREC" -h > /dev/null
"$BAREC" --version | grep -q "^barec "
"$BAREC" -V > /dev/null

"$BAREC" 2>&1 >/dev/null | grep -q "^Usage:"
if "$BAREC" > /dev/null 2>&1; then exit 1; fi
if "$BAREC" --bogus 2> /dev/null; then exit 1; fi
if "$BAREC" generate 2> /dev/null; then exit 1; fi
if "$BAREC" generate a.bare b.bare 2> /dev/null; then exit 1; fi
if "$BAREC" generate a.bare --std c11 2> /dev/null; then exit 1; fi
if "$BAREC" config extra 2> /dev/null; then exit 1; fi

cat > "$TMP/msg.bare" << 'EOF'
type Point struct {
  x: f32
  y: f32
}
EOF

"$BAREC" check "$TMP/msg.bare"

cat > "$TMP/bad.bare" << 'EOF'
type Broken Missing
EOF
if "$BAREC" check "$TMP/bad.bare" 2> "$TMP/err"; then exit 1; fi
grep -q "bad.bare:1:13: error: unknown type 'Missing'" "$TMP/err"

"$BAREC" "$TMP/msg.bare"
test -f "$TMP/msg.h"
test -f "$TMP/msg.c"

"$BAREC" generate "$TMP/msg.bare" -o "$TMP/out/nested" -n point --std c99 --runtime
test -f "$TMP/out/nested/point.h"
test -f "$TMP/out/nested/point.c"
test -f "$TMP/out/nested/bare.h"
test -f "$TMP/out/nested/bare.c"
grep -q "BARE_NODISCARD" "$TMP/out/nested/point.h"
grep -q '#include "point.h"' "$TMP/out/nested/point.c"

"$BAREC" config > "$TMP/barec.conf"
mkdir "$TMP/proj"
cp "$TMP/msg.bare" "$TMP/proj/"
cp "$TMP/barec.conf" "$TMP/proj/"
"$BAREC" generate "$TMP/proj/msg.bare" 2> "$TMP/note"
grep -q "using config" "$TMP/note"
cmp "$TMP/msg.h" "$TMP/proj/msg.h"

"$BAREC" generate "$TMP/msg.bare" -o "$TMP/pfx" --prefix=acme
grep -q "AcmePoint" "$TMP/pfx/msg.h"
grep -q "acme_point_decode" "$TMP/pfx/msg.h"
