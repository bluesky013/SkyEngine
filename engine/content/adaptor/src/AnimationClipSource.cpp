#include <content/adaptor/AnimationClipSource.h>
#include <content/adaptor/assets/AnimationAsset.h>
#include <framework/asset/AssetManager.h>

namespace sky::content {
    namespace {
        class RequestOperation final : public IAnimationSourceRequest<AnimationClip> {
        public:
            RequestOperation(const Uuid &id, std::shared_ptr<const void> code) : moduleLease(std::move(code)), assetId(id)
            {
                if (assetId) {
                    asset = AssetManager::Get()->LoadAsset<AnimationClip>(assetId, AssetLoadPolicy::ProductsOnly);
                }
            }
            AnimationSourceResult<AnimationClip> Poll() override
            {
                AnimationSourceResult<AnimationClip> result;
                result.moduleLease   = moduleLease;
                result.assetLease    = asset;
                result.version.asset = assetId;
                if (!asset || asset->GetStatus() == AssetBase::Status::FAILED) {
                    result.state  = AnimationSourceState::Error;
                    result.status = {AnimationError::InvalidAsset,
                                     "AnimationClip asset is missing, has the wrong type, or failed to load: " + assetId.ToString()};
                    return result;
                }
                if (!asset->IsLoaded()) {
                    return result;
                }
                result.version.generation = asset->GetInstanceId();
                result.version.revision   = asset->GetRevision();
                if (!resource) {
                    resource = CreateAnimationClipFromAsset(asset, &result.status);
                }
                if (!resource) {
                    result.state = AnimationSourceState::Error;
                    if (result.status) {
                        result.status = {AnimationError::InvalidAsset, "AnimationClip resource validation failed"};
                    }
                    return result;
                }
                result.resource     = resource;
                result.skeletonHint = asset->Data().skeleton;
                result.state        = AnimationSourceState::Ready;
                return result;
            }

        private:
            std::shared_ptr<const void>           moduleLease;
            Uuid                                  assetId;
            std::shared_ptr<Asset<AnimationClip>> asset;
            CounterPtr<AnimationClip>             resource;
        };
        class Source final : public IAnimationClipSource {
        public:
            uint64_t GetChangeSerial() const override
            {
                return AssetManager::Get()->GetProductChangeSerial();
            }
            explicit Source(std::shared_ptr<const void> code) : moduleLease(std::move(code))
            {
            }
            AnimationSourceRequest<AnimationClip> Request(const Uuid &id) override
            {
                return {moduleLease, std::make_shared<RequestOperation>(id, moduleLease)};
            }

        private:
            std::shared_ptr<const void> moduleLease;
        };
    } // namespace
    std::unique_ptr<IAnimationClipSource> CreateAnimationClipSource(std::shared_ptr<const void> moduleLease)
    {
        return moduleLease ? std::make_unique<Source>(std::move(moduleLease)) : nullptr;
    }
} // namespace sky::content
