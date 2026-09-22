//
// Created by blues on 2024/10/6.
//

#pragma once

#include <memory>

namespace sky::phy {

    // Backend-neutral material values.
    struct PhysicsMaterialData {
        float staticFriction  = 0.5f;
        float dynamicFriction = 0.5f;
        float restitution     = 0.f;
    };

    inline PhysicsMaterialData GetDefaultPhysicsMaterial()
    {
        return PhysicsMaterialData{};
    }

    // Backend material resource (e.g. PxMaterial / Bullet values).
    class IMaterialImpl {
    public:
        IMaterialImpl() = default;
        virtual ~IMaterialImpl() = default;
    };

    // Engine material wrapper: owns the backend material created through PhysicsRegistry.
    class PhysicsMaterial {
    public:
        explicit PhysicsMaterial(const PhysicsMaterialData &data);
        ~PhysicsMaterial();

        const PhysicsMaterialData &GetData() const { return data; }
        IMaterialImpl *GetImpl() const { return impl.get(); }

    private:
        PhysicsMaterialData            data;
        std::unique_ptr<IMaterialImpl> impl;
    };

} // namespace sky::phy
