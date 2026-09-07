#pragma once

#include "config.h"
#include "diag.h"
#include "schema.h"
#include "strbuf.h"

/// Emits the generated header and source for a checked schema. basename is
/// the include name referenced from the generated source ("<basename>.h").
/// Fails when a cap override path does not address anything in the schema or
/// a cap cannot be represented.
[[nodiscard]] bool codegen_generate(const Schema *schema, const Config *cfg, const char *basename,
                                    StrBuf *header, StrBuf *source, Diag *diag);
