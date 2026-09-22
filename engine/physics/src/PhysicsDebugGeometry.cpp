//
// Created on 2026/09/22.
//

#include <physics/PhysicsDebugGeometry.h>

#include <cstring>

namespace sky::phy {

    namespace {
        PhysicsDebugVertex MakeVertex(const Vector3 &position, const float color[4])
        {
            PhysicsDebugVertex vertex;
            vertex.position = position;
            std::memcpy(vertex.color, color, sizeof(vertex.color));
            return vertex;
        }
    } // namespace

    void PhysicsDebugGeometry::AddLine(const Vector3 &a, const Vector3 &b, const float color[4])
    {
        lines.push_back(MakeVertex(a, color));
        lines.push_back(MakeVertex(b, color));
    }

    void PhysicsDebugGeometry::AddTriangle(const Vector3 &a, const Vector3 &b, const Vector3 &c, const float color[4])
    {
        triangles.push_back(MakeVertex(a, color));
        triangles.push_back(MakeVertex(b, color));
        triangles.push_back(MakeVertex(c, color));
    }

} // namespace sky::phy
