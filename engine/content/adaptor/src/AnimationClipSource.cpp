#include <content/adaptor/AnimationClipSource.h>
#include <content/adaptor/assets/AnimationAsset.h>
#include <framework/asset/AssetManager.h>

namespace sky::content {
    namespace {
        class RequestOperation final : public IAnimationSourceRequest<AnimationClip> {
        public:
            explicit RequestOperation(const Uuid &id) : assetId(id)
            {
                if (assetId) {
                    asset = AssetManager::Get()->LoadAsset<AnimationClip>(assetId, AssetLoadPolicy::ProductsOnly);
                }
            }
            AnimationSourceResult<AnimationClip> Poll() override
            {
                AnimationSourceResult<AnimationClip> result;
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
            AnimationSourceRequest<AnimationClip> Request(const Uuid &id) override
            {
                return AnimationSourceRequest<AnimationClip>{std::make_shared<RequestOperation>(id)};
            }
        };
    } // namespace
    std::unique_ptr<IAnimationClipSource> CreateAnimationClipSource()
    {
        return std::make_unique<Source>();
    }
} // namespace sky::content
