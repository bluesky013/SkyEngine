//
// Created on 2026/10/26.
//

#include "JoltConversion.h"
#include "JoltWorld.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace sky::phy {

    JPH::RefConst<JPH::Shape> JoltWorld::Cook(const ShapeDesc &desc, const Vector3 &scale, uint32_t depth) const
    {
        const float margin = descriptor.collisionMargin;
        std::string error;
        if (depth == 0 && !ValidateShape(desc, &error)) {
            Fail(PhysicsError::InvalidArgument, error.c_str());
            return {};
        }
        if (depth > 32 || !Finite(desc.pivot) || !Finite(scale) || scale.x <= 0 || scale.y <= 0 || scale.z <= 0) {
            Fail(PhysicsError::InvalidArgument, "Invalid shape transform or compound depth");
            return {};
        }
        JPH::Shape::ShapeResult result;
        switch (desc.type) {
        case ShapeType::Box:
            if (!Finite(desc.halfExt) || desc.halfExt.x <= 0 || desc.halfExt.y <= 0 || desc.halfExt.z <= 0) {
                break;
            }
            result =
                JPH::BoxShapeSettings(ToJolt(desc.halfExt), std::min({margin, desc.halfExt.x * 0.5f, desc.halfExt.y * 0.5f, desc.halfExt.z * 0.5f}))
                    .Create();
            break;
        case ShapeType::Sphere:
            if (!Finite(desc.radius) || desc.radius <= 0) {
                break;
            }
            result = JPH::SphereShapeSettings(desc.radius).Create();
            break;
        case ShapeType::Capsule:
            if (!Finite(desc.radius) || desc.radius <= 0 || !Finite(desc.height) || desc.height < 0) {
                break;
            }
            result = desc.height == 0 ? JPH::SphereShapeSettings(desc.radius).Create()
                                      : JPH::CapsuleShapeSettings(desc.height * 0.5f, desc.radius).Create();
            break;
        case ShapeType::ConvexHull: {
            if (desc.points.size() < 4) {
                break;
            }
            JPH::Array<JPH::Vec3> points;
            for (const auto &p : desc.points) {
                if (!Finite(p)) {
                    Fail(PhysicsError::InvalidArgument, "Non-finite hull vertex");
                    return {};
                }
                points.push_back(ToJolt(p));
            }
            result = JPH::ConvexHullShapeSettings(points, margin).Create();
            break;
        }
        case ShapeType::TriangleMesh: {
            if (!desc.meshData) {
                break;
            }
            const auto  &mesh      = *desc.meshData;
            const size_t indexSize = mesh.indexType == IndexType::U16 ? 2 : 4;
            if ((mesh.indexType != IndexType::U16 && mesh.indexType != IndexType::U32) || mesh.vtxStride < sizeof(Vector3) ||
                mesh.position.size() % mesh.vtxStride != 0 || mesh.indexRaw.empty() || mesh.indexRaw.size() % (3 * indexSize) != 0) {
                break;
            }
            JPH::TriangleList triangles;
            for (size_t offset = 0; offset < mesh.indexRaw.size(); offset += 3 * indexSize) {
                JPH::Vec3 vertices[3];
                for (size_t j = 0; j < 3; ++j) {
                    uint32_t index = 0;
                    if (indexSize == 2) {
                        uint16_t value;
                        std::memcpy(&value, mesh.indexRaw.data() + offset + j * indexSize, 2);
                        index = value;
                    } else {
                        std::memcpy(&index, mesh.indexRaw.data() + offset + j * indexSize, 4);
                    }
                    if (index >= mesh.position.size() / mesh.vtxStride) {
                        Fail(PhysicsError::InvalidArgument, "Mesh index out of range");
                        return {};
                    }
                    Vector3 v;
                    std::memcpy(&v, mesh.position.data() + static_cast<size_t>(index) * mesh.vtxStride, sizeof(v));
                    if (!Finite(v)) {
                        Fail(PhysicsError::InvalidArgument, "Non-finite mesh vertex");
                        return {};
                    }
                    vertices[j] = ToJolt(v);
                }
                triangles.emplace_back(vertices[0], vertices[1], vertices[2]);
            }
            result = JPH::MeshShapeSettings(triangles).Create();
            break;
        }
        case ShapeType::HeightField: {
            if (desc.cols < 2 || desc.rows < 2 || static_cast<uint64_t>(desc.cols) * desc.rows != desc.samples.size() || desc.upAxis > 2 ||
                !Finite(desc.scaleX) || !Finite(desc.scaleZ) || desc.scaleX <= 0 || desc.scaleZ <= 0 || !Finite(desc.heightScale) ||
                desc.heightScale == 0 || !Finite(desc.heightOffset)) {
                break;
            }
            JPH::VertexList          vertices;
            JPH::IndexedTriangleList triangles;
            for (uint32_t z = 0; z < desc.rows; ++z) {
                for (uint32_t x = 0; x < desc.cols; ++x) {
                    const float h = desc.samples[static_cast<size_t>(z) * desc.cols + x] * desc.heightScale + desc.heightOffset;
                    const float a = (static_cast<float>(x) - (desc.cols - 1) * 0.5f) * desc.scaleX;
                    const float b = (static_cast<float>(z) - (desc.rows - 1) * 0.5f) * desc.scaleZ;
                    if (!Finite(h) || !Finite(a) || !Finite(b)) {
                        Fail(PhysicsError::InvalidArgument, "Non-finite heightfield");
                        return {};
                    }
                    if (desc.upAxis == 1) {
                        vertices.emplace_back(a, h, b);
                    } else if (desc.upAxis == 0) {
                        vertices.emplace_back(h, -a, b);
                    } else {
                        vertices.emplace_back(a, -b, h);
                    }
                    if (x + 1 < desc.cols && z + 1 < desc.rows) {
                        const uint32_t i = z * desc.cols + x;
                        triangles.emplace_back(i, i + desc.cols, i + 1);
                        triangles.emplace_back(i + 1, i + desc.cols, i + desc.cols + 1);
                    }
                }
            }
            result = JPH::MeshShapeSettings(vertices, triangles).Create();
            break;
        }
        case ShapeType::Compound: {
            if (desc.children.empty()) {
                break;
            }
            JPH::StaticCompoundShapeSettings compound;
            for (const auto &child : desc.children) {
                auto shape = Cook(child, VEC3_ONE, depth + 1);
                if (!shape) {
                    return {};
                }
                compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shape.GetPtr());
            }
            result = compound.Create();
            break;
        }
        default: break;
        }
        if (!result.IsValid()) {
            Fail(PhysicsError::InvalidArgument, result.HasError() ? result.GetError().c_str() : "Invalid shape descriptor");
            return {};
        }
        JPH::RefConst<JPH::Shape> shape = result.Get();
        if (desc.pivot.x != 0 || desc.pivot.y != 0 || desc.pivot.z != 0) {
            result = JPH::RotatedTranslatedShapeSettings(ToJolt(desc.pivot), JPH::Quat::sIdentity(), shape.GetPtr()).Create();
            if (!result.IsValid()) {
                Fail(PhysicsError::InvalidArgument, result.GetError().c_str());
                return {};
            }
            shape = result.Get();
        }
        if (scale.x != 1 || scale.y != 1 || scale.z != 1) {
            result = shape->ScaleShape(ToJolt(scale));
            if (!result.IsValid()) {
                Fail(PhysicsError::Unsupported, result.GetError().c_str());
                return {};
            }
            shape = result.Get();
        }
        return shape;
    }
} // namespace sky::phy
