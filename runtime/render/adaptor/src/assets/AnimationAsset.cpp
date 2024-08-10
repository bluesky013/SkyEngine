//
// Created by blues on 2024/8/2.
//

#include <render/adaptor/assets/AnimationAsset.h>

namespace sky {

    void AnimationAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(name);
        uint32_t size = 0;
        archive.LoadValue(size);
        nodeChannels.resize(size);

        for (uint32_t i = 0; i < size; ++i) {
            auto &channel = nodeChannels[i];
            archive.LoadValue(channel.name);
            uint32_t channelSize = 0;
            archive.LoadValue(channelSize);
            channel.position.time.resize(channelSize / sizeof(float));
            archive.LoadValue(reinterpret_cast<char*>(channel.position.time.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.position.keys.resize(channelSize / sizeof(Vector3));
            archive.LoadValue(reinterpret_cast<char*>(channel.position.keys.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.scale.time.resize(channelSize / sizeof(float));
            archive.LoadValue(reinterpret_cast<char*>(channel.scale.time.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.scale.keys.resize(channelSize / sizeof(Vector3));
            archive.LoadValue(reinterpret_cast<char*>(channel.scale.keys.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.rotation.time.resize(channelSize / sizeof(float));
            archive.LoadValue(reinterpret_cast<char*>(channel.position.time.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.rotation.keys.resize(channelSize / sizeof(Quaternion));
            archive.LoadValue(reinterpret_cast<char*>(channel.rotation.keys.data()), channelSize);
        }
    }

    void AnimationAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(name);
        archive.SaveValue(static_cast<uint32_t>(nodeChannels.size()));
        for (const auto &channel : nodeChannels) {
            archive.SaveValue(channel.name);
            auto channelSize = static_cast<uint32_t>(channel.position.time.size() * sizeof(float));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.position.time.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.position.keys.size() * sizeof(Vector3));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.position.keys.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.scale.time.size() * sizeof(float));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.scale.time.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.scale.keys.size() * sizeof(Vector3));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.scale.keys.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.rotation.time.size() * sizeof(float));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.rotation.time.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.rotation.keys.size() * sizeof(Quaternion));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.rotation.keys.data()), channelSize);
        }
    }

    AnimClipPtr CreateAnimationFromAsset(const AnimationAssetPtr &asset)
    {
        const auto &data = asset->Data();

        auto *clip = new AnimationClip(data.name);

        for (const auto &nodeChannel : data.nodeChannels) {
            clip->AddChannel(new AnimationNodeChannel(nodeChannel));
        }
        return clip;
    }

} // namespace sky