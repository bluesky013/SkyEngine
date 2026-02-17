//
// Created by blues on 2024/8/2.
//

#include <render/adaptor/assets/AnimationAsset.h>

namespace sky {

    void AnimationClipAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(name);
        archive.LoadValue(frameRate);
        archive.LoadValue(skeleton);

        uint32_t size = 0;
        archive.LoadValue(size);
        nodeChannels.resize(size);

        for (uint32_t i = 0; i < size; ++i) {
            auto &channel = nodeChannels[i];
            archive.LoadValue(channel.name);
            uint32_t channelSize = 0;
            archive.LoadValue(channelSize);
            channel.position.times.resize(channelSize / sizeof(AnimTimeKey));
            archive.LoadValue(reinterpret_cast<char*>(channel.position.times.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.position.keys.resize(channelSize / sizeof(Vector3));
            archive.LoadValue(reinterpret_cast<char*>(channel.position.keys.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.scale.times.resize(channelSize / sizeof(AnimTimeKey));
            archive.LoadValue(reinterpret_cast<char*>(channel.scale.times.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.scale.keys.resize(channelSize / sizeof(Vector3));
            archive.LoadValue(reinterpret_cast<char*>(channel.scale.keys.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.rotation.times.resize(channelSize / sizeof(AnimTimeKey));
            archive.LoadValue(reinterpret_cast<char*>(channel.rotation.times.data()), channelSize);

            archive.LoadValue(channelSize);
            channel.rotation.keys.resize(channelSize / sizeof(Quaternion));
            archive.LoadValue(reinterpret_cast<char*>(channel.rotation.keys.data()), channelSize);
        }
    }

    void AnimationClipAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(name);
        archive.SaveValue(frameRate);
        archive.SaveValue(skeleton);

        archive.SaveValue(static_cast<uint32_t>(nodeChannels.size()));
            for (const auto &channel : nodeChannels) {
            archive.SaveValue(channel.name);
            auto channelSize = static_cast<uint32_t>(channel.position.times.size() * sizeof(AnimTimeKey));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.position.times.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.position.keys.size() * sizeof(Vector3));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.position.keys.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.scale.times.size() * sizeof(AnimTimeKey));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.scale.times.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.scale.keys.size() * sizeof(Vector3));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.scale.keys.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.rotation.times.size() * sizeof(AnimTimeKey));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.rotation.times.data()), channelSize);

            channelSize = static_cast<uint32_t>(channel.rotation.keys.size() * sizeof(Quaternion));
            archive.SaveValue(channelSize);
            archive.SaveValue(reinterpret_cast<const char*>(channel.rotation.keys.data()), channelSize);
        }
    }

    AnimClipPtr CreateAnimationClipFromAsset(const AnimationClipAssetPtr &asset)
    {
        const auto &data = asset->Data();

        auto *clip = new AnimationClip(Name(data.name.c_str()), asset->GetUuid());
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

    void AnimationAssetData::LoadJson(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("Version", version);
    }

    void AnimationAssetData::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject("Version", version);
        ar.EndObject();
    }

} // namespace sky
