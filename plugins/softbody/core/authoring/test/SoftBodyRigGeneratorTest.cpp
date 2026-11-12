#include <softbody/authoring/SoftBodyRigGenerator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace sky::softbody {
    namespace {
        constexpr float    PI = 3.14159265358979323846f;
        RigGenerationInput Sphere()
        {
            RigGenerationInput input;
            input.source.key                = "sphere";
            input.bodyPrimitive             = "body";
            input.settings.maxControlPoints = 30;
            phy::AuthoringMeshPrimitive primitive;
            primitive.key = "body";
            primitive.positions.push_back({0.f, .2f, 0.f});
            constexpr uint32_t longitude = 24, latitude = 12;
            for (uint32_t y = 1; y < latitude; ++y)
                for (uint32_t x = 0; x < longitude; ++x) {
                    const float theta = PI * static_cast<float>(y) / latitude, phi = 2.f * PI * static_cast<float>(x) / longitude;
                    primitive.positions.push_back(
                        {.2f * std::sin(theta) * std::cos(phi), .2f * std::cos(theta), .2f * std::sin(theta) * std::sin(phi)});
                }
            const uint32_t bottom = static_cast<uint32_t>(primitive.positions.size());
            primitive.positions.push_back({0.f, -.2f, 0.f});
            const auto triangle = [&](uint32_t a, uint32_t b, uint32_t c) {
                const auto   &p = primitive.positions[a];
                const auto   &q = primitive.positions[b];
                const auto   &r = primitive.positions[c];
                const Vector3 e = q - p, f = r - p;
                const Vector3 n{e.y * f.z - e.z * f.y, e.z * f.x - e.x * f.z, e.x * f.y - e.y * f.x};
                if (n.x * p.x + n.y * p.y + n.z * p.z < 0.f)
                    std::swap(b, c);
                primitive.indices.insert(primitive.indices.end(), {a, b, c});
            };
            for (uint32_t x = 0; x < longitude; ++x) {
                triangle(0, 1 + x, 1 + (x + 1) % longitude);
                triangle(bottom, 1 + (latitude - 2) * longitude + x, 1 + (latitude - 2) * longitude + (x + 1) % longitude);
            }
            for (uint32_t y = 0; y < latitude - 2; ++y)
                for (uint32_t x = 0; x < longitude; ++x) {
                    const uint32_t a = 1 + y * longitude + x, b = 1 + y * longitude + (x + 1) % longitude;
                    triangle(a, b, a + longitude);
                    triangle(b, b + longitude, a + longitude);
                }
            input.source.primitives.push_back(std::move(primitive));
            input.controls = {{"anchor", {0.f, 0.f, 0.f}, phy::SoftBodyAttachmentMode::RootKinematic},
                              {"head", {.04f, .02f, .01f}, phy::SoftBodyAttachmentMode::WorldFixed}};
            return input;
        }
        RigGenerationInput SuppliedFan()
        {
            auto input = Sphere();
            input.controls.clear();
            for (auto &p : input.source.primitives[0].positions)
                p *= 2.f;
            auto &g                = input.settings;
            g.mode                 = RigGenerationMode::SuppliedCenterFan;
            g.maxControlPoints     = 15;
            g.centerIndex          = 14;
            g.bindingPositionScale = .7f;
            for (uint32_t ring = 0; ring < 2; ++ring)
                for (uint32_t i = 0; i < 6; ++i) {
                    const float angle = static_cast<float>(i) * PI / 3.f;
                    input.controls.push_back(
                        {"point-" + std::to_string(input.controls.size()),
                         {.5f * std::cos(PI / 6.f) * std::cos(angle), .5f * std::cos(PI / 6.f) * std::sin(angle), ring == 0 ? -.25f : .25f}});
                }
            input.controls.push_back({"point-12", {0, 0, .5f}});
            input.controls.push_back({"point-13", {0, 0, -.5f}});
            input.controls.push_back({"point-14", {0, 0, 0}});
            const auto face = [&](uint32_t a, uint32_t b, uint32_t c) {
                if (input.controls[a].position.Dot(input.controls[b].position.Cross(input.controls[c].position)) < 0.f)
                    std::swap(b, c);
                g.tetrahedra.push_back({14, a, b, c});
            };
            for (uint32_t i = 0; i < 6; ++i) {
                const uint32_t j = (i + 1) % 6;
                face(i, j, i + 6);
                face(j, j + 6, i + 6);
                face(12, i + 6, j + 6);
                face(13, j, i);
            }
            return input;
        }
        void VerifyBodyWeights(const RigGenerationInput &input, const RigGenerationOutput &output)
        {
            ASSERT_EQ(output.binding.primitives[0].vertices.size(), input.source.primitives[0].positions.size());
            for (size_t i = 0; i < output.binding.primitives[0].vertices.size(); ++i) {
                const auto &entry = output.binding.primitives[0].vertices[i];
                Vector3     reconstructed{};
                float       sum = 0.f;
                for (size_t k = 0; k < 4; ++k) {
                    ASSERT_GT(entry.bones[k], 0);
                    ASSERT_LT(entry.bones[k], output.binding.bones.size());
                    ASSERT_GE(entry.weights[k], 0.f);
                    sum += entry.weights[k];
                    reconstructed += output.binding.bones[entry.bones[k]].reference.translation * entry.weights[k];
                }
                EXPECT_NEAR(sum, 1.f, 2e-7f);
                for (size_t axis = 0; axis < 3; ++axis)
                    EXPECT_NEAR(reconstructed[axis], input.source.primitives[0].positions[i][axis], 1e-6f);
            }
        }
        void PrintQuality(const RigGenerationInput &input, const RigGenerationOutput &output)
        {
            Vector3 sourceLow{INFINITY, INFINITY, INFINITY}, sourceHigh{-INFINITY, -INFINITY, -INFINITY};
            Vector3 cageLow = sourceLow, cageHigh = sourceHigh;
            for (const auto &point : input.source.primitives[0].positions)
                for (size_t axis = 0; axis < 3; ++axis) {
                    sourceLow[axis]  = std::min(sourceLow[axis], point[axis]);
                    sourceHigh[axis] = std::max(sourceHigh[axis], point[axis]);
                }
            for (const auto &face : output.mesh.faces)
                for (const auto index : face)
                    for (size_t axis = 0; axis < 3; ++axis) {
                        cageLow[axis]  = std::min(cageLow[axis], output.mesh.vertices[index].position[axis]);
                        cageHigh[axis] = std::max(cageHigh[axis], output.mesh.vertices[index].position[axis]);
                    }
            std::cout << " algorithm=" << output.diagnostics.algorithm << " controls=" << output.mesh.vertices.size()
                      << " tetrahedra=" << output.mesh.tetrahedra.size() << " volume_excess=" << output.diagnostics.relativeVolumeExcess
                      << " sampled_distance=" << output.diagnostics.maximumSampledSurfaceDistance
                      << " distance_bound=" << output.diagnostics.maximumSurfaceDistance << " min_quality=" << output.diagnostics.minimumTetQuality
                      << " rest_error=" << output.diagnostics.maximumRestReconstructionError << " support_gaps=";
            for (size_t axis = 0; axis < 3; ++axis) {
                std::cout << sourceLow[axis] - cageLow[axis] << ',' << cageHigh[axis] - sourceHigh[axis] << ';';
                EXPECT_NEAR(output.diagnostics.axisSupportGaps[axis * 2], sourceLow[axis] - cageLow[axis], 1e-6);
                EXPECT_NEAR(output.diagnostics.axisSupportGaps[axis * 2 + 1], cageHigh[axis] - sourceHigh[axis], 1e-6);
            }
            std::cout << '\n';
        }
    } // namespace

    TEST(SoftBodyAuthoring, SuppliedCenterFanPreservesTopologyAndFourWeightDisplacementBinding)
    {
        auto          input = SuppliedFan();
        const Vector3 anchor{.2f, .1f, .15f};
        auto          eye = input.source.primitives[0];
        eye.key           = "eye";
        eye.positions     = {anchor, anchor + Vector3{.02f, 0, 0}, anchor + Vector3{0, .02f, 0}};
        eye.indices       = {0, 1, 2};
        input.source.primitives.push_back(eye);
        input.attachments.push_back({"eye", "", AttachmentBindingMode::BindingAnchor, anchor});
        RigGenerationOutput out;
        const auto          status = GenerateRig(input, out);
        ASSERT_TRUE(status) << status.message;
        ASSERT_EQ(out.mesh.vertices.size(), 15u);
        ASSERT_EQ(out.mesh.tetrahedra.size(), 24u);
        EXPECT_EQ(out.mesh.edges.size(), 50u);
        ASSERT_EQ(out.binding.bones.size(), 16u);
        EXPECT_TRUE(out.diagnostics.coverageInBindingSpace);
        EXPECT_FLOAT_EQ(out.diagnostics.bindingPositionScale, .7f);
        EXPECT_LT(out.diagnostics.maximumBindingPositionError, 1e-6);
        EXPECT_LT(out.diagnostics.maximumRestReconstructionError, 1e-6);
        for (const auto &f : out.mesh.faces)
            EXPECT_EQ(std::find(f.begin(), f.end(), 14u), f.end());
        for (size_t i = 0; i < input.controls.size(); ++i) {
            EXPECT_EQ(out.mesh.vertices[i].position, input.controls[i].position);
            EXPECT_EQ(out.controlIds[i], input.controls[i].id);
            EXPECT_EQ(out.binding.bones[i + 1].reference.translation, input.controls[i].position);
            EXPECT_EQ(out.binding.bones[i + 1].parent, 0u);
        }
        const auto    deform = [](Vector3 p) { return Vector3{1.2f * p.x + .1f * p.y, .8f * p.y, 1.1f * p.z}; };
        const Vector3 shift{.01f, -.03f, .02f};
        for (size_t primitive = 0; primitive < out.binding.primitives.size(); ++primitive) {
            const auto &points = input.source.primitives[primitive].positions;
            for (size_t vertex = 0; vertex < points.size(); ++vertex) {
                const auto &weights = out.binding.primitives[primitive].vertices[vertex];
                Vector3     rest{}, moved{};
                float       sum = 0;
                for (size_t k = 0; k < 4; ++k) {
                    ASSERT_GT(weights.bones[k], 0u);
                    ASSERT_LE(weights.bones[k], 15u);
                    ASSERT_GE(weights.weights[k], 0.f);
                    sum += weights.weights[k];
                    const auto &bone = out.binding.bones[weights.bones[k]];
                    const auto  bind = bone.reference.ToMatrix();
                    const auto  p    = bind * bind.Inverse() * Vector4(points[vertex].x, points[vertex].y, points[vertex].z, 1);
                    rest += Vector3{p.x, p.y, p.z} * weights.weights[k];
                    moved += (points[vertex] + deform(bone.reference.translation) + shift - bone.reference.translation) * weights.weights[k];
                }
                EXPECT_NEAR(sum, 1.f, 2e-7f);
                const Vector3 q        = (primitive == 0 ? points[vertex] : anchor) * .7f;
                const Vector3 expected = points[vertex] + deform(q) + shift - q;
                for (size_t axis = 0; axis < 3; ++axis) {
                    EXPECT_NEAR(rest[axis], points[vertex][axis], 1e-6f);
                    EXPECT_NEAR(moved[axis], expected[axis], 1e-6f);
                }
                if (primitive != 0) {
                    EXPECT_EQ(weights.bones, out.binding.primitives[primitive].vertices[0].bones);
                    EXPECT_EQ(weights.weights, out.binding.primitives[primitive].vertices[0].weights);
                }
            }
        }
    }

    TEST(SoftBodyAuthoring, InvalidSuppliedCagesAndCancellationPreserveCompletePreviousRig)
    {
        const auto          valid = SuppliedFan();
        RigGenerationOutput out;
        ASSERT_TRUE(GenerateRig(valid, out));
        const auto                      ids       = out.controlIds;
        const auto                      signature = out.binding.layoutSignature;
        std::vector<RigGenerationInput> invalid(9, valid);
        invalid[0].settings.tetrahedra[0].b = 999;
        invalid[1].settings.tetrahedra.push_back(invalid[1].settings.tetrahedra[0]);
        invalid[2].settings.tetrahedra.pop_back();
        std::swap(invalid[3].settings.tetrahedra[0].b, invalid[3].settings.tetrahedra[0].c);
        invalid[4].settings.centerIndex          = 0;
        invalid[5].settings.bindingPositionScale = 5.f;
        invalid[6].settings.minTetQuality        = .99f;
        invalid[7].settings.fillControlBudget    = true;
        invalid[8].settings.mode                 = RigGenerationMode::END;
        for (const auto &input : invalid) {
            EXPECT_FALSE(GenerateRig(input, out));
            EXPECT_EQ(out.controlIds, ids);
            EXPECT_EQ(out.binding.layoutSignature, signature);
            EXPECT_EQ(out.mesh.tetrahedra.size(), 24u);
        }
        const std::atomic_bool cancelled{true};
        EXPECT_FALSE(GenerateRig(valid, out, &cancelled));
        EXPECT_EQ(out.controlIds, ids);
        EXPECT_EQ(out.binding.layoutSignature, signature);
    }

    TEST(SoftBodyAuthoring, GeneratesCompleteBindingAndPreservesModes)
    {
        const auto          input = Sphere();
        RigGenerationOutput output;
        const auto          status = GenerateRig(input, output);
        ASSERT_TRUE(status) << status.message;
        EXPECT_LE(output.mesh.vertices.size(), input.settings.maxControlPoints);
        EXPECT_EQ(output.binding.bones.size(), output.mesh.vertices.size() + 1);
        EXPECT_LE(output.diagnostics.maximumSurfaceDistance, input.settings.maxSurfaceDistance);
        EXPECT_LE(output.diagnostics.relativeVolumeExcess, input.settings.maxVolumeExcess);
        EXPECT_EQ(output.diagnostics.uncoveredTriangleCount, 0u);
        for (const auto &control : input.controls) {
            const auto found = std::find(output.controlIds.begin(), output.controlIds.end(), control.id);
            ASSERT_NE(found, output.controlIds.end());
            const auto &vertex = output.mesh.vertices[std::distance(output.controlIds.begin(), found)];
            EXPECT_EQ(vertex.position.x, control.position.x);
            EXPECT_EQ(vertex.position.y, control.position.y);
            EXPECT_EQ(vertex.position.z, control.position.z);
            EXPECT_EQ(vertex.mode, control.mode);
            EXPECT_EQ(vertex.inverseMass, 0.f);
        }
        VerifyBodyWeights(input, output);
        RigGenerationOutput again;
        ASSERT_TRUE(GenerateRig(input, again));
        EXPECT_EQ(again.binding.layoutSignature, output.binding.layoutSignature);
        EXPECT_EQ(again.controlIds, output.controlIds);
    }

    TEST(SoftBodyAuthoring, RequiresExplicitRigidAccessoryAttachment)
    {
        auto                        input = Sphere();
        phy::AuthoringMeshPrimitive eye;
        eye.key       = "eye";
        eye.positions = {{.3f, .02f, 0.f}, {.31f, .02f, 0.f}, {.3f, .03f, 0.f}};
        eye.indices   = {0, 1, 2};
        input.source.primitives.push_back(eye);
        RigGenerationOutput output;
        output.controlIds  = {"unchanged"};
        const auto missing = GenerateRig(input, output);
        EXPECT_EQ(missing.error, phy::PhysicsError::Unsupported);
        EXPECT_NE(missing.message.find("eye"), std::string::npos);
        EXPECT_EQ(output.controlIds, std::vector<std::string>{"unchanged"});
        input.attachments.push_back({"eye", "head"});
        const auto status = GenerateRig(input, output);
        ASSERT_TRUE(status) << status.message;
        ASSERT_EQ(output.binding.primitives.size(), 2u);
        const auto head = std::find(output.controlIds.begin(), output.controlIds.end(), "head");
        for (const auto &weight : output.binding.primitives[1].vertices) {
            EXPECT_EQ(weight.bones[0], std::distance(output.controlIds.begin(), head) + 1);
            EXPECT_EQ(weight.weights[0], 1.f);
            EXPECT_EQ(weight.weights[1], 0.f);
            EXPECT_EQ(weight.weights[2], 0.f);
            EXPECT_EQ(weight.weights[3], 0.f);
        }
    }

    TEST(SoftBodyAuthoring, InvalidInputsAndCancellationPreserveOutput)
    {
        auto                input = Sphere();
        RigGenerationOutput output;
        output.controlIds = {"sentinel"};
        std::atomic_bool cancelled{true};
        EXPECT_FALSE(GenerateRig(input, output, &cancelled));
        EXPECT_EQ(output.controlIds, std::vector<std::string>{"sentinel"});
        input.source.primitives[0].indices.resize(input.source.primitives[0].indices.size() - 3);
        EXPECT_EQ(GenerateRig(input, output).error, phy::PhysicsError::InvalidArgument);
        EXPECT_EQ(output.controlIds, std::vector<std::string>{"sentinel"});
        input                            = Sphere();
        input.settings.useExplicitCenter = true;
        input.settings.cageCenter        = {1.f, 0.f, 0.f};
        EXPECT_EQ(GenerateRig(input, output).error, phy::PhysicsError::Unsupported);
        input                = Sphere();
        input.controls[1].id = input.controls[0].id;
        EXPECT_EQ(GenerateRig(input, output).error, phy::PhysicsError::InvalidArgument);
        input                      = Sphere();
        input.controls[1].position = input.controls[0].position;
        EXPECT_EQ(GenerateRig(input, output).error, phy::PhysicsError::InvalidArgument);
        EXPECT_EQ(output.controlIds, std::vector<std::string>{"sentinel"});
    }

    TEST(SoftBodyAuthoring, ConfiguredBudgetsChangeTheGeneratedControlCount)
    {
        auto input                        = Sphere();
        input.settings.maxSurfaceDistance = .5f;
        input.settings.maxVolumeExcess    = 4.f;
        input.settings.maxControlPoints   = 20;
        RigGenerationOutput smaller;
        ASSERT_TRUE(GenerateRig(input, smaller));
        EXPECT_LE(smaller.mesh.vertices.size(), 20u);
        input.settings.maxControlPoints = 48;
        RigGenerationOutput larger;
        ASSERT_TRUE(GenerateRig(input, larger));
        EXPECT_LE(larger.mesh.vertices.size(), 48u);
        EXPECT_GT(larger.mesh.vertices.size(), smaller.mesh.vertices.size());
        VerifyBodyWeights(input, larger);
    }

    TEST(SoftBodyAuthoring, ExactBudgetsPreserveMandatoryControlsAndCompleteFourWeightBinding)
    {
        auto input                        = Sphere();
        input.controls[0].position        = {-.03f, .01f, .02f};
        input.settings.fillControlBudget  = true;
        input.settings.maxSurfaceDistance = .5f;
        input.settings.maxVolumeExcess    = 4.f;
        for (const uint32_t count : {30u, 25u, 20u, 15u, 10u}) {
            SCOPED_TRACE(count);
            input.settings.maxControlPoints = count;
            RigGenerationOutput output;
            const auto          status = GenerateRig(input, output);
            ASSERT_TRUE(status) << status.message;
            ASSERT_EQ(output.mesh.vertices.size(), count);
            EXPECT_EQ(output.diagnostics.controlPointCount, count);
            EXPECT_EQ(output.binding.bones.size(), count + 1);
            EXPECT_GE(output.diagnostics.minimumTetQuality, input.settings.minTetQuality);
            EXPECT_EQ(output.diagnostics.uncoveredTriangleCount, 0u);
            std::vector<bool> boundary(count, false), used(count, false);
            for (const auto &face : output.mesh.faces)
                for (const auto vertex : face)
                    boundary[vertex] = true;
            for (const auto &tet : output.mesh.tetrahedra)
                for (const auto vertex : tet.vertices)
                    used[vertex] = true;
            for (size_t i = 0; i < count; ++i) {
                EXPECT_TRUE(used[i]);
                if (i != 0 && output.controlIds[i].starts_with("auto:")) {
                    EXPECT_TRUE(boundary[i]);
                    EXPECT_EQ(output.mesh.vertices[i].mode, phy::SoftBodyAttachmentMode::Dynamic);
                }
            }
            for (const auto &control : input.controls) {
                const auto found = std::find(output.controlIds.begin(), output.controlIds.end(), control.id);
                ASSERT_NE(found, output.controlIds.end());
                const auto &vertex = output.mesh.vertices[std::distance(output.controlIds.begin(), found)];
                EXPECT_EQ(vertex.position, control.position);
                EXPECT_EQ(vertex.mode, control.mode);
            }
            VerifyBodyWeights(input, output);
            if (count == 10)
                EXPECT_EQ(output.diagnostics.algorithm, "adaptive-octa-cage-v1");
        }
    }

    TEST(SoftBodyAuthoring, ExactBudgetQualityAndCancellationFailuresPreserveCompleteOutput)
    {
        auto input                        = Sphere();
        input.settings.fillControlBudget  = true;
        input.settings.maxControlPoints   = 10;
        input.settings.maxSurfaceDistance = .5f;
        input.settings.maxVolumeExcess    = 4.f;
        RigGenerationOutput output;
        const auto          status = GenerateRig(input, output);
        ASSERT_TRUE(status) << status.message;
        const auto signature         = output.binding.layoutSignature;
        const auto ids               = output.controlIds;
        const auto first             = output.mesh.vertices.front().position;
        input.settings.minTetQuality = 1.f;
        EXPECT_EQ(GenerateRig(input, output).error, phy::PhysicsError::CapacityExceeded);
        EXPECT_EQ(output.binding.layoutSignature, signature);
        EXPECT_EQ(output.controlIds, ids);
        EXPECT_EQ(output.mesh.vertices.front().position, first);
        EXPECT_EQ(output.mesh.vertices.size(), 10u);
        std::atomic_bool cancelled{true};
        EXPECT_FALSE(GenerateRig(input, output, &cancelled));
        EXPECT_EQ(output.binding.layoutSignature, signature);
        EXPECT_EQ(output.controlIds, ids);
    }

    TEST(SoftBodyAuthoring, LowBudgetOctahedronRemainsAnUpperBoundWhenFillingIsDisabled)
    {
        auto input = Sphere();
        EXPECT_FALSE(input.settings.fillControlBudget);
        input.settings.useExplicitCenter  = true;
        input.settings.cageCenter         = {};
        input.settings.maxControlPoints   = 10;
        input.settings.maxSurfaceDistance = .5f;
        input.settings.maxVolumeExcess    = 4.f;
        RigGenerationOutput output;
        const auto          status = GenerateRig(input, output);
        ASSERT_TRUE(status) << status.message;
        EXPECT_LT(output.mesh.vertices.size(), 10u);
        EXPECT_EQ(output.diagnostics.algorithm, "adaptive-octa-cage-v1");
        VerifyBodyWeights(input, output);
    }

    TEST(SoftBodyAuthoring, RefusesInsufficientDistanceBudget)
    {
        auto input = Sphere();
        input.controls.clear();
        input.settings.maxControlPoints   = 13;
        input.settings.maxSurfaceDistance = .00001f;
        RigGenerationOutput output;
        output.controlIds = {"sentinel"};
        const auto status = GenerateRig(input, output);
        EXPECT_EQ(status.error, phy::PhysicsError::CapacityExceeded) << status.message;
        EXPECT_EQ(output.controlIds, std::vector<std::string>{"sentinel"});
    }

    TEST(SoftBodyAuthoring, RejectsClosedNonStarTorus)
    {
        RigGenerationInput input;
        input.source.key                = "torus";
        input.bodyPrimitive             = "body";
        input.settings.maxControlPoints = 30;
        phy::AuthoringMeshPrimitive primitive;
        primitive.key            = "body";
        constexpr uint32_t major = 24, minor = 12;
        for (uint32_t i = 0; i < major; ++i)
            for (uint32_t j = 0; j < minor; ++j) {
                const float a = 2.f * PI * i / major, b = 2.f * PI * j / minor, radius = .3f + .1f * std::cos(b);
                primitive.positions.push_back({radius * std::cos(a), .1f * std::sin(b), radius * std::sin(a)});
            }
        for (uint32_t i = 0; i < major; ++i)
            for (uint32_t j = 0; j < minor; ++j) {
                const uint32_t a = i * minor + j, b = ((i + 1) % major) * minor + j, c = i * minor + (j + 1) % minor,
                               d = ((i + 1) % major) * minor + (j + 1) % minor;
                primitive.indices.insert(primitive.indices.end(), {a, b, c, b, d, c});
            }
        input.source.primitives.push_back(std::move(primitive));
        RigGenerationOutput output;
        EXPECT_EQ(GenerateRig(input, output).error, phy::PhysicsError::Unsupported);
    }

    TEST(SoftBodyAuthoring, VerifiedSealArtifact)
    {
        const char *path = std::getenv("SKY_SOFTBODY_SEAL_FIXTURE");
        if (!path)
            GTEST_SKIP() << "Optional external original seal fixture";
        std::ifstream stream(path);
        ASSERT_TRUE(stream.good());
        uint32_t vertices = 0, triangles = 0;
        stream >> vertices >> triangles;
        ASSERT_GT(vertices, 0u);
        ASSERT_LT(vertices, 100000u);
        ASSERT_GT(triangles, 0u);
        ASSERT_LT(triangles, 200000u);
        RigGenerationInput input;
        input.source.key                  = "seal-profile";
        input.bodyPrimitive               = "body";
        input.settings.maxSurfaceDistance = .12f;
        input.settings.maxVolumeExcess    = .50f;
        input.settings.useExplicitCenter  = true;
        input.settings.cageCenter         = {-.10f, .12f, 0.f};
        const char *budgetText            = std::getenv("SKY_SOFTBODY_SEAL_BUDGET");
        if (budgetText) {
            const std::string_view text(budgetText);
            const auto             parsed = std::from_chars(text.data(), text.data() + text.size(), input.settings.maxControlPoints);
            ASSERT_EQ(parsed.ec, std::errc{});
            ASSERT_EQ(parsed.ptr, text.data() + text.size());
            input.settings.fillControlBudget  = true;
            input.settings.maxVolumeExcess    = 10.f;
            input.settings.maxSurfaceDistance = 5.f;
        }
        phy::AuthoringMeshPrimitive primitive;
        primitive.key = "body";
        primitive.positions.resize(vertices);
        primitive.indices.resize(triangles * 3);
        for (auto &position : primitive.positions)
            stream >> position.x >> position.y >> position.z;
        for (auto &index : primitive.indices)
            stream >> index;
        ASSERT_TRUE(stream.good());
        input.source.primitives.push_back(std::move(primitive));
        input.controls = {{"root-anchor", {0.f, .20f, 0.f}, phy::SoftBodyAttachmentMode::Dynamic},
                          {"head-anchor", {.25f, .28f, 0.f}, phy::SoftBodyAttachmentMode::Dynamic}};
        RigGenerationOutput output;
        const auto          status = GenerateRig(input, output);
        ASSERT_TRUE(status) << status.message;
        VerifyBodyWeights(input, output);
        EXPECT_LE(output.mesh.vertices.size(), input.settings.maxControlPoints);
        EXPECT_LE(output.diagnostics.relativeVolumeExcess, input.settings.maxVolumeExcess);
        EXPECT_LE(output.diagnostics.maximumSurfaceDistance, input.settings.maxSurfaceDistance);
        if (budgetText) {
            EXPECT_EQ(output.mesh.vertices.size(), input.settings.maxControlPoints);
        } else {
            for (const auto gap : output.diagnostics.axisSupportGaps)
                EXPECT_NEAR(gap, 0., 1e-5);
        }
        std::cout << "Seal";
        PrintQuality(input, output);
        if (const char *reportPath = std::getenv("SKY_SOFTBODY_CAGE_REPORT")) {
            std::ofstream report(reportPath);
            ASSERT_TRUE(report.good());
            report << std::setprecision(17) << output.mesh.vertices.size() << ' ' << output.mesh.tetrahedra.size() << ' ' << output.mesh.faces.size()
                   << '\n';
            for (const auto &vertex : output.mesh.vertices)
                report << vertex.position.x << ' ' << vertex.position.y << ' ' << vertex.position.z << '\n';
            for (const auto &tet : output.mesh.tetrahedra)
                report << tet.vertices[0] << ' ' << tet.vertices[1] << ' ' << tet.vertices[2] << ' ' << tet.vertices[3] << '\n';
            for (const auto &face : output.mesh.faces)
                report << face[0] << ' ' << face[1] << ' ' << face[2] << '\n';
            ASSERT_TRUE(report.good());
        }
    }

    TEST(SoftBodyAuthoring, AnisotropicSourcePreservesCompleteEmbeddingAtTheSameBudget)
    {
        auto input = Sphere();
        for (auto &point : input.source.primitives[0].positions) {
            point.x *= 3.f;
            point.y *= .5f;
            point.z *= 1.1f;
        }
        for (auto &point : input.controls) {
            point.position.x *= 3.f;
            point.position.y *= .5f;
            point.position.z *= 1.1f;
        }
        input.settings.maxVolumeExcess    = .36f;
        input.settings.maxSurfaceDistance = .09f;
        RigGenerationOutput output;
        const auto          status = GenerateRig(input, output);
        ASSERT_TRUE(status) << status.message;
        EXPECT_LE(output.mesh.vertices.size(), 30u);
        EXPECT_LE(output.diagnostics.relativeVolumeExcess, .36);
        EXPECT_LE(output.diagnostics.maximumSurfaceDistance, .09);
        for (const auto gap : output.diagnostics.axisSupportGaps)
            EXPECT_NEAR(gap, 0., 1e-5);
        VerifyBodyWeights(input, output);
        std::cout << "Ellipsoid";
        PrintQuality(input, output);
    }
} // namespace sky::softbody
