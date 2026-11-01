#pragma once

#include <cstdio>

namespace sky {

    void WritePlatformLog(FILE *stream, const char *tag, const char *type, const char *message);

} // namespace sky
