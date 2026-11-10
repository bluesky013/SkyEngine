#include <curve/builder/CurveBuilder.h>

#include <core/hash/Fnv1a.h>
#include <curve/assets/CurveAsset.h>
#include <curve/assets/CurveSource.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetBuilderManager.h>

namespace sky::curve {
    class CurveAssetBuilder final : public AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> extensions{".curve"};
            return extensions;
        }
        std::string_view QueryType(const std::string &) const override
        {
            return AssetTraits<CurveAsset>::ASSET_TYPE;
        }
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override
        {
            result.retCode = AssetBuildRetCode::FAILED;
            if (!request.file || !request.assetInfo) {
                result.error = "Curve source file or identity is missing";
                return;
            }
            const auto input = request.file->ReadAsArchive();
            if (!input) {
                result.error = "Curve source cannot be read";
                return;
            }
            auto &stream = input->GetStream();
            stream.seekg(0, std::ios::end);
            const auto length = stream.tellg();
            if (length <= 0 || length > static_cast<std::streamoff>(MAX_CURVE_SOURCE_BYTES)) {
                result.error = "Curve source size is invalid";
                return;
            }
            stream.seekg(0, std::ios::beg);
            std::string text(static_cast<size_t>(length), '\0');
            if (!stream || !input->LoadRaw(text.data(), text.size())) {
                result.error = "Curve source cannot be read completely";
                return;
            }
            Curve       curve;
            std::string error;
            if (!ParseCurveSource(text, curve, error)) {
                result.error = std::move(error);
                return;
            }
            auto asset = CreateProduct<CurveAsset>(request, result);
            if (!asset)
                return;
            asset->Data().sourceHash = Fnv1a64(text);
            asset->Data().curve      = std::move(curve);
            asset->Data().valid      = true;
            FinishProduct(asset, request, result);
        }
    };

    void RegisterCurveBuilder()
    {
        AssetBuilderManager::Get()->RegisterBuilder(new CurveAssetBuilder());
    }
} // namespace sky::curve
