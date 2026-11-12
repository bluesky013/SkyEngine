#pragma once

#include <framework/asset/AssetBuilder.h>

namespace sky::softbody {
    class SoftBodyBuilder final : public AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &extension) const override;
        void             Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        std::vector<std::string> extensions{".softbodyrig"};
    };
} // namespace sky::softbody
