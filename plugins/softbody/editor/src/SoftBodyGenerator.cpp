#include <softbody/editor/SoftBodyGenerator.h>

#include <aurora/editor/MeshGeneratorDocumentContract.h>
#include <framework/serialization/SerializationContext.h>
#include <softbody/assets/SoftBodyRecipe.h>

namespace sky::softbody {
    namespace {
        using namespace sky::editor;
        using namespace sky::aurora::editor;

        class RigResult final : public IAssetGeneratorPayload, public IMeshGeneratorOutput {
        public:
            const void *QueryInterface(const Uuid &id) const override
            {
                return id == MESH_GENERATOR_OUTPUT_INTERFACE ? static_cast<const IMeshGeneratorOutput *>(this) : nullptr;
            }
            const MeshGeneratorOutputData &GetMeshOutput() const override
            {
                return output;
            }
            MeshGeneratorOutputData output;
        };

        class Generator final : public IAssetGenerator {
        public:
            AssetGeneratorDescriptor Describe() const override
            {
                return {RIG_GENERATOR_ID,
                        "Softbody: fitted control cage",
                        {"AuroraMesh"},
                        {"mesh-binding", "control-points", "four-influences"},
                        GetTypeNode(TypeInfo<RigAuthoringSettings>::RegisteredId())};
            }
            AssetGeneratorStatus CreateSettings(Any &out) const override
            {
                out = Any(RigAuthoringSettings{});
                return {};
            }
            AssetGeneratorStatus CheckSupport(const AssetGeneratorContext &context) const override
            {
                return context.inputType == "AuroraMesh"
                           ? AssetGeneratorStatus{}
                           : AssetGeneratorStatus{AssetGeneratorCode::Unsupported, "Softbody binding requires an Aurora mesh"};
            }
            AssetGenerationOutput Generate(const AssetGenerationInput &input, AssetGenerationControl &control) const override
            {
                const auto *settings = input.settings.GetAsConst<RigAuthoringSettings>();
                const auto *contract = input.payload ? input.payload->QueryInterface(MESH_GENERATOR_INPUT_INTERFACE) : nullptr;
                if (!settings || !contract) {
                    return {{AssetGeneratorCode::InvalidArgument, "Mesh authoring input or softbody settings are missing"}, {}};
                }
                const auto    &source = static_cast<const IMeshGeneratorInput *>(contract)->GetMeshInput();
                SoftBodyRecipe recipe;
                recipe.source    = source.sourceOwner;
                recipe.sourceKey = source.sourceKey;
                recipe.settings  = *settings;
                if (recipe.settings.bodyPrimitive.empty() && source.mesh.primitives.size() == 1) {
                    recipe.settings.bodyPrimitive = source.mesh.primitives.front().key;
                }
                if (recipe.settings.bodyPrimitive.empty()) {
                    return {{AssetGeneratorCode::InvalidSettings, "Select the body primitive and explicitly attach every other primitive"}, {}};
                }
                for (const auto &point : source.controls) {
                    ControlPoint converted;
                    converted.id       = point.id;
                    converted.position = point.position;
                    switch (point.mode) {
                    case MeshControlMode::Free: converted.mode = phy::SoftBodyAttachmentMode::Dynamic; break;
                    case MeshControlMode::WorldFixed: converted.mode = phy::SoftBodyAttachmentMode::WorldFixed; break;
                    case MeshControlMode::RootDriven: converted.mode = phy::SoftBodyAttachmentMode::RootKinematic; break;
                    default: return {{AssetGeneratorCode::InvalidArgument, "Invalid control point mode"}, {}};
                    }
                    recipe.controls.push_back(std::move(converted));
                }
                std::string recipeText;
                if (const auto status = WriteSoftBodyRecipe(recipe, recipeText); !status) {
                    return {{AssetGeneratorCode::InvalidSettings, status.message}, {}};
                }
                if (const auto status = control.ReportProgress(.05f, "Validating mesh and constrained controls"); !status) {
                    return {status, {}};
                }
                RigGenerationInput request;
                request.source        = source.mesh;
                request.bodyPrimitive = recipe.settings.bodyPrimitive;
                request.controls      = recipe.controls;
                request.attachments   = recipe.settings.attachments;
                request.settings      = recipe.settings.generator;
                RigGenerationOutput rig;
                const auto          generated = GenerateRig(request, rig, &control.CancellationFlag());
                if (control.IsCancellationRequested()) {
                    return {{AssetGeneratorCode::Cancelled, "Softbody generation cancelled"}, {}};
                }
                if (!generated) {
                    return {{AssetGeneratorCode::Failed, generated.message}, {}};
                }
                rig.binding.materialSlots = recipe.settings.materialSlots;
                phy::SoftBodyDesc body;
                if (const auto status = ConfigureRigPhysics(recipe.settings.material, rig.mesh, body); !status) {
                    return {{AssetGeneratorCode::InvalidSettings, status.message}, {}};
                }
                const auto result  = std::make_shared<RigResult>();
                auto      &output  = result->output;
                output.sourceOwner = source.sourceOwner;
                output.sourceKey   = source.sourceKey;
                output.binding     = std::move(rig.binding);
                output.controls    = source.controls;
                for (const auto &point : rig.mesh.vertices) {
                    output.cagePoints.push_back(point.position);
                }
                for (const auto &tet : rig.mesh.tetrahedra) {
                    output.tetrahedra.push_back(tet.vertices);
                }
                output.recipeText      = std::move(recipeText);
                output.recipeExtension = RIG_RECIPE_EXTENSION;
                output.diagnostics     = {{"Control points", std::to_string(rig.mesh.vertices.size())},
                                          {"Tetrahedra", std::to_string(rig.mesh.tetrahedra.size())},
                                          {"Cage volume excess", std::to_string(rig.diagnostics.relativeVolumeExcess * 100.) + "%"},
                                          {"Sampled cage-to-mesh gap (m)", std::to_string(rig.diagnostics.maximumSampledSurfaceDistance)},
                                          {"Cage-to-mesh distance upper bound (m)", std::to_string(rig.diagnostics.maximumSurfaceDistance)},
                                          {"Rest -Y support offset (m)", std::to_string(rig.diagnostics.axisSupportGaps[2])},
                                          {"Rest reconstruction error (m)", std::to_string(rig.diagnostics.maximumRestReconstructionError)},
                                          {"Binding position error (m)", std::to_string(rig.diagnostics.maximumBindingPositionError)},
                                          {"Coverage domain", rig.diagnostics.coverageInBindingSpace ? "Scaled binding positions" : "Display positions"},
                                          {"Binding position scale", std::to_string(rig.diagnostics.bindingPositionScale)}};
                if (const auto status = control.ReportProgress(1.f, "Complete binding candidate"); !status) {
                    return {status, {}};
                }
                return {{}, result};
            }
        };

        class RecipeCodec final : public IMeshGeneratorRecipeCodec {
        public:
            AssetGeneratorStatus Decode(std::span<const uint8_t> bytes, MeshGeneratorRecipeState &out) const override
            {
                SoftBodyRecipe recipe;
                const auto     status = ParseSoftBodyRecipe({reinterpret_cast<const char *>(bytes.data()), bytes.size()}, recipe);
                if (!status) {
                    return {AssetGeneratorCode::InvalidArgument, status.message};
                }
                MeshGeneratorRecipeState candidate;
                candidate.sourceOwner = recipe.source;
                candidate.sourceKey   = recipe.sourceKey;
                candidate.generatorId = RIG_GENERATOR_ID;
                candidate.settings    = Any(recipe.settings);
                for (const auto &point : recipe.controls) {
                    MeshControlPoint converted;
                    converted.id       = point.id;
                    converted.position = point.position;
                    switch (point.mode) {
                    case phy::SoftBodyAttachmentMode::Dynamic: converted.mode = MeshControlMode::Free; break;
                    case phy::SoftBodyAttachmentMode::WorldFixed: converted.mode = MeshControlMode::WorldFixed; break;
                    case phy::SoftBodyAttachmentMode::RootKinematic: converted.mode = MeshControlMode::RootDriven; break;
                    default: return {AssetGeneratorCode::InvalidArgument, "Invalid recipe control mode"};
                    }
                    candidate.controls.push_back(std::move(converted));
                }
                out = std::move(candidate);
                return {};
            }
        };
    } // namespace

    std::shared_ptr<editor::IAssetGenerator> CreateSoftBodyGenerator()
    {
        return std::make_shared<Generator>();
    }
    std::shared_ptr<aurora::editor::IMeshGeneratorRecipeCodec> CreateSoftBodyRecipeCodec()
    {
        return std::make_shared<RecipeCodec>();
    }
} // namespace sky::softbody
