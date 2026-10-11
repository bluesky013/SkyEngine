//
// Animation asset load/save + runtime build (see AnimationAsset.h).
//

#include <content/adaptor/assets/AnimationAsset.h>

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <utility>

namespace sky {

    void AnimationClipAssetData::clear()
    {
        name.clear();
        frameRate = 0.f;
        skeleton  = Uuid{};
        channels.clear();
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
        channels.resize(count);
        for (auto &channel : channels) {
            ar.LoadValue(channel.name);
            ar.LoadValue(channel.positionTimes.offset);
            ar.LoadValue(channel.positionTimes.size);
            ar.LoadValue(channel.positionKeys.offset);
            ar.LoadValue(channel.positionKeys.size);
            ar.LoadValue(channel.scaleTimes.offset);
            ar.LoadValue(channel.scaleTimes.size);
            ar.LoadValue(channel.scaleKeys.offset);
            ar.LoadValue(channel.scaleKeys.size);
            ar.LoadValue(channel.rotationTimes.offset);
            ar.LoadValue(channel.rotationTimes.size);
            ar.LoadValue(channel.rotationKeys.offset);
            ar.LoadValue(channel.rotationKeys.size);
        }
    }

    void AnimationClipAssetData::Save(BinaryOutputArchive &ar) const
    {
        ar.SaveValue(version);
        ar.SaveValue(name);
        ar.SaveValue(frameRate);
        ar.SaveValue(skeleton);

        ar.SaveValue(static_cast<uint32_t>(channels.size()));
        for (const auto &channel : channels) {
            ar.SaveValue(channel.name);
            ar.SaveValue(channel.positionTimes.offset);
            ar.SaveValue(channel.positionTimes.size);
            ar.SaveValue(channel.positionKeys.offset);
            ar.SaveValue(channel.positionKeys.size);
            ar.SaveValue(channel.scaleTimes.offset);
            ar.SaveValue(channel.scaleTimes.size);
            ar.SaveValue(channel.scaleKeys.offset);
            ar.SaveValue(channel.scaleKeys.size);
            ar.SaveValue(channel.rotationTimes.offset);
            ar.SaveValue(channel.rotationTimes.size);
            ar.SaveValue(channel.rotationKeys.offset);
            ar.SaveValue(channel.rotationKeys.size);
        }
    }

    void AnimationClipAssetData::LoadSource(BinaryInputArchive &ar)
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

    std::vector<uint8_t> AnimationClipAssetData::BuildBulk()
    {
        std::vector<uint8_t> bulk;

        const auto append = [&bulk](const void *data, size_t bytes) -> AnimTrackHeader {
            AnimTrackHeader header;
            header.offset = bulk.size();
            header.size   = bytes;
            if (bytes != 0 && data != nullptr) {
                const auto *begin = reinterpret_cast<const uint8_t *>(data);
                bulk.insert(bulk.end(), begin, begin + bytes);
            }
            return header;
        };

        channels.clear();
        channels.reserve(nodeChannels.size());
        for (const auto &node : nodeChannels) {
            AnimChannelHeader header;
            header.name          = node.name;
            header.positionTimes = append(node.position.times.data(), node.position.times.size() * sizeof(AnimTimeKey));
            header.positionKeys  = append(node.position.keys.data(), node.position.keys.size() * sizeof(Vector3));
            header.scaleTimes    = append(node.scale.times.data(), node.scale.times.size() * sizeof(AnimTimeKey));
            header.scaleKeys     = append(node.scale.keys.data(), node.scale.keys.size() * sizeof(Vector3));
            header.rotationTimes = append(node.rotation.times.data(), node.rotation.times.size() * sizeof(AnimTimeKey));
            header.rotationKeys  = append(node.rotation.keys.data(), node.rotation.keys.size() * sizeof(Quaternion));
            channels.push_back(std::move(header));
        }
        return bulk;
    }

    AnimClipPtr CreateAnimationClipFromAsset(const AnimationClipAssetPtr &asset)
    {
        if (asset == nullptr) {
            return {};
        }
        const auto &data = asset->Data();
        if (data.version != AnimationClipAssetData::CURRENT_VERSION) {
            return {};
        }

        auto  *clip     = new AnimationClip(Name(data.name.c_str()));
        size_t numFrame = 0;
        for (const auto &header : data.channels) {
            AnimNodeChannelData node;
            node.name = header.name;

            const auto loadTrack = [&asset](const AnimTrackHeader &times, const AnimTrackHeader &keys, auto &out) {
                using KeyType = typename std::decay_t<decltype(out.keys)>::value_type;

                auto timesBytes = asset->ReadBulk(times.offset, times.size);
                out.times.resize(timesBytes.size() / sizeof(AnimTimeKey));
                if (!timesBytes.empty()) {
                    std::memcpy(out.times.data(), timesBytes.data(), timesBytes.size());
                }

                auto keysBytes = asset->ReadBulk(keys.offset, keys.size);
                out.keys.resize(keysBytes.size() / sizeof(KeyType));
                if (!keysBytes.empty()) {
                    std::memcpy(out.keys.data(), keysBytes.data(), keysBytes.size());
                }
            };

            loadTrack(header.positionTimes, header.positionKeys, node.position);
            loadTrack(header.scaleTimes, header.scaleKeys, node.scale);
            loadTrack(header.rotationTimes, header.rotationKeys, node.rotation);

            clip->AddChannel(new AnimationNodeChannel(node));
            numFrame = std::max(numFrame, node.position.keys.size());
            numFrame = std::max(numFrame, node.scale.keys.size());
            numFrame = std::max(numFrame, node.rotation.keys.size());
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
