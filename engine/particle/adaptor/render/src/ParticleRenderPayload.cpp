#include <particle/render/ParticleRenderPayload.h>

#include <particle/render/ParticleSpritePacking.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace sky::particle {
    namespace {
        constexpr uint32_t PAYLOAD_VERSION   = 1;
        constexpr size_t   MAX_PAYLOAD_BYTES = 16u * 1024u * 1024u;
        struct PayloadHeader {
            uint32_t version  = PAYLOAD_VERSION;
            uint32_t emitters = 0;
        };
        struct EmitterHeader {
            uint32_t blend = 0, texture = 0, count = 0;
        };

        template <typename T>
        void AppendBytes(std::vector<uint8_t> &bytes, const T &value)
        {
            const auto offset = bytes.size();
            bytes.resize(offset + sizeof(T));
            std::memcpy(bytes.data() + offset, &value, sizeof(T));
        }

        template <typename T>
        T ReadBytes(std::span<const std::byte> bytes, size_t &offset)
        {
            T value;
            std::memcpy(&value, bytes.data() + offset, sizeof(T));
            offset += sizeof(T);
            return value;
        }
    } // namespace

    std::optional<aurora::RendererError> ValidateParticleRenderPayload(const aurora::FrameExtensionPayload &payload, bool validateValues)
    {
        const auto invalid = [] { return aurora::RendererError{aurora::RendererErrorCode::INVALID_PACKET, "Invalid particle render payload"}; };
        if (payload.outputs.size() != 1 || !payload.outputs.front().IsValid() || payload.bytes.size() < sizeof(PayloadHeader) ||
            payload.bytes.size() > MAX_PAYLOAD_BYTES || payload.resources.size() > 1024)
            return invalid();
        const auto bytes  = std::as_bytes(std::span{payload.bytes});
        size_t     offset = 0;
        const auto header = ReadBytes<PayloadHeader>(bytes, offset);
        if (header.version != PAYLOAD_VERSION || header.emitters > 65536)
            return invalid();
        for (uint32_t index = 0; index < header.emitters; ++index) {
            if (bytes.size() - offset < sizeof(EmitterHeader))
                return invalid();
            const auto emitter = ReadBytes<EmitterHeader>(bytes, offset);
            if (emitter.blend >= static_cast<uint32_t>(ParticleBlendMode::END) || emitter.texture >= payload.resources.size() || !emitter.count ||
                emitter.count > (bytes.size() - offset) / sizeof(ParticleSpriteInstance))
                return invalid();
            const auto valueBytes = size_t(emitter.count) * sizeof(ParticleSpriteInstance);
            if (valueBytes > bytes.size() - offset)
                return invalid();
            if (validateValues) {
                for (uint32_t particle = 0; particle < emitter.count; ++particle) {
                    static_assert(sizeof(ParticleSpriteInstance) == sizeof(float) * 16);
                    const auto values = ReadBytes<std::array<float, 16>>(bytes, offset);
                    if (!std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); }))
                        return invalid();
                }
            } else {
                offset += valueBytes;
            }
        }
        return offset == bytes.size() ? std::optional<aurora::RendererError>{} : invalid();
    }

    ParticleRenderPayloadReader::ParticleRenderPayloadReader(std::span<const std::byte> inBytes) : bytes(inBytes)
    {
        if (bytes.size() < sizeof(PayloadHeader))
            return;
        const auto header = ReadBytes<PayloadHeader>(bytes, offset);
        if (header.version == PAYLOAD_VERSION)
            remaining = header.emitters;
    }

    bool ParticleRenderPayloadReader::Next(ParticleRenderPayloadEmitter &out)
    {
        if (!remaining || bytes.size() - offset < sizeof(EmitterHeader))
            return false;
        const auto header = ReadBytes<EmitterHeader>(bytes, offset);
        const auto size   = size_t(header.count) * sizeof(ParticleSpriteInstance);
        if (header.count > (bytes.size() - offset) / sizeof(ParticleSpriteInstance))
            return false;
        out = {static_cast<ParticleBlendMode>(header.blend), header.texture, header.count, bytes.subspan(offset, size)};
        offset += size;
        --remaining;
        return true;
    }

    std::variant<aurora::RendererError, aurora::FrameExtensionPayload>
    EncodeParticleRenderFrame(const ParticleRenderFrame &frame, aurora::ViewportId output, std::span<const ParticleRenderTexture> textures)
    {
        aurora::FrameExtensionPayload payload;
        payload.feature = Name("particle.sprite");
        payload.outputs.push_back(output);
        size_t byteCount = sizeof(PayloadHeader);
        for (const auto &emitter : frame.emitters) {
            if (emitter.type != ParticleRendererType::SPRITE || emitter.instances.empty() ||
                std::none_of(textures.begin(), textures.end(), [&](const auto &entry) { return entry.id == emitter.texture; }))
                continue;
            if (byteCount > MAX_PAYLOAD_BYTES - sizeof(EmitterHeader) ||
                emitter.instances.size() > (MAX_PAYLOAD_BYTES - sizeof(EmitterHeader) - byteCount) / sizeof(ParticleSpriteInstance))
                return aurora::RendererError{aurora::RendererErrorCode::OUT_OF_MEMORY, "Particle render payload exceeds the frame budget"};
            byteCount += sizeof(EmitterHeader) + emitter.instances.size() * sizeof(ParticleSpriteInstance);
        }
        payload.bytes.reserve(byteCount);
        PayloadHeader header;
        AppendBytes(payload.bytes, header);
        for (const auto &emitter : frame.emitters) {
            if (emitter.type != ParticleRendererType::SPRITE || emitter.instances.empty())
                continue;
            const auto texture = std::find_if(textures.begin(), textures.end(), [&](const auto &entry) { return entry.id == emitter.texture; });
            if (texture == textures.end())
                continue;
            auto       resource      = std::find(payload.resources.begin(), payload.resources.end(), texture->resource);
            const auto resourceIndex = static_cast<uint32_t>(resource - payload.resources.begin());
            if (resource == payload.resources.end())
                payload.resources.push_back(texture->resource);
            AppendBytes(payload.bytes,
                        EmitterHeader{static_cast<uint32_t>(emitter.blend), resourceIndex, static_cast<uint32_t>(emitter.instances.size())});
            for (const auto &particle : emitter.instances)
                AppendBytes(payload.bytes, PackParticleSpriteInstance(particle));
            ++header.emitters;
        }
        std::memcpy(payload.bytes.data(), &header, sizeof(header));
#if SKY_DEVELOP
        constexpr bool validateValues = true;
#else
        constexpr bool validateValues = false;
#endif
        if (auto error = ValidateParticleRenderPayload(payload, validateValues))
            return *error;
        return payload;
    }
} // namespace sky::particle
