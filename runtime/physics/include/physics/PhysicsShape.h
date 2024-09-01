//
// Created by blues on 2024/9/1.
//

#pragma once

#include <cstdint>

namespace sky::phy {

    enum class PhysicsShapeType : uint32_t {

    };

    class PhysicsShape {
    public:
        PhysicsShape() = default;
        ~PhysicsShape() = default;
    };

    class PhysicsShapeFactory {
    public:
        PhysicsShapeFactory() = default;
        virtual ~PhysicsShapeFactory() = default;

        virtual PhysicsShape* CreateShape(PhysicsShapeType type) = 0;
    };

} // namespace sky::phy
