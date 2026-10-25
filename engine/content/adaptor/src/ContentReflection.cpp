//
// Content adaptor reflection (see ContentReflection.h).
//

#include <content/adaptor/ContentReflection.h>

#include <content/adaptor/assets/AnimationAsset.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    void ContentReflection(SerializationContext *context)
    {
        context->Register<AnimationClipAssetData>("AnimationClipAssetData")
            .BinLoad<&AnimationClipAssetData::Load>()
            .BinSave<&AnimationClipAssetData::Save>();

        auto *manager = AssetManager::Get();
        manager->RegisterAssetHandler<AnimationClip>();
    }

} // namespace sky
