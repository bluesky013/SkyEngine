#pragma once

#include <core/math/Vector3.h>
#include <core/util/Uuid.h>

namespace sky::particle {

    struct ParticleViewInfo {
        Vector3 position{};
        bool    valid = false;
    };

    class IParticleViewProvider {
    public:
        virtual ~IParticleViewProvider()                                      = default;
        virtual bool GetView(const Uuid &world, ParticleViewInfo &view) const = 0;
    };

} // namespace sky::particle
