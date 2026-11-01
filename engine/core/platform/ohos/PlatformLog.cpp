#include "logger/PlatformLog.h"

#include <cstring>
#include <hilog/log.h>

namespace sky {

    void WritePlatformLog(FILE *stream, const char *tag, const char *type, const char *message)
    {
        const auto level  = std::strcmp(type, "ERROR") == 0 ? LOG_ERROR : std::strcmp(type, "WARNING") == 0 ? LOG_WARN : LOG_INFO;
        const int  result = OH_LOG_Print(LOG_APP, level, 0, tag, "%{public}s", message);
        if (stream != nullptr || result < 0) {
            fprintf(stream != nullptr ? stream : stderr, "[%s] [%s] : %s\n", tag, type, message);
        }
    }

} // namespace sky
