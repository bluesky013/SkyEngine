//
// Created by Zach Lee on 2023/4/2.
//

#include <core/util/String.h>

namespace sky {

    std::vector<std::string> Split(const std::string &s, const char *separator)
    {
        std::vector<std::string> output;
        std::string::size_type prev = 0, pos = 0;
        while ((pos = s.find_first_of(std::string(separator), pos)) != std::string::npos) {
            if (pos > prev) {
                std::string substring(s.substr(prev, pos - prev));
                output.push_back(substring);
            }
            prev = ++pos;
        }

        if (prev <= s.length()) {
            output.push_back(s.substr(prev, pos - prev));
        }
        return output;
    }

}