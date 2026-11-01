#include <aurora/adaptor/AnimationSkeletonSource.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <framework/asset/AssetManager.h>

namespace sky::aurora {
    namespace {
        class RequestOperation final : public IAnimationSourceRequest<Skeleton> {
        public:
            explicit RequestOperation(const Uuid &id) : assetId(id)
            {
                if (assetId) {
                    asset = AssetManager::Get()->LoadAsset<Skeleton>(assetId, AssetLoadPolicy::ProductsOnly);
                }
            }
            AnimationSourceResult<Skeleton> Poll() override
            {
                AnimationSourceResult<Skeleton> result;
                result.assetLease    = asset;
                result.version.asset = assetId;
                if (!asset || asset->GetStatus() == AssetBase::Status::FAILED) {
                    result.state  = AnimationSourceState::Error;
                    result.status = {AnimationError::InvalidAsset,
                                     "Skeleton asset is missing, has the wrong type, or failed to load: " + assetId.ToString()};
                    return result;
                }
                if (!asset->IsLoaded()) {
                    return result;
                }
                result.version.generation = asset->GetInstanceId();
                result.version.revision   = asset->GetRevision();
                if (!resource) {
                    resource = FetchOrCreateSkeletonByAsset(asset, &result.status);
                }
                if (!resource) {
                    result.state = AnimationSourceState::Error;
                    if (result.status) {
                        result.status = {AnimationError::InvalidAsset, "Skeleton resource validation failed"};
                    }
                    return result;
                }
                result.resource       = resource;
                result.version.layout = result.resource->GetLayoutSignature();
                result.state          = AnimationSourceState::Ready;
                return result;
            }

        private:
            Uuid                             assetId;
            std::shared_ptr<Asset<Skeleton>> asset;
            CounterPtr<Skeleton>             resource;
        };
        class Source final : public IAnimationSkeletonSource {
        public:
            uint64_t GetChangeSerial() const override
            {
                return AssetManager::Get()->GetProductChangeSerial();
            }
            AnimationSourceRequest<Skeleton> Request(const Uuid &id) override
            {
                return AnimationSourceRequest<Skeleton>{std::make_shared<RequestOperation>(id)};
            }
        };
    } // namespace
    std::unique_ptr<IAnimationSkeletonSource> CreateAnimationSkeletonSource()
    {
        return std::make_unique<Source>();
    }
} // namespace sky::aurora
