#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sky::editor {
    // Writes a top-down RGBA8 readback as a PNG.
    bool WriteScreenshotPng(const std::string &path, uint32_t width, uint32_t height, const std::vector<uint8_t> &rgba);
} // namespace sky::editor
