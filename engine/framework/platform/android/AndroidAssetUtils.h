//
// Created on 2026/10/15.
//

#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sky {

    // APK assets use logical relative paths, independent of the host path separator.
    inline bool NormalizeAndroidAssetPath(std::string_view path, std::string &out)
    {
        if (path.find('\0') != std::string_view::npos) {
            out.clear();
            return false;
        }
        std::vector<std::string_view> segments;
        size_t                        offset = 0;
        while (offset < path.size()) {
            const size_t end     = path.find_first_of("/\\", offset);
            const auto   segment = path.substr(offset, (end == std::string_view::npos ? path.size() : end) - offset);
            if (segment == "..") {
                if (segments.empty()) {
                    out.clear();
                    return false;
                }
                segments.pop_back();
            } else if (!segment.empty() && segment != ".") {
                segments.push_back(segment);
            }
            if (end == std::string_view::npos) {
                break;
            }
            offset = end + 1;
        }
        std::string normalized;
        for (const auto segment : segments) {
            if (!normalized.empty()) {
                normalized += '/';
            }
            normalized.append(segment.data(), segment.size());
        }
        out = std::move(normalized);
        return true;
    }

    // AAsset_read returns int and may make partial progress, including for compressed assets.
    template <typename Reader>
    bool ReadAndroidAssetBytes(Reader &&reader, char *out, size_t size)
    {
        if (size != 0 && out == nullptr) {
            return false;
        }
        size_t offset = 0;
        while (offset != size) {
            const size_t chunk = std::min(size - offset, size_t{1024 * 1024});
            const int    count = reader(out + offset, chunk);
            if (count <= 0 || static_cast<size_t>(count) > chunk) {
                return false;
            }
            offset += static_cast<size_t>(count);
        }
        return true;
    }

} // namespace sky
