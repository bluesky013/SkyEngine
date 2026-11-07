#include "ScreenshotWriter.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace sky::editor {
    bool WriteScreenshotPng(const std::string &path, uint32_t width, uint32_t height, const std::vector<uint8_t> &rgba)
    {
        if (width == 0u || height == 0u || rgba.size() < static_cast<std::size_t>(width) * height * 4u) {
            return false;
        }
        return stbi_write_png(path.c_str(), int(width), int(height), 4, rgba.data(), int(width * 4u)) != 0;
    }
} // namespace sky::editor
