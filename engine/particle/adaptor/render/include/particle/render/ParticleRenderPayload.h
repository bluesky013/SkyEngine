#pragma once

#include <aurora/renderer/IRenderFeature.h>
#include <core/util/Uuid.h>
#include <particle/ParticleRenderTypes.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace sky::particle {

    struct ParticleRenderTexture {
        Uuid                      id;
        aurora::RenderResourceKey resource;
    };

    struct ParticleRenderPayloadEmitter {
        ParticleBlendMode              blend = ParticleBlendMode::ALPHA;
        uint32_t                       texture = 0;
        uint32_t                       count = 0;
        std::span<const std::byte>     instances;
    };

    std::variant<aurora::RendererError, aurora::FrameExtensionPayload>
    EncodeParticleRenderFrame(const ParticleRenderFrame &, aurora::ViewportId, std::span<const ParticleRenderTexture>);

    std::optional<aurora::RendererError> ValidateParticleRenderPayload(const aurora::FrameExtensionPayload &, bool validateValues = false);

    // Reads a payload already validated by ValidateParticleRenderPayload.
    class ParticleRenderPayloadReader {
    public:
        explicit ParticleRenderPayloadReader(std::span<const std::byte> bytes);
        bool Next(ParticleRenderPayloadEmitter &out);

    private:
        std::span<const std::byte> bytes;
        size_t                     offset = 0;
        uint32_t                   remaining = 0;
    };

} // namespace sky::particle
