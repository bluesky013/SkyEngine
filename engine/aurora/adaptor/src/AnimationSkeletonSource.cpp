#include <aurora/adaptor/AnimationSkeletonSource.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <framework/asset/AssetManager.h>

namespace sky::aurora {
    namespace {
        class RequestOperation final : public IAnimationSourceRequest<Skeleton> {
        public:
            RequestOperation(const Uuid &id, std::shared_ptr<const void> code) : moduleLease(std::move(code)), assetId(id)
            {
                if (assetId) {
                    asset = AssetManager::Get()->LoadAsset<Skeleton>(assetId, AssetLoadPolicy::ProductsOnly);
                }
            }
            AnimationSourceResult<Skeleton> Poll() override
            {
                AnimationSourceResult<Skeleton> result;
                result.moduleLease   = moduleLease;
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
            std::shared_ptr<const void>      moduleLease;
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
            explicit Source(std::shared_ptr<const void> code) : moduleLease(std::move(code))
            {
            }
            AnimationSourceRequest<Skeleton> Request(const Uuid &id) override
            {
                return {moduleLease, std::make_shared<RequestOperation>(id, moduleLease)};
            }

        private:
            std::shared_ptr<const void> moduleLease;
        };
    } // namespace
    std::unique_ptr<IAnimationSkeletonSource> CreateAnimationSkeletonSource(std::shared_ptr<const void> moduleLease)
    {
        return moduleLease ? std::make_unique<Source>(std::move(moduleLease)) : nullptr;
    }
} // namespace sky::aurora
