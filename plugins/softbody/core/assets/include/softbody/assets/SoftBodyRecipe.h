#pragma once

#include <softbody/assets/SoftBodyAsset.h>
#include <softbody/authoring/SoftBodyRigGenerator.h>

namespace sky::softbody {
    inline constexpr const char *RIG_GENERATOR_ID     = "softbody.adaptive-star.v1";
    inline constexpr const char *RIG_RECIPE_EXTENSION = "softbodyrig";

    struct RigMaterialSettings {
        bool     enableRootFollow = true;
        float    followCompliance = .02f;
        float    particleMass     = .1f;
        float    edgeCompliance   = .00001f;
        float    volumeCompliance = .000000001f;
        float    pressure         = 0.f;
        float    gravityFactor    = 1.f;
        float    linearDamping    = .1f;
        float    vertexRadius     = .005f;
        float    friction         = .3f;
        float    restitution      = 0.f;
        uint32_t solverIterations = 8;
    };

    struct RigAuthoringSettings {
        std::string             bodyPrimitive;
        RigGeneratorSettings    generator;
        RigMaterialSettings     material;
        std::vector<Attachment> attachments;
        SoftBodyNormalPolicy    normalPolicy = SoftBodyNormalPolicy::TetGradient;
        std::vector<Uuid>       materialSlots;
    };

    struct SoftBodyRecipe {
        uint32_t                  version = 2;
        Uuid                      source;
        std::string               sourceKey;
        std::vector<ControlPoint> controls;
        RigAuthoringSettings      settings;
    };

    phy::PhysicsStatus ValidateSoftBodyRecipe(const SoftBodyRecipe &recipe);
    phy::PhysicsStatus ParseSoftBodyRecipe(std::string_view text, SoftBodyRecipe &out);
    phy::PhysicsStatus WriteSoftBodyRecipe(const SoftBodyRecipe &recipe, std::string &out);
    phy::PhysicsStatus ConfigureRigPhysics(const RigMaterialSettings &settings, const phy::SoftBodyMesh &mesh, phy::SoftBodyDesc &out);
    void               RegisterSoftBodyRecipeTypes();
} // namespace sky::softbody
