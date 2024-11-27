//
// Created by blues on 2024/12/1.
//

#include <core/util/Time.h>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace sky {

    std::string GetCurrentTimeString()
    {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d%H%M%S");
        return oss.str();
    }

} // namespace sky
