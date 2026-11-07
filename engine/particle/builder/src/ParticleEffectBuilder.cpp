#include <particle/builder/ParticleEffectBuilder.h>

#include <core/hash/Fnv1a.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <particle/assets/ParticleEffectAsset.h>
#include <particle/assets/ParticleEffectSource.h>

#include <algorithm>

namespace sky::particle {
    class ParticleEffectBuilder final : public AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> extensions{".particle"};
            return extensions;
        }
        std::string_view QueryType(const std::string &) const override
        {
            return AssetTraits<ParticleEffectAsset>::ASSET_TYPE;
        }
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override
        {
            std::string text;
            if (!request.file || !request.file->ReadString(text)) {
                result.error = "Particle source cannot be read";
                return;
            }
            ParticleEffectDesc effect;
            std::string        error;
            if (!ParseParticleSource(text, effect, error)) {
                result.error = std::move(error);
                return;
            }
            const auto compiled = CompileParticleEffect(effect);
            if (!compiled.succeeded) {
                result.error = compiled.diagnostics.empty() ? "Particle compilation failed" : compiled.diagnostics.front().message;
                return;
            }
            auto asset = CreateProduct<ParticleEffectAsset>(request, result);
            if (!asset)
                return;
            asset->Data().sourceHash = Fnv1a64(text);
            asset->Data().programs   = compiled.programs;
            asset->Data().valid      = true;
            std::vector<Uuid> textures;
            for (const auto &emitter : effect.emitters) {
                for (const auto &renderer : emitter.renderers) {
                    if (renderer.texture && std::find(textures.begin(), textures.end(), renderer.texture) == textures.end())
                        textures.push_back(renderer.texture);
                }
            }
            for (const auto &texture : textures)
                AddAssetDependency(asset, request, texture);
            FinishProduct(asset, request, result);
        }
    };

    void RegisterParticleBuilder()
    {
        AssetBuilderManager::Get()->RegisterBuilder(new ParticleEffectBuilder());
    }
} // namespace sky::particle
