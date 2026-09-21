//
// Created on 2026/09/21.
//

#include <audio/AudioImport.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <string>

namespace sky::audio {

    bool IsSupportedSourceFormat(std::string_view path)
    {
        static constexpr std::array<std::string_view, 4> SUPPORTED = {".wav", ".mp3", ".flac", ".ogg"};

        const auto dot = path.find_last_of('.');
        if (dot == std::string_view::npos) {
            return false;
        }

        std::string ext(path.substr(dot));
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        return std::any_of(SUPPORTED.begin(), SUPPORTED.end(), [&ext](std::string_view candidate) {
            return ext == candidate;
        });
    }

} // namespace sky::audio
