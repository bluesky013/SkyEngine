#include <softbody/world/SoftBodyPose.h>

#include <algorithm>
#include <cmath>

namespace sky::softbody {
    namespace {
        bool Finite(const Vector3 &p)
        {
            return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
        }
        float Determinant(const Matrix3 &m)
        {
            return m[0].Dot(m[1].Cross(m[2]));
        }
    } // namespace
    phy::PhysicsStatus BuildSoftBodyPose(const SoftBodyAssetData &asset,
                                         std::span<const Vector3> worldPositions,
                                         const Transform         &root,
                                         std::span<const Matrix4> baseModels,
                                         std::vector<Matrix4>    &models,
                                         std::vector<Matrix4>    &modelNormals)
    {
        const auto invalid = [](const char *message) { return phy::PhysicsStatus{phy::PhysicsError::InvalidArgument, message}; };
        if (const auto status = ValidateSoftBodyAsset(asset); !status)
            return status;
        const size_t count = asset.body.mesh.vertices.size();
        if (worldPositions.size() != count || baseModels.size() != count + 1 || root.scale != VEC3_ONE || !Finite(root.translation) ||
            !std::all_of(std::begin(root.rotation.v), std::end(root.rotation.v), [](float x) { return std::isfinite(x); }) ||
            std::abs(root.rotation.Dot(root.rotation) - 1.f) > .0001f)
            return invalid("Soft body pose requires a complete rig and rigid actor transform");
        const auto           inverse = root.GetInverse();
        std::vector<Vector3> points;
        points.reserve(count);
        for (const auto &p : worldPositions) {
            const auto local = inverse.Translate(p);
            if (!Finite(local))
                return invalid("Soft body pose contains a nonfinite point");
            points.push_back(local);
        }
        std::vector<Matrix3> gradients(count);
        std::vector<float>   totals(count);
        for (const auto &tet : asset.body.mesh.tetrahedra) {
            const auto   &ids = tet.vertices;
            const auto   &a   = asset.body.mesh.vertices[ids[0]].position;
            const Matrix3 rest(asset.body.mesh.vertices[ids[1]].position - a, asset.body.mesh.vertices[ids[2]].position - a,
                               asset.body.mesh.vertices[ids[3]].position - a);
            const Matrix3 deformed(points[ids[1]] - points[ids[0]], points[ids[2]] - points[ids[0]], points[ids[3]] - points[ids[0]]);
            const float   volume      = Determinant(rest);
            const auto    inverseRest = rest.Inverse();
            const Matrix3 gradient(deformed * inverseRest[0], deformed * inverseRest[1], deformed * inverseRest[2]);
            if (!std::isfinite(Determinant(gradient)) || Determinant(gradient) <= 1e-7f)
                return invalid("Soft body normal field is inverted or singular");
            for (const auto id : ids) {
                for (uint32_t column = 0; column < 3; ++column)
                    gradients[id][column] += gradient[column] * volume;
                totals[id] += volume;
            }
        }
        std::vector<Matrix4> next(baseModels.begin(), baseModels.end()), normals;
        normals.reserve(count + 1);
        for (const auto &base : baseModels)
            if (!std::all_of(std::begin(base.v), std::end(base.v), [](float x) { return std::isfinite(x); }) || !std::isfinite(base.Determinant()) ||
                std::abs(base.Determinant()) < 1e-12f)
                return invalid("Soft body base pose has invalid matrices");
        const auto rootNormal = baseModels[0].InverseTranspose();
        if (!std::all_of(std::begin(rootNormal.v), std::end(rootNormal.v), [](float x) { return std::isfinite(x); }))
            return invalid("Soft body root normal transform overflowed");
        if (asset.normalPolicy == SoftBodyNormalPolicy::TetGradient)
            normals.push_back(rootNormal);
        for (size_t i = 0; i < count; ++i) {
            if (totals[i] <= 0)
                return invalid("Soft body vertex has no normal neighbourhood");
            const auto delta = points[i] - asset.body.mesh.vertices[i].position;
            next[i + 1][3].x += delta.x;
            next[i + 1][3].y += delta.y;
            next[i + 1][3].z += delta.z;
            if (!std::all_of(std::begin(next[i + 1].v), std::end(next[i + 1].v), [](float x) { return std::isfinite(x); }))
                return invalid("Soft body control transform overflowed");
            if (asset.normalPolicy == SoftBodyNormalPolicy::StandardSkinning)
                continue;
            const Matrix3 gradient(gradients[i][0] / totals[i], gradients[i][1] / totals[i], gradients[i][2] / totals[i]);
            if (!std::isfinite(Determinant(gradient)) || Determinant(gradient) <= 1e-7f)
                return invalid("Soft body averaged normal field is singular");
            auto deformation = Matrix4::Identity();
            for (uint32_t col = 0; col < 3; ++col)
                for (uint32_t row = 0; row < 3; ++row)
                    deformation[col][row] = gradient[col][row];
            const auto normal = deformation.InverseTranspose() * baseModels[i + 1].InverseTranspose();
            if (!std::all_of(std::begin(normal.v), std::end(normal.v), [](float x) { return std::isfinite(x); }))
                return invalid("Soft body normal transform overflowed");
            normals.push_back(normal);
        }
        models       = std::move(next);
        modelNormals = std::move(normals);
        return {};
    }
} // namespace sky::softbody
