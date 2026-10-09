//
// Animation asset load/save + runtime build (see AnimationAsset.h).
//

#include <content/adaptor/assets/AnimationAsset.h>

#include <cstring>
#include <utility>

namespace sky {

    void AnimationClipAssetData::clear()
    {
        name.clear();
        frameRate = 0.f;
        skeleton  = Uuid{};
        nodeChannels.clear();
    }

    void AnimationClipAssetData::Load(BinaryInputArchive &ar)
    {
        ar.LoadValue(version);
        if (version != CURRENT_VERSION) {
            clear();
            return;
        }
        ar.LoadValue(name);
        ar.LoadValue(frameRate);
        ar.LoadValue(skeleton);

        uint32_t count = 0;
        ar.LoadValue(count);
        nodeChannels.resize(count);
        for (auto &channel : nodeChannels) {
            ar.LoadValue(channel.name);

            uint32_t bytes = 0;
            ar.LoadValue(bytes);
            channel.position.times.resize(bytes / sizeof(AnimTimeKey));
            if (bytes != 0) {
                ar.LoadValue(reinterpret_cast<char *>(channel.position.times.data()), bytes);
            }
            ar.LoadValue(bytes);
            channel.position.keys.resize(bytes / sizeof(Vector3));
            if (bytes != 0) {
                ar.LoadValue(reinterpret_cast<char *>(channel.position.keys.data()), bytes);
            }
            ar.LoadValue(bytes);
            channel.scale.times.resize(bytes / sizeof(AnimTimeKey));
            if (bytes != 0) {
                ar.LoadValue(reinterpret_cast<char *>(channel.scale.times.data()), bytes);
            }
            ar.LoadValue(bytes);
            channel.scale.keys.resize(bytes / sizeof(Vector3));
            if (bytes != 0) {
                ar.LoadValue(reinterpret_cast<char *>(channel.scale.keys.data()), bytes);
            }
            ar.LoadValue(bytes);
            channel.rotation.times.resize(bytes / sizeof(AnimTimeKey));
            if (bytes != 0) {
                ar.LoadValue(reinterpret_cast<char *>(channel.rotation.times.data()), bytes);
            }
            ar.LoadValue(bytes);
            channel.rotation.keys.resize(bytes / sizeof(Quaternion));
            if (bytes != 0) {
                ar.LoadValue(reinterpret_cast<char *>(channel.rotation.keys.data()), bytes);
            }
        }
    }

    void AnimationClipAssetData::Save(BinaryOutputArchive &ar) const
    {
        ar.SaveValue(version);
        ar.SaveValue(name);
        ar.SaveValue(frameRate);
        ar.SaveValue(skeleton);

        ar.SaveValue(static_cast<uint32_t>(nodeChannels.size()));
        for (const auto &channel : nodeChannels) {
            ar.SaveValue(channel.name);
            auto bytes = static_cast<uint32_t>(channel.position.times.size() * sizeof(AnimTimeKey));
            ar.SaveValue(bytes);
            ar.SaveValue(reinterpret_cast<const char *>(channel.position.times.data()), bytes);
            bytes = static_cast<uint32_t>(channel.position.keys.size() * sizeof(Vector3));
            ar.SaveValue(bytes);
            ar.SaveValue(reinterpret_cast<const char *>(channel.position.keys.data()), bytes);
            bytes = static_cast<uint32_t>(channel.scale.times.size() * sizeof(AnimTimeKey));
            ar.SaveValue(bytes);
            ar.SaveValue(reinterpret_cast<const char *>(channel.scale.times.data()), bytes);
            bytes = static_cast<uint32_t>(channel.scale.keys.size() * sizeof(Vector3));
            ar.SaveValue(bytes);
            ar.SaveValue(reinterpret_cast<const char *>(channel.scale.keys.data()), bytes);
            bytes = static_cast<uint32_t>(channel.rotation.times.size() * sizeof(AnimTimeKey));
            ar.SaveValue(bytes);
            ar.SaveValue(reinterpret_cast<const char *>(channel.rotation.times.data()), bytes);
            bytes = static_cast<uint32_t>(channel.rotation.keys.size() * sizeof(Quaternion));
            ar.SaveValue(bytes);
            ar.SaveValue(reinterpret_cast<const char *>(channel.rotation.keys.data()), bytes);
        }
    }

    AnimClipPtr CreateAnimationClipFromAsset(const AnimationClipAssetPtr &asset)
    {
        if (asset == nullptr) {
            return {};
        }
        const auto &data = asset->Data();

        auto  *clip     = new AnimationClip(Name(data.name.c_str()));
        size_t numFrame = 0;
        for (const auto &nodeChannel : data.nodeChannels) {
            clip->AddChannel(new AnimationNodeChannel(nodeChannel));
            numFrame = std::max(nodeChannel.position.keys.empty() ? 0 : nodeChannel.position.keys.size(), numFrame);
            numFrame = std::max(nodeChannel.scale.keys.empty() ? 0 : nodeChannel.scale.keys.size(), numFrame);
            numFrame = std::max(nodeChannel.rotation.keys.empty() ? 0 : nodeChannel.rotation.keys.size(), numFrame);
        }
        clip->SetFrameRate(data.frameRate);
        clip->SetNumFrame(static_cast<uint32_t>(numFrame));
        return clip;
    }

    void AnimationAssetData::clear()
    {
    }

    void AnimationAssetData::Load(BinaryInputArchive &ar)
    {
        ar.LoadValue(version);
        if (version != CURRENT_VERSION) {
            clear();
        }
    }

    void AnimationAssetData::Save(BinaryOutputArchive &ar) const
    {
        ar.SaveValue(version);
    }

} // namespace sky
