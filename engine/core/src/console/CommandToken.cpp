//
// Created by blues on 2025/5/25.
//

#include <core/console/CommandToken.h>

namespace sky {

    std::vector<std::string> TokenizeCommand(std::string_view input)
    {
        std::vector<std::string> tokens;
        size_t i = 0;
        const size_t len = input.size();

        while (i < len) {
            // skip whitespace
            while (i < len && (input[i] == ' ' || input[i] == '\t')) {
                ++i;
            }
            if (i >= len) {
                break;
            }

            if (input[i] == '"') {
                // quoted token
                ++i;
                std::string token;
                while (i < len && input[i] != '"') {
                    token += input[i];
                    ++i;
                }
                if (i < len) {
                    ++i; // skip closing quote
                }
                tokens.emplace_back(std::move(token));
            } else {
                // unquoted token
                size_t start = i;
                while (i < len && input[i] != ' ' && input[i] != '\t') {
                    ++i;
                }
                tokens.emplace_back(input.substr(start, i - start));
            }
        }

        return tokens;
    }

} // namespace sky
