#pragma once

#include <istream>
#include <physics/PhysicsMaterial.h>
#include <physics/PhysicsShapes.h>
#include <physics/PhysicsStatus.h>
#include <string_view>

namespace sky::phy {
    inline constexpr std::string_view COLLISION_ASSET_TYPE      = "PhysicsCollision";
    inline constexpr std::string_view MATERIAL_ASSET_TYPE       = "PhysicsMaterial";
    inline constexpr size_t           MAX_PHYSICS_PRODUCT_BYTES = 64u * 1024u * 1024u;

    struct CollisionSource {
        ShapeDesc         shape;
        std::vector<Uuid> elements;
    };

    bool          ReadPhysicsText(std::istream &stream, std::string &out);
    PhysicsStatus ParseCollision(std::string_view text, CollisionSource &out);
    PhysicsStatus WriteCollision(const CollisionSource &source, std::string &out);
    PhysicsStatus ParsePhysicsMaterial(std::string_view text, PhysicsMaterialData &out);
    PhysicsStatus WritePhysicsMaterial(const PhysicsMaterialData &source, std::string &out);
} // namespace sky::phy
