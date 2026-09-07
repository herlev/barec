#pragma once

#include "backend/config.h"
#include "util/strbuf.h"
#include "util/types.h"

/// Splits name into words on underscores and case boundaries (orderId ->
/// order id, HTTPServer -> http server, FOO_BAR -> foo bar) and appends
/// them to out re-rendered in the given style.
void name_render(Str name, CaseStyle style, StrBuf *out);
