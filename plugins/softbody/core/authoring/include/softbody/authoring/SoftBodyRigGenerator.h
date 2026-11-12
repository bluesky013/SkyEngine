#pragma once

#include <physics/softbody/IMeshAuthoringCompiler.h>
#include <physics/softbody/SoftBodyTypes.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace sky::softbody {

    inline constexpr uint32_t MIN_RIG_CONTROL_POINTS = 7;
    inline constexpr uint32_t MAX_RIG_CONTROL_POINTS = phy::AUTHORING_MAX_BONES - 1;

    struct ControlPoint {
        std::string                 id;
        Vector3                     position = VEC3_ZERO;
        phy::SoftBodyAttachmentMode mode     = phy::SoftBodyAttachmentMode::Dynamic;
    };

    enum class RigGenerationMode : uint32_t { AutoFit = 0, SuppliedCenterFan, END };
    enum class AttachmentBindingMode : uint32_t { Control = 0, BindingAnchor, END };

    struct SuppliedTetrahedron {
        uint32_t a = 0, b = 0, c = 0, d = 0;
    };

    struct Attachment {
        std::string           primitiveKey;
        std::string           controlId;
        AttachmentBindingMode mode          = AttachmentBindingMode::Control;
        Vector3               bindingAnchor = VEC3_ZERO;
    };

    struct RigGeneratorSettings {
        uint32_t                         maxControlPoints   = 30;
        bool                             fillControlBudget  = false;
        float                            maxSurfaceDistance = .16f;
        float                            maxVolumeExcess    = .85f;
        float                            minTetQuality      = 1e-5f;
        bool                             useExplicitCenter  = false;
        Vector3                          cageCenter         = VEC3_ZERO;
        RigGenerationMode                mode               = RigGenerationMode::AutoFit;
        uint32_t                         centerIndex        = 0;
        std::vector<SuppliedTetrahedron> tetrahedra;
        float                            bindingPositionScale = 1.f;
    };

    struct RigGenerationInput {
        phy::AuthoringMeshSource  source;
        std::string               bodyPrimitive;
        std::vector<ControlPoint> controls;
        std::vector<Attachment>   attachments;
        RigGeneratorSettings      settings;
    };

    struct RigGenerationDiagnostics {
        std::string algorithm;
        uint32_t    sourceVertexCount             = 0;
        uint32_t    sourceTriangleCount           = 0;
        uint32_t    controlPointCount             = 0;
        uint32_t    tetrahedronCount              = 0;
        uint32_t    uncoveredTriangleCount        = 0;
        double      sourceVolume                  = 0.;
        double      cageVolume                    = 0.;
        double      relativeVolumeExcess          = 0.;
        double      maximumSampledSurfaceDistance = 0.;
        // Certified upper bound from adaptive surface subdivision and distance Lipschitz continuity.
        double maximumSurfaceDistance         = 0.;
        double minimumTetQuality              = 0.;
        double maximumRestReconstructionError = 0.;
        double maximumBindingPositionError    = 0.;
        float  bindingPositionScale           = 1.f;
        bool   coverageInBindingSpace         = false;
        // Rest-geometry support offsets for -X, +X, -Y, +Y, -Z, +Z; collision radius is separate.
        std::array<double, 6> axisSupportGaps{};
    };

    struct RigGenerationOutput {
        phy::SoftBodyMesh         mesh;
        phy::AuthoringMeshBinding binding;
        std::vector<std::string>  controlIds;
        RigGenerationDiagnostics  diagnostics;
    };

    // Body vertices use tetrahedral barycentrics; every other primitive needs an explicit rigid attachment.
    // Failure or cancellation leaves out unchanged. The source, controls and center share baked model space.
    phy::PhysicsStatus GenerateRig(const RigGenerationInput &input, RigGenerationOutput &out, const std::atomic_bool *cancel = nullptr);

} // namespace sky::softbody
