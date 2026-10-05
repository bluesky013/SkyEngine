//
// Created on 2026/10/05.
//

#include <editor/sandbox/UiIconBuilder.h>

#include <framework/asset/DerivedDataCache.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>

namespace sky {

    namespace {

        // Rasterizes an SVG icon into RGBA8 pixels; the DDC caches the result so it
        // is only produced once per (svg + size + version).
        class UiIconBuilder : public IDerivedDataBuilder {
        public:
            std::string GetId() const override { return "ui-icon-svg"; }
            uint32_t GetVersion() const override { return 1; }

            bool Build(const std::vector<uint8_t> &source, const std::string &settings,
                       std::vector<uint8_t> &out) const override
            {
                if (source.empty()) {
                    return false;
                }
                std::string svg(source.begin(), source.end());
                NSVGimage *image = nsvgParse(svg.data(), "px", 96.0f);
                if (image == nullptr) {
                    return false;
                }

                const int srcW = static_cast<int>(image->width + 0.5f);
                const int srcH = static_cast<int>(image->height + 0.5f);
                int dstW = srcW;
                int dstH = srcH;
                int parsedW = 0;
                int parsedH = 0;
                if (std::sscanf(settings.c_str(), "%dx%d", &parsedW, &parsedH) == 2 && parsedW > 0 && parsedH > 0) {
                    dstW = parsedW;
                    dstH = parsedH;
                }
                if (dstW <= 0 || dstH <= 0) {
                    nsvgDelete(image);
                    return false;
                }

                NSVGrasterizer *rast = nsvgCreateRasterizer();
                if (rast == nullptr) {
                    nsvgDelete(image);
                    return false;
                }
                out.assign(static_cast<size_t>(dstW) * dstH * 4, 0);
                const float scale = srcW > 0 ? static_cast<float>(dstW) / static_cast<float>(srcW) : 1.0f;
                nsvgRasterize(rast, image, 0, 0, scale, out.data(), dstW, dstH, dstW * 4);
                nsvgDeleteRasterizer(rast);
                nsvgDelete(image);
                return true;
            }
        };

    } // namespace

} // namespace sky

namespace sky::editor {

    void InstallUiIconBuilder()
    {
        sky::DerivedDataCache::Get().Register(std::make_shared<sky::UiIconBuilder>());
    }

} // namespace sky::editor
