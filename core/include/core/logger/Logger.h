//
// Created by Zach Lee on 2021/11/7.
//
#pragma once

#define ENABLE_FILE_LINE

namespace sky {

    class Logger {
    public:
        static void Print(const char* tag, const char* type, const char* tmp, ...);
    };

}

#define LOG_E(tag, fmt, ...) LOG(tag, "ERROR",   fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) LOG(tag, "WARNING", fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) LOG(tag, "INFO",    fmt, ##__VA_ARGS__)