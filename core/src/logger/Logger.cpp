//
// Created by Zach Lee on 2021/11/7.
//

#include "core/logger/Logger.h"
#include <string>
#include <stdarg.h>

namespace sky {

    void Logger::Print(const char* tag, const char* type, const char* fmt, ...)
    {
        const uint32_t MAX_SIZE = 1024;
        char buffer[MAX_SIZE];
        va_list params;
        va_start(params, fmt);
        vsnprintf(buffer, MAX_SIZE - 1, fmt, params);
        va_end(params);
        buffer[MAX_SIZE - 1] = '\0';

        printf("[%s] [%s] : %s\n", tag, type, buffer);
    }
}