//
// Created by blues on 2024/12/1.
//

#include <core/util/Time.h>
#include <core/platform/Platform.h>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace sky {

    std::string GetCurrentTimeString()
    {
        auto t = std::time(nullptr);
        std::tm tm = {};

#if SKY_PLATFORM_COMPILER_MSVC
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d%H%M%S");
        return oss.str();
    }

} // namespace sky
