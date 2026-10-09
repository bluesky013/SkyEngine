//
// Content builder: cooks `.clip` / `.graph` animation sources into their
// asset data. Owned by the content adaptor, not by any renderer.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::builder {

    class AnimationBuilder : public AssetBuilder {
    public:
        AnimationBuilder()           = default;
        ~AnimationBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &ext) const override;
        void             Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        void RequestClip(const AssetBuildRequest &request, AssetBuildResult &result);
        void RequestGraph(const AssetBuildRequest &request, AssetBuildResult &result);

        std::vector<std::string> extensions = {".clip", ".graph"};
    };

} // namespace sky::builder
