//
// Created by blues on 2024/6/2.
//

#pragma once

#include <core/platform/Platform.h>

namespace sky::py {

    // Attach the host environment before the first PythonInit() so the plugin shares the host's singletons.
    SKY_EXPORT void PythonAttachEnvironment(void *environment);

    SKY_EXPORT void PythonDetachEnvironment();

    SKY_EXPORT bool PythonInit();
    SKY_EXPORT void PythonShutdown();
    SKY_EXPORT bool PythonRunString(const char *source);
    SKY_EXPORT bool PythonRunFile(const char *path);

} // namespace sky::py
