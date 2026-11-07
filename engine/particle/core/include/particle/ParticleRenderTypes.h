#pragma once

#include <core/math/Color.h>
#include <core/math/Vector3.h>
#include <core/util/Uuid.h>
#include <particle/ParticleEffect.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace sky::particle {

    // World-space billboard instance; renderers own any GPU translation.
    struct ParticleRenderInstance {
        Vector3 position{};
        float   size     = 1.f;
        float   rotation = 0.f;
        Color   color{1.f, 1.f, 1.f, 1.f};
        float   subU      = 0.f;
        float   subV      = 0.f;
        float   subWidth  = 1.f;
        float   subHeight = 1.f;
    };

    struct ParticleRenderEmitter {
        uint64_t                            programHash = 0;
        Uuid                                texture;
        ParticleRendererType                type  = ParticleRendererType::SPRITE;
        ParticleBlendMode                   blend = ParticleBlendMode::ALPHA;
        std::vector<ParticleRenderInstance> instances;
    };

    struct ParticleRenderFrame {
        Uuid                               world;
        uint64_t                           revision = 0;
        std::vector<ParticleRenderEmitter> emitters;
    };

    class IParticleRenderSink {
    public:
        virtual ~IParticleRenderSink()                                  = default;
        virtual void Submit(std::shared_ptr<const ParticleRenderFrame>) = 0;
        virtual void RemoveWorld(const Uuid &)                          = 0;
    };

} // namespace sky::particle
