//
// Animation asset load/save + runtime build (see AnimationAsset.h).
//

#include <content/adaptor/assets/AnimationAsset.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace sky {

    namespace {
        bool CanRead(BinaryInputArchive &ar, uint64_t bytes)
        {
            auto &stream = ar.GetStream().GetStream();
            if (!ar.IsValid() || !stream) {
                return false;
            }
            const auto position = stream.tellg();
            stream.seekg(0, std::ios::end);
            const auto end = stream.tellg();
            stream.seekg(position);
            return stream && position >= 0 && end >= position && bytes <= static_cast<uint64_t>(end - position);
        }

        bool ReadString(BinaryInputArchive &ar, std::string &value)
        {
            uint32_t bytes = 0;
            ar.LoadValue(bytes);
            if (bytes > 4096 || !CanRead(ar, bytes)) {
                return false;
            }
            value.resize(bytes);
            ar.LoadValue(value.data(), bytes);
            return ar.IsValid() && value.find('\0') == std::string::npos;
        }

        template <typename T>
        bool ReadArray(BinaryInputArchive &ar, std::vector<T> &values)
        {
            uint32_t bytes = 0;
            ar.LoadValue(bytes);
            if (bytes % sizeof(T) != 0 || !CanRead(ar, bytes)) {
                return false;
            }
            values.resize(bytes / sizeof(T));
            if (bytes != 0) {
                ar.LoadValue(reinterpret_cast<char *>(values.data()), bytes);
            }
            return ar.IsValid();
        }
    } // namespace

    void AnimationClipAssetData::clear()
    {
        name.clear();
        frameRate        = 0.f;
        sourceFrameCount = 0;
        skeleton         = Uuid{};
        channels.clear();
        nodeChannels.clear();
        validation = {};
    }

    void AnimationClipAssetData::Load(BinaryInputArchive &ar)
    {
        clear();
        ar.LoadValue(version);
        if (version != CURRENT_VERSION) {
            validation = {AnimationError::UnsupportedVersion, "clip product version is obsolete; recook the source"};
            return;
        }
        if (!ReadString(ar, name)) {
            validation = {AnimationError::InvalidAsset, "invalid clip header name"};
            return;
        }
        ar.LoadValue(frameRate);
        ar.LoadValue(skeleton);
        ar.LoadValue(sourceFrameCount);

        uint32_t count = 0;
        ar.LoadValue(count);
        if (!std::isfinite(frameRate) || frameRate < 0.f || count > ANIM_MAX_BONE || !ar.IsValid()) {
            validation = {AnimationError::InvalidAsset, "invalid clip header rate or channel count"};
            return;
        }
        channels.resize(count);
        for (auto &channel : channels) {
            if (!ReadString(ar, channel.name)) {
                validation = {AnimationError::InvalidAsset, "invalid clip channel name"};
                return;
            }
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
        if (!ar.IsValid()) {
            validation = {AnimationError::InvalidAsset, "truncated clip header"};
        }
    }

    void AnimationClipAssetData::Save(BinaryOutputArchive &ar) const
    {
        ar.SaveValue(version);
        ar.SaveValue(name);
        ar.SaveValue(frameRate);
        ar.SaveValue(skeleton);
        ar.SaveValue(sourceFrameCount);

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

    AnimationStatus AnimationClipAssetData::LoadSource(BinaryInputArchive &ar)
    {
        clear();
        ar.LoadValue(version);
        if (version != CURRENT_VERSION && version != LEGACY_SOURCE_VERSION) {
            validation = {AnimationError::UnsupportedVersion, "unsupported clip source version"};
            return validation;
        }
        if (!ReadString(ar, name)) {
            validation = {AnimationError::InvalidAsset, "invalid clip source name"};
            return validation;
        }
        ar.LoadValue(frameRate);
        ar.LoadValue(skeleton);
        if (version == CURRENT_VERSION) {
            ar.LoadValue(sourceFrameCount);
        }

        uint32_t count = 0;
        ar.LoadValue(count);
        if (count > ANIM_MAX_BONE || !ar.IsValid()) {
            validation = {AnimationError::InvalidAsset, "invalid clip source channel count"};
            return validation;
        }
        nodeChannels.resize(count);
        for (auto &channel : nodeChannels) {
            if (!ReadString(ar, channel.name) || !ReadArray(ar, channel.position.times) || !ReadArray(ar, channel.position.keys) ||
                !ReadArray(ar, channel.scale.times) || !ReadArray(ar, channel.scale.keys) || !ReadArray(ar, channel.rotation.times) ||
                !ReadArray(ar, channel.rotation.keys)) {
                validation = {AnimationError::InvalidAsset, "truncated or misaligned clip source channel"};
                return validation;
            }
        }
        version = CURRENT_VERSION;
        return PrepareSource();
    }

    AnimationStatus AnimationClipAssetData::SaveSource(BinaryOutputArchive &ar)
    {
        const auto status = PrepareSource();
        if (!status) {
            return status;
        }
        ar.SaveValue(CURRENT_VERSION);
        ar.SaveValue(name);
        ar.SaveValue(frameRate);
        ar.SaveValue(skeleton);
        ar.SaveValue(sourceFrameCount);
        ar.SaveValue(static_cast<uint32_t>(nodeChannels.size()));
        const auto write = [&ar](const auto &values) {
            const auto size = values.size() * sizeof(typename std::decay_t<decltype(values)>::value_type);
            if (size > UINT32_MAX) {
                return false;
            }
            ar.SaveValue(static_cast<uint32_t>(size));
            if (size != 0) {
                ar.SaveValue(reinterpret_cast<const char *>(values.data()), size);
            }
            return ar.IsValid();
        };
        for (const auto &channel : nodeChannels) {
            ar.SaveValue(channel.name);
            if (!write(channel.position.times) || !write(channel.position.keys) || !write(channel.scale.times) || !write(channel.scale.keys) ||
                !write(channel.rotation.times) || !write(channel.rotation.keys)) {
                return {AnimationError::InvalidAsset, "clip source write failed or track exceeds format size limit"};
            }
        }
        return ar.IsValid() ? AnimationStatus{} : AnimationStatus{AnimationError::InvalidAsset, "clip source write failed"};
    }

    AnimationStatus AnimationClipAssetData::PrepareSource()
    {
        validation = {};
        if (!std::isfinite(frameRate) || frameRate < 0.f || nodeChannels.size() > ANIM_MAX_BONE) {
            validation = {AnimationError::InvalidDuration, "invalid clip frame rate or channel count"};
            return validation;
        }
        uint32_t                        frameCount = 0;
        std::unordered_set<std::string> names;
        for (const auto &channel : nodeChannels) {
            validation = channel.Validate();
            if (!validation) {
                return validation;
            }
            if (!names.emplace(channel.name).second) {
                validation = {AnimationError::InvalidChannel, "duplicate clip source channel"};
                return validation;
            }
            frameCount = std::max(frameCount, channel.GetFrameCount());
        }
        if (sourceFrameCount != 0 && sourceFrameCount < frameCount) {
            validation = {AnimationError::InvalidDuration, "source frame count does not cover all keys"};
            return validation;
        }
        if (sourceFrameCount == 0) {
            sourceFrameCount = frameCount;
        }
        if (frameRate > 0.f && !std::isfinite(static_cast<float>(sourceFrameCount) / frameRate)) {
            validation = {AnimationError::InvalidDuration, "clip duration is not finite"};
        }
        return validation;
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

    AnimClipPtr CreateAnimationClipFromAsset(const AnimationClipAssetPtr &asset, AnimationStatus *status)
    {
        const auto fail = [status](AnimationStatus error) -> AnimClipPtr {
            if (status != nullptr) {
                *status = std::move(error);
            }
            return {};
        };
        if (asset == nullptr) {
            return fail({AnimationError::InvalidAsset, "clip asset is missing"});
        }
        const auto &data = asset->Data();
        if (data.version != AnimationClipAssetData::CURRENT_VERSION) {
            return fail({AnimationError::UnsupportedVersion, "clip product version is obsolete; recook the source"});
        }
        if (!data.validation) {
            return fail(data.validation);
        }

        AnimClipPtr clip = new AnimationClip(Name(data.name.c_str()));
        for (const auto &header : data.channels) {
            AnimNodeChannelData node;
            node.name = header.name;

            const auto loadTrack = [&asset, &data](const AnimTrackHeader &times, const AnimTrackHeader &keys, auto &out) {
                using KeyType = typename std::decay_t<decltype(out.keys)>::value_type;
                if (times.size % sizeof(AnimTimeKey) != 0 || keys.size % sizeof(KeyType) != 0 ||
                    times.size / sizeof(AnimTimeKey) != keys.size / sizeof(KeyType) || times.size / sizeof(AnimTimeKey) > data.sourceFrameCount ||
                    times.offset > UINT64_MAX - times.size || keys.offset > UINT64_MAX - keys.size) {
                    return false;
                }
                auto timesBytes = asset->ReadBulk(times.offset, times.size);
                if (timesBytes.size() != times.size) {
                    return false;
                }
                out.times.resize(timesBytes.size() / sizeof(AnimTimeKey));
                if (!timesBytes.empty()) {
                    std::memcpy(out.times.data(), timesBytes.data(), timesBytes.size());
                }

                auto keysBytes = asset->ReadBulk(keys.offset, keys.size);
                if (keysBytes.size() != keys.size) {
                    return false;
                }
                out.keys.resize(keysBytes.size() / sizeof(KeyType));
                if (!keysBytes.empty()) {
                    std::memcpy(out.keys.data(), keysBytes.data(), keysBytes.size());
                }
                return true;
            };

            if (!loadTrack(header.positionTimes, header.positionKeys, node.position) || !loadTrack(header.scaleTimes, header.scaleKeys, node.scale) ||
                !loadTrack(header.rotationTimes, header.rotationKeys, node.rotation)) {
                return fail({AnimationError::InvalidAsset, "invalid clip bulk range or time/value count"});
            }
            const auto channelStatus = clip->AddChannel(new AnimationNodeChannel(node));
            if (!channelStatus) {
                return fail(channelStatus);
            }
        }
        clip->SetFrameRate(data.frameRate);
        clip->SetNumFrame(data.sourceFrameCount);
        const auto clipStatus = clip->Validate();
        if (!clipStatus) {
            return fail(clipStatus);
        }
        if (status != nullptr) {
            *status = {};
        }
        return clip;
    }

} // namespace sky
