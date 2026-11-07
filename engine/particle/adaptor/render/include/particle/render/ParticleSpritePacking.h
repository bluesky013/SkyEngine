#pragma once

#include <core/math/Color.h>
#include <core/math/Vector4.h>
#include <particle/ParticleRenderTypes.h>

namespace sky::particle {

    struct ParticleSpriteInstance {
        Vector4 positionSize;
        Color   color;
        Vector4 rotationSubUv;
        Vector4 uvRect;
    };

    ParticleSpriteInstance PackParticleSpriteInstance(const ParticleRenderInstance &);

} // namespace sky::particle
