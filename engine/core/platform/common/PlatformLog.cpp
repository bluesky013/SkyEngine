#include "logger/PlatformLog.h"

namespace sky {

    void WritePlatformLog(FILE *stream, const char *tag, const char *type, const char *message)
    {
        fprintf(stream != nullptr ? stream : stdout, "[%s] [%s] : %s\n", tag, type, message);
    }

} // namespace sky
