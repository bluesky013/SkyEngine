//
// Created by Zach Lee on 2021/11/7.
//
#pragma once

#include <cstdint>

//#define ENABLE_FILE_LINE

namespace sky {

    class Logger {
    public:
        static void Print(const char *tag, const char *type, const char *tmp, ...);
    };

} // namespace sky

#define LOG_E(tag, fmt, ...) Logger::Print(tag, "ERROR", fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) Logger::Print(tag, "WARNING", fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) Logger::Print(tag, "INFO", fmt, ##__VA_ARGS__)