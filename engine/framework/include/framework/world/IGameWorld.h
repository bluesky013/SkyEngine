#pragma once

namespace sky {
    class World;

    class IGameWorld {
    public:
        virtual ~IGameWorld()           = default;
        virtual World *GetWorld() const = 0;
    };
} // namespace sky
