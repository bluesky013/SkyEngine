#include <aurora/editor/MeshGeneratorDocument.h>

#include "MeshMaterialPreview.h"
#include "MeshPreviewSurface.h"

#include <core/async/ThreadPool.h>
#include <editor/core/EditorCore.h>
#include <editor/core/asset/AssetCookService.h>
#include <editor/core/asset/AssetMutationService.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/viewport/EditorCamera.h>
#include <editor/ui/UiDraw.h>
#include <editor/widgets/AssetPreviewCameraInput.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/UiSkin.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <set>
#include <unordered_set>

namespace sky::aurora::editor {
    using namespace sky::editor;
    using namespace sky::ui;

    namespace {
        struct MeshDocumentViewSettings {
            std::string savePath;
            bool        showCage        = false;
            bool        showBones       = true;
            bool        showWeights     = false;
            uint32_t    weightBone      = 1;
            bool        showDiagnostics = false;
        };
        struct EditState {
            uint64_t                                      id = 1;
            std::string                                   sourceKey;
            AssetGeneratorSnapshot                        generator;
            Any                                           settings;
            std::vector<MeshControlPoint>                 controls;
            std::shared_ptr<const IAssetGeneratorPayload> candidate;
            bool                                          candidateCurrent = false;
        };
        struct SourceStamp {
            std::string                     path;
            std::filesystem::file_time_type time;
            uintmax_t                       size                                  = 0;
            bool                            operator==(const SourceStamp &) const = default;
        };
        bool ReadStamp(const Uuid &owner, SourceStamp &out)
        {
            if (!AssetDataBase::Get()->GetAbsoluteSourcePath(owner, out.path)) {
                return false;
            }
            std::error_code error;
            out.time = std::filesystem::last_write_time(out.path, error);
            if (error) {
                return false;
            }
            out.size = std::filesystem::file_size(out.path, error);
            return !error;
        }
        bool Finite(const Vector3 &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }
        AssetGeneratorStatus ValidateControls(const std::vector<MeshControlPoint> &controls)
        {
            std::unordered_set<std::string> ids;
            for (const auto &control : controls) {
                if (control.id.empty() || !ids.insert(control.id).second || !Finite(control.position) || control.mode < MeshControlMode::Free ||
                    control.mode >= MeshControlMode::END) {
                    return {AssetGeneratorCode::InvalidArgument, "Control IDs must be unique and positions/modes must be valid"};
                }
            }
            return {};
        }
        const MeshGeneratorOutputData *Output(const std::shared_ptr<const IAssetGeneratorPayload> &payload)
        {
            const auto *interface =
                payload ? static_cast<const IMeshGeneratorOutput *>(payload->QueryInterface(MESH_GENERATOR_OUTPUT_INTERFACE)) : nullptr;
            return interface ? &interface->GetMeshOutput() : nullptr;
        }
        AssetGeneratorStatus ValidateOutput(const MeshGeneratorInputData &input, const MeshGeneratorOutputData &value)
        {
            const auto invalid = [](const char *message) { return AssetGeneratorStatus{AssetGeneratorCode::Failed, message}; };
            if (value.sourceOwner != input.sourceOwner || value.sourceKey != input.sourceKey || value.controls != input.controls ||
                value.binding.bones.empty() || value.binding.bones.size() > 256 || !value.binding.layoutSignature ||
                value.binding.primitives.size() != input.mesh.primitives.size() || value.recipeText.empty() || value.recipeExtension.empty() ||
                !std::all_of(value.recipeExtension.begin(), value.recipeExtension.end(),
                             [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); })) {
                return invalid("Generator result has an incompatible source, binding or recipe");
            }
            for (const auto *extension : {"fbx", "gltf", "glb", "obj", "dae"}) {
                if (value.recipeExtension == extension) {
                    return invalid("Generated recipes cannot overwrite imported mesh formats");
                }
            }
            std::unordered_set<std::string> boneNames;
            for (size_t i = 0; i < value.binding.bones.size(); ++i) {
                const auto &bone = value.binding.bones[i];
                if (bone.name.empty() || !boneNames.insert(bone.name).second || (bone.parent != UINT16_MAX && bone.parent >= i) ||
                    !Finite(bone.reference.translation) || !Finite(bone.reference.scale)) {
                    return invalid("Generated skeleton hierarchy is invalid");
                }
            }
            for (const auto &primitive : input.mesh.primitives) {
                const auto binding = std::find_if(value.binding.primitives.begin(), value.binding.primitives.end(),
                                                  [&](const auto &entry) { return entry.key == primitive.key; });
                if (binding == value.binding.primitives.end() || binding->vertices.size() != primitive.positions.size()) {
                    return invalid("Generated binding does not cover every source vertex");
                }
                for (const auto &vertex : binding->vertices) {
                    float sum = 0.f;
                    for (uint32_t i = 0; i < 4; ++i) {
                        if (!std::isfinite(vertex.weights[i]) || vertex.weights[i] < 0.f || vertex.bones[i] >= value.binding.bones.size()) {
                            return invalid("Generated skin has invalid four-influence weights");
                        }
                        sum += vertex.weights[i];
                    }
                    if (std::abs(sum - 1.f) > 0.0001f) {
                        return invalid("Generated skin weights are not normalized");
                    }
                }
            }
            if (!std::all_of(value.cagePoints.begin(), value.cagePoints.end(), Finite)) {
                return invalid("Generated cage contains nonfinite points");
            }
            for (const auto &tet : value.tetrahedra) {
                if (std::any_of(tet.begin(), tet.end(), [&](uint32_t i) { return i >= value.cagePoints.size(); })) {
                    return invalid("Generated cage references a missing point");
                }
                const auto &a      = value.cagePoints[tet[0]];
                const float volume = (value.cagePoints[tet[1]] - a).Dot((value.cagePoints[tet[2]] - a).Cross(value.cagePoints[tet[3]] - a));
                if (!std::isfinite(volume) || volume <= 0.f) {
                    return invalid("Generated cage contains an inverted or degenerate tetrahedron");
                }
            }
            return {};
        }
        class InputPayload final : public IAssetGeneratorPayload, public IMeshGeneratorInput {
        public:
            explicit InputPayload(MeshGeneratorInputData value) : value(std::move(value))
            {
            }
            const void *QueryInterface(const Uuid &id) const override
            {
                return id == MESH_GENERATOR_INPUT_INTERFACE ? static_cast<const IMeshGeneratorInput *>(this) : nullptr;
            }
            const MeshGeneratorInputData &GetMeshInput() const override
            {
                return value;
            }

        private:
            MeshGeneratorInputData value;
        };
        class StateCommand final : public UndoCommand {
        public:
            StateCommand(std::string name, EditState before, EditState after, std::function<void(const EditState &)> apply)
                : UndoCommand(std::move(name)), before(std::move(before)), after(std::move(after)), apply(std::move(apply))
            {
            }
            void Do() override
            {
                apply(after);
            }
            void Undo() override
            {
                apply(before);
            }

        private:
            EditState                              before, after;
            std::function<void(const EditState &)> apply;
        };

        class MeshPanel;
        class MeshViewport;
        class Document final : public IMeshGeneratorDocument {
        public:
            explicit Document(Uuid asset) : asset(asset), previewAsset(asset)
            {
                history.AddBeforeHistoryCallback([this] { CancelInteractions(); });
            }
            ~Document() override
            {
                lifetime.reset();
                CancelGeneration();
                if (request)
                    request->Wait();
                for (const auto &id : actionIds)
                    EditorActionRegistry::Get()->Remove(scope, id);
                for (const auto &id : generatorActionIds)
                    EditorActionRegistry::Get()->Remove(scope, id);
            }
            const MeshGeneratorInputData *Source() const override
            {
                return loaded ? &input : nullptr;
            }
            const MeshGeneratorOutputData *Generated() const override
            {
                return Output(state.candidate);
            }
            const Any &Settings() const override
            {
                return state.settings;
            }
            std::string GeneratorId() const override
            {
                return state.generator.identity.id;
            }
            Uuid SavedOwner() const override
            {
                return savedOwner;
            }
            Uuid MeshAsset() const
            {
                return previewAsset;
            }
            void SetSavePath(std::string path) override
            {
                viewSettings.savePath = std::move(path);
            }
            void SetActionScope(std::string value) override
            {
                scope = std::move(value);
            }
            CommandService &History() override
            {
                return history;
            }
            bool IsDirty() const override
            {
                return state.id != savedState;
            }
            bool IsSavePending() const override
            {
                return savePending;
            }
            void RequestSave() override
            {
                savePending = true;
            }
            std::string Status() const override
            {
                return diagnostic;
            }
            bool FitsContentToViewport() const override
            {
                return true;
            }
            bool                       IsEditing() const override;
            void                       CancelInteractions() override;
            std::unique_ptr<UIElement> CreateContent(UITextSystem *text) override;
            std::unique_ptr<UIElement> CreatePreview(UITextSystem *text) override;
            void                       Tick(float) override;

            AssetGeneratorStatus SelectGenerator(const std::string &id) override
            {
                const auto next = AssetGeneratorRegistry::Get()->Find(id);
                Any        settings;
                if (const auto status = AssetGeneratorRegistry::Get()->CheckSupport(next, Context()); !status)
                    return status;
                if (const auto status = AssetGeneratorRegistry::Get()->CreateSettings(next, settings); !status)
                    return status;
                auto changed             = state;
                changed.generator        = next;
                changed.settings         = std::move(settings);
                changed.candidateCurrent = false;
                ApplyEdit("Select mesh generator", std::move(changed));
                return {};
            }
            AssetGeneratorStatus SetControls(std::vector<MeshControlPoint> controls) override
            {
                if (const auto status = ValidateControls(controls); !status)
                    return status;
                auto changed             = state;
                changed.controls         = std::move(controls);
                changed.candidateCurrent = false;
                ApplyEdit("Edit control points", std::move(changed));
                return {};
            }
            AssetGeneratorStatus SetSettings(const Any &settings) override
            {
                if (!settings || GetTypeNode(settings) != state.generator.descriptor.settingsType) {
                    return {AssetGeneratorCode::InvalidSettings, "Settings do not match the selected generator"};
                }
                auto changed             = state;
                changed.settings         = settings;
                changed.candidateCurrent = false;
                ApplyEdit("Edit generator settings", std::move(changed));
                return {};
            }
            AssetGeneratorStatus RequestGenerate() override
            {
                if (!loaded || !sourceCurrent || request || generationPending) {
                    return {AssetGeneratorCode::Unavailable, "Mesh source is unavailable or generation is already running"};
                }
                if (const auto status = AssetGeneratorRegistry::Get()->CheckSupport(state.generator, Context()); !status)
                    return status;
                generationPending = true;
                diagnostic        = "Generation queued";
                return {};
            }
            void CancelGeneration() override
            {
                generationPending = false;
                restoringBinding  = false;
                if (request)
                    request->Cancel();
            }
            bool Busy() const
            {
                return generationPending || request != nullptr;
            }
            uint64_t Revision() const
            {
                return revision;
            }
            std::weak_ptr<void> Lifetime() const
            {
                return lifetime;
            }
            const EditState &State() const
            {
                return state;
            }
            MeshDocumentViewSettings &ViewSettings()
            {
                return viewSettings;
            }
            EditorCamera &Camera()
            {
                return camera;
            }
            const std::string &Selected() const
            {
                return selected;
            }
            void Select(std::string id)
            {
                selected = std::move(id);
                ++viewRevision;
            }
            uint64_t ViewRevision() const
            {
                return viewRevision;
            }
            void Notice(AssetGeneratorStatus status)
            {
                if (!status)
                    diagnostic = std::move(status.message);
            }
            UndoCommandPtr MakeEdit(std::string name, EditState changed)
            {
                changed.id                      = ++nextState;
                const std::weak_ptr<void> alive = lifetime;
                return std::make_unique<StateCommand>(std::move(name), state, std::move(changed), [this, alive](const EditState &value) {
                    if (alive.expired())
                        return;
                    CancelGeneration();
                    const bool sourceChanged = value.sourceKey != input.sourceKey;
                    if (sourceChanged)
                        previewAsset = SelectedMeshProduct(asset, openedSourceKey, input.sourceOwner, value.sourceKey);
                    state           = value;
                    input.controls  = state.controls;
                    input.sourceKey = state.sourceKey;
                    if (sourceChanged) {
                        const auto source =
                            std::find_if(sources.begin(), sources.end(), [&](const auto &item) { return item.key == state.sourceKey; });
                        if (source != sources.end())
                            input.mesh = *source;
                        Focus();
                    }
                    ++revision;
                    ++viewRevision;
                    diagnostic = state.candidateCurrent ? "Complete binding preview; save to cook" : "Inputs changed; generate a new binding";
                });
            }
            void ApplyEdit(std::string name, EditState changed)
            {
                history.Execute(MakeEdit(std::move(name), std::move(changed)));
            }
            void Focus();
            void FitPreview(Vector2 extent)
            {
                if (framePending && subjectBounds && FrameAssetPreview(camera, *subjectBounds, extent))
                    framePending = false;
            }
            const std::optional<AABB> &SubjectBounds() const
            {
                return subjectBounds;
            }
            void UseProductBounds(const std::vector<AABB> &bounds)
            {
                if (!subjectBounds && !bounds.empty()) {
                    subjectBounds = bounds.front();
                    for (size_t i = 1; i < bounds.size(); ++i)
                        subjectBounds->Merge(bounds[i]);
                }
            }
            void AddControl();
            void RemoveControl();
            void RegisterActions();
            void Save();

        private:
            AssetGeneratorContext Context() const
            {
                return {input.sourceOwner, document, "AuroraMesh", revision, {"mesh-binding"}};
            }
            void                                    Load();
            void                                    RefreshGeneratorActions();
            Uuid                                    asset, previewAsset, document = Uuid::Create(), savedOwner;
            std::string                             openedSourceKey;
            EditState                               state;
            MeshGeneratorInputData                  input;
            std::vector<phy::AuthoringMeshSource>   sources;
            SourceStamp                             sourceStamp, savedStamp;
            MeshDocumentViewSettings                viewSettings;
            EditorCamera                            camera = MakeAssetPreviewCamera();
            std::optional<AABB>                     subjectBounds;
            bool                                    framePending = true;
            CommandService                          history;
            ThreadPool                              pool{1};
            std::shared_ptr<AssetGenerationRequest> request;
            std::shared_ptr<void>                   lifetime = std::make_shared<int>(0);
            std::string                             scope = "assetViewer", selected, diagnostic = "Loading mesh source";
            std::vector<std::string>                actionIds, generatorActionIds;
            std::vector<AssetGeneratorIdentity>     listedGenerators;
            uint64_t                                listedRegistryRevision = 0, listedInputRevision = 0;
            std::chrono::steady_clock::time_point   nextSourceCheck{};
            UIElement::InputReference               panel, viewport;
            uint64_t                                revision = 1, nextState = 1, savedState = 1, viewRevision = 0;
            bool loaded = false, sourceCurrent = false, loadPending = true, generationPending = false, savePending = false, actionsReady = false;
            bool restoringBinding = false;
        };

        void Document::Load()
        {
            loadPending    = false;
            auto *database = AssetDataBase::Get();
            Uuid  owner;
            if (!database->GetOwner(asset, owner)) {
                diagnostic = "Mesh source owner is unavailable";
                return;
            }
            auto source = database->FindAsset(owner);
            if (!source) {
                diagnostic = "Mesh source was removed";
                return;
            }
            std::string extension = source->ext;
            if (!extension.empty() && extension.front() == '.')
                extension.erase(extension.begin());
            MeshGeneratorRecipeState recipe;
            if (const auto codec = MeshGeneratorRecipeRegistry::Get()->Acquire(extension)) {
                if (!ReadStamp(owner, savedStamp)) {
                    diagnostic = "Recipe source is unavailable";
                    return;
                }
                std::vector<uint8_t> bytes;
                const auto           file = database->OpenFile(source);
                if (!file || !file->ReadBin(bytes)) {
                    diagnostic = "Recipe source could not be read";
                    return;
                }
                if (const auto status = codec->Decode(bytes, recipe); !status) {
                    diagnostic = status.message;
                    return;
                }
                SourceStamp current;
                if (!ReadStamp(owner, current) || current != savedStamp) {
                    diagnostic = "Recipe source changed while loading";
                    return;
                }
                savedOwner = owner;
                EditorAssetItem saved;
                if (EditorAssetCatalog::Get()->Find(owner, saved))
                    viewSettings.savePath = saved.path;
                owner  = recipe.sourceOwner;
                source = database->FindAsset(owner);
            }
            if (!source || !ReadStamp(owner, sourceStamp)) {
                diagnostic = "Original mesh source is unavailable";
                return;
            }
            const auto compiler = phy::MeshAuthoringCompilerRegistry::Get()->Acquire();
            if (!compiler) {
                diagnostic = "Mesh authoring compiler is unavailable";
                return;
            }
            std::vector<uint8_t> bytes;
            const auto           file = database->OpenFile(source);
            if (!file || !file->ReadBin(bytes)) {
                diagnostic = "Mesh source could not be read";
                return;
            }
            if (const auto status = compiler->Load(bytes, source->ext, sources); !status) {
                diagnostic = status.message;
                return;
            }
            SourceStamp after;
            if (sources.empty() || !ReadStamp(owner, after) || after != sourceStamp) {
                diagnostic = "Mesh source changed while loading";
                return;
            }
            openedSourceKey           = recipe.sourceKey.empty() ? ImportedMeshSourceKey(owner, asset) : recipe.sourceKey;
            const auto selectedSource = openedSourceKey.empty() ? sources.begin()
                                                                : std::find_if(sources.begin(), sources.end(),
                                                                               [&](const auto &value) { return value.key == openedSourceKey; });
            if (selectedSource == sources.end()) {
                diagnostic = "Recipe mesh source key no longer exists";
                return;
            }
            openedSourceKey = selectedSource->key;
            input           = {owner, selectedSource->key, *selectedSource, recipe.controls};
            state.sourceKey = input.sourceKey;
            state.controls  = recipe.controls;
            loaded = sourceCurrent = true;
            if (!recipe.generatorId.empty()) {
                state.generator = AssetGeneratorRegistry::Get()->Find(recipe.generatorId);
                if (!state.generator.identity.generation || GetTypeNode(recipe.settings) != state.generator.descriptor.settingsType) {
                    diagnostic = "Recipe generator or settings schema is unavailable";
                    Focus();
                    return;
                }
                state.settings = std::move(recipe.settings);
            } else {
                const auto generators = AssetGeneratorRegistry::Get()->List(Context());
                if (!generators.empty()) {
                    state.generator = generators.front();
                    Notice(AssetGeneratorRegistry::Get()->CreateSettings(state.generator, state.settings));
                }
            }
            if (const auto status = ValidateControls(state.controls); !status) {
                diagnostic    = status.message;
                sourceCurrent = false;
                return;
            }
            ++viewRevision;
            Focus();
            if (savedOwner && state.generator.identity.generation) {
                generationPending = restoringBinding = true;
                diagnostic                           = "Loading saved binding";
            } else
                diagnostic = "Select control points and Generate";
            RefreshGeneratorActions();
        }

        void Document::Save()
        {
            savePending        = false;
            const auto *result = Generated();
            if (!loaded || !sourceCurrent || !state.candidateCurrent || !result || Busy()) {
                diagnostic = "Save requires a complete binding generated from the current inputs";
                return;
            }
            SourceStamp current;
            if (!ReadStamp(input.sourceOwner, current) || current != sourceStamp) {
                diagnostic    = "Source changed before save";
                sourceCurrent = false;
                return;
            }
            if (savedOwner && (!ReadStamp(savedOwner, current) || current != savedStamp)) {
                diagnostic = "Saved recipe changed externally; close and reopen before saving";
                return;
            }
            if (viewSettings.savePath.empty()) {
                const auto source = AssetDataBase::Get()->FindAsset(input.sourceOwner);
                if (!source) {
                    diagnostic = "Original source was removed";
                    return;
                }
                std::string mount;
                for (const auto &entry : AssetDataBase::Get()->GetMounts())
                    if (entry.writable) {
                        mount = entry.displayName;
                        break;
                    }
                if (mount.empty()) {
                    diagnostic = "No writable source mount is available";
                    return;
                }
                viewSettings.savePath = mount + "/" + source->path.Parent().GetStr();
                if (viewSettings.savePath.back() != '/')
                    viewSettings.savePath += '/';
                viewSettings.savePath += source->path.FileNameWithoutExt() + ".generated." + result->recipeExtension;
            }
            if (std::filesystem::path(viewSettings.savePath).extension().string() != "." + result->recipeExtension ||
                savedOwner == input.sourceOwner) {
                diagnostic = "Save target must use the generated recipe extension and a separate source owner";
                return;
            }
            Uuid       saved;
            const auto status = AssetMutationService::Get()->SaveGeneratedSource(viewSettings.savePath, savedOwner, result->recipeText, saved);
            if (!status) {
                diagnostic = status.message;
                return;
            }
            savedOwner = saved;
            savedState = state.id;
            AssetCookService::Get()->RequestCook(savedOwner);
            if (ReadStamp(savedOwner, savedStamp)) {
                diagnostic = "Recipe saved; cook queued";
            } else {
                sourceCurrent = false;
                diagnostic    = "Recipe saved and cook queued; source could not be revalidated, reopen before further edits";
            }
            ++viewRevision;
        }

        void Document::Focus()
        {
            framePending = true;
            if (!loaded)
                return;
            Vector3 minimum(std::numeric_limits<float>::max()), maximum(std::numeric_limits<float>::lowest());
            for (const auto &primitive : input.mesh.primitives)
                for (const auto &point : primitive.positions) {
                    const auto p = primitive.transform * Vector4(point.x, point.y, point.z, 1.f);
                    for (uint32_t axis = 0; axis < 3; ++axis) {
                        minimum[axis] = std::min(minimum[axis], p[axis]);
                        maximum[axis] = std::max(maximum[axis], p[axis]);
                    }
                }
            if (!Finite(minimum) || !Finite(maximum))
                return;
            subjectBounds = AABB(minimum, maximum);
            ++viewRevision;
        }
        void Document::AddControl()
        {
            if (!loaded)
                return;
            auto             controls = state.controls;
            MeshControlPoint point{Uuid::Create().ToString(), camera.pivot, MeshControlMode::Free};
            selected = point.id;
            controls.emplace_back(std::move(point));
            Notice(SetControls(std::move(controls)));
        }
        void Document::RemoveControl()
        {
            auto controls = state.controls;
            if (std::erase_if(controls, [&](const auto &point) { return point.id == selected; })) {
                selected.clear();
                Notice(SetControls(std::move(controls)));
            }
        }

        class MeshPanel final : public UIElement {
        public:
            MeshPanel(Document &document, UITextSystem *text) : document(document)
            {
                const ReflectedFormContext context{document.History(), EditorCore::GetPropertyEditors(), EditorCore::GetPropertyChanges(),
                                                   GetDefaultUiTheme()};
                generator = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "Generator settings", context)));
                point     = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "Selected control", context)));
                view      = static_cast<ReflectedFormView *>(AddChild(std::make_unique<ReflectedFormView>(text, "Preview and recipe", context)));
                view->Bind({&document.ViewSettings(), GetTypeNode(TypeInfo<MeshDocumentViewSettings>::RegisteredId())});
                generator->Form().SetEditCommandFactory([this](const PropertyDescriptor &field, Any value) {
                    const Any before = field.GetValue();
                    if (!field.SetValue(value))
                        return UndoCommandPtr{};
                    auto changed             = this->document.State();
                    changed.settings         = settings;
                    changed.candidateCurrent = false;
                    if (!field.SetValue(before))
                        return UndoCommandPtr{};
                    return this->document.MakeEdit("Edit generator settings", std::move(changed));
                });
                point->Form().SetEditCommandFactory([this](const PropertyDescriptor &field, Any value) {
                    const auto before = selectedPoint;
                    if (!field.SetValue(value))
                        return UndoCommandPtr{};
                    auto       changed = this->document.State();
                    const auto found =
                        std::find_if(changed.controls.begin(), changed.controls.end(), [&](const auto &item) { return item.id == before.id; });
                    if (found != changed.controls.end())
                        *found = selectedPoint;
                    selectedPoint = before;
                    if (!ValidateControls(changed.controls))
                        return UndoCommandPtr{};
                    changed.candidateCurrent = false;
                    return this->document.MakeEdit("Edit selected control", std::move(changed));
                });
                Sync();
            }
            bool Editing() const
            {
                return generator->HasActiveInteraction() || point->HasActiveInteraction() || view->HasActiveInteraction();
            }
            void Cancel()
            {
                generator->CancelInteractions();
                point->CancelInteractions();
                view->CancelInteractions();
                captured = nullptr;
            }
            void Sync()
            {
                if (revision != document.ViewRevision() && !Editing()) {
                    generator->Unbind();
                    point->Unbind();
                    settings = document.Settings();
                    if (settings && GetTypeNode(settings))
                        generator->Bind({settings.Data(), GetTypeNode(settings)});
                    const auto &controls = document.State().controls;
                    const auto  found =
                        std::find_if(controls.begin(), controls.end(), [&](const auto &item) { return item.id == document.Selected(); });
                    if (found != controls.end()) {
                        selectedPoint = *found;
                        point->Bind({&selectedPoint, GetTypeNode(TypeInfo<MeshControlPoint>::RegisteredId())});
                    }
                    revision = document.ViewRevision();
                }
                generator->SynchronizeLayout();
                point->SynchronizeLayout();
                view->SynchronizeLayout();
            }
            bool WantsPointerCapture() const override
            {
                return captured != nullptr;
            }
            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                UIElement *target = captured;
                if (!target)
                    for (auto *form : {generator, point, view})
                        if (form->GetBounds().Contains(event.x, event.y)) {
                            target = form;
                            break;
                        }
                if (!target)
                    return UIEventResult::UNHANDLED;
                if (event.action == UIPointerAction::DOWN)
                    focus = target;
                const auto result = target->OnPointerEvent(event);
                captured          = target->WantsPointerCapture() ? target : nullptr;
                return result;
            }
            void OnPointerCancel(uint32_t pointer) override
            {
                if (captured)
                    captured->OnPointerCancel(pointer);
                captured = nullptr;
            }
            UIEventResult OnKeyEvent(const UIKeyEvent &event) override
            {
                return focus ? focus->OnKeyEvent(event) : UIEventResult::UNHANDLED;
            }
            UIEventResult OnTextInput(const UITextInputEvent &event) override
            {
                return focus ? focus->OnTextInput(event) : UIEventResult::UNHANDLED;
            }

        protected:
            UISize OnMeasure(const UIConstraints &constraints) override
            {
                const auto &m = GetDefaultUiTheme().metrics;
                return constraints.Constrain({m.fieldWidth * 2, m.rowHeight * 28});
            }
            void ArrangeChildren(const UIRect &content) override
            {
                const auto middle = content.top + content.Height() * 0.5f;
                const auto lower  = content.top + content.Height() * 0.75f;
                generator->Arrange({content.left, content.top, content.right, middle});
                point->Arrange({content.left, middle, content.right, lower});
                view->Arrange({content.left, lower, content.right, content.bottom});
            }

        private:
            Document          &document;
            Any                settings;
            MeshControlPoint   selectedPoint;
            ReflectedFormView *generator = nullptr, *point = nullptr, *view = nullptr;
            UIElement         *focus = nullptr, *captured = nullptr;
            uint64_t           revision = UINT64_MAX;
        };

        void Line(UIPaintContext &context, const Vector2 &a, const Vector2 &b, float width, uint32_t color)
        {
            const float x = b.x - a.x, y = b.y - a.y, length = std::hypot(x, y);
            if (!std::isfinite(length) || length <= 0.00001f)
                return;
            const float dx = -y / length * width * 0.5f, dy = x / length * width * 0.5f;
            context.AddTriangle(a.x - dx, a.y - dy, a.x + dx, a.y + dy, b.x + dx, b.y + dy, color);
            context.AddTriangle(a.x - dx, a.y - dy, b.x + dx, b.y + dy, b.x - dx, b.y - dy, color);
        }
        class MeshViewport final : public UIElement {
        public:
            MeshViewport(Document &document, UITextSystem *text)
                : surfaceRegistry(text ? text->GetSharedRegistryReference() : UITextSystem::SharedRegistryReference{})
                , document(document)
                , skin(GetDefaultUiTheme(), text)
            {
                SetClipsChildren(true);
                SetFocusable(true);
            }
            ~MeshViewport() override
            {
                if (auto *registry = surfaceRegistry.Get(); registry && surfaceTexture != UI_INVALID_TEXTURE)
                    registry->ReleaseTexture(surfaceTexture);
            }
            bool Editing() const
            {
                return dragging;
            }
            bool WantsPointerCapture() const override
            {
                return dragging || navigation.Captured();
            }
            void OnPointerCancel(uint32_t) override
            {
                dragging = false;
                navigation.Cancel();
                MarkPaintDirty();
            }
            UIEventResult OnKeyEvent(const UIKeyEvent &event) override
            {
                if (!AssetPreviewCameraInput::IsFrameKey(event))
                    return UIEventResult::UNHANDLED;
                document.Focus();
                MarkPaintDirty();
                return UIEventResult::HANDLED;
            }
            UIRect Image() const
            {
                return skin.PanelContentRect(GetBounds());
            }
            void ArrangeChildren(const UIRect &content) override
            {
                UIElement::ArrangeChildren(content);
                const auto image = Image();
                ResizeAssetPreviewCamera(document.Camera(), {image.Width(), image.Height()});
            }
            bool Project(const Vector3 &position, Vector2 &screen) const
            {
                const auto image = Image();
                if (!document.Camera().Project(position, {image.Width(), image.Height()}, screen))
                    return false;
                screen.x += image.left;
                screen.y += image.top;
                return true;
            }
            UIEventResult OnPointerEvent(const UIPointerEvent &event) override
            {
                auto &camera = document.Camera();
                if (!dragging && navigation.Handle(camera, event, Image())) {
                    MarkPaintDirty();
                    return UIEventResult::HANDLED;
                }
                if (!document.Source())
                    return UIEventResult::UNHANDLED;
                if (event.action == UIPointerAction::DOWN) {
                    if (event.button != 0 || !Image().Contains(event.x, event.y))
                        return UIEventResult::UNHANDLED;
                    pointer                          = event.pointerId;
                    previous                         = {event.x, event.y};
                    const auto             &m        = skin.Theme().metrics;
                    float                   distance = m.gizmoHitRadius * m.gizmoHitRadius;
                    const MeshControlPoint *nearest  = nullptr;
                    for (const auto &point : document.Source()->controls) {
                        Vector2 screen;
                        if (!Project(point.position, screen))
                            continue;
                        const float candidate = (screen.x - event.x) * (screen.x - event.x) + (screen.y - event.y) * (screen.y - event.y);
                        if (candidate <= distance) {
                            distance = candidate;
                            nearest  = &point;
                        }
                    }
                    if (nearest) {
                        document.Select(nearest->id);
                        moving   = *nearest;
                        dragging = true;
                    } else {
                        document.Select({});
                    }
                    MarkPaintDirty();
                    return UIEventResult::HANDLED;
                }
                if (pointer != event.pointerId || !dragging)
                    return UIEventResult::UNHANDLED;
                if (event.action == UIPointerAction::MOVE || event.action == UIPointerAction::UP) {
                    const float dx = event.x - previous.x, dy = event.y - previous.y;
                    const float units = camera.WorldUnitsPerPixel(moving.position, Image().Height());
                    moving.position += camera.Right() * (dx * units) - camera.Up() * (dy * units);
                    previous = {event.x, event.y};
                    MarkPaintDirty();
                }
                if (event.action == UIPointerAction::UP) {
                    auto       controls = document.Source()->controls;
                    const auto found    = std::find_if(controls.begin(), controls.end(), [&](const auto &point) { return point.id == moving.id; });
                    if (found != controls.end()) {
                        *found = moving;
                        document.Notice(document.SetControls(std::move(controls)));
                    }
                    dragging = false;
                }
                return UIEventResult::HANDLED;
            }
            void OnPaint(UIPaintContext &context) override
            {
                skin.DrawPanel(context, GetBounds(), "Mesh, cage and bone weights");
                const auto image = Image();
                skin.DrawField(context, image, false, false);
                document.FitPreview({image.Width(), image.Height()});
                const auto *source = document.Source();
                context.PushClip(image);
                const auto &theme     = skin.Theme();
                const auto &m         = theme.metrics;
                const auto *generated = document.State().candidateCurrent ? document.Generated() : nullptr;
                const auto &settings  = document.ViewSettings();
                DrawMaterial(context, image);
                if (settings.showWeights) {
                    if (source && generated)
                        DrawWeights(context, image, source->mesh, generated->binding);
                    else
                        DrawNotice(context, image, document.Busy() ? "Loading binding weights..." : "Generate a binding to view bone weights");
                }
                weightsDisplayed = settings.showWeights && source && generated;
                if (!source) {
                    context.PopClip();
                    return;
                }
                if (generated && settings.showCage) {
                    std::set<std::array<uint32_t, 2>> edges;
                    for (const auto &tet : generated->tetrahedra)
                        for (size_t a = 0; a < 4; ++a)
                            for (size_t b = a + 1; b < 4; ++b)
                                edges.insert({std::min(tet[a], tet[b]), std::max(tet[a], tet[b])});
                    constexpr size_t previewEdges = 1024;
                    const size_t     edgeStride   = std::max(size_t{1}, edges.size() / previewEdges + (edges.size() % previewEdges != 0));
                    size_t           edgeIndex    = 0;
                    for (const auto &edge : edges) {
                        if (edgeIndex++ % edgeStride != 0)
                            continue;
                        Vector2 a, b;
                        if (edge[1] < generated->cagePoints.size() && Project(generated->cagePoints[edge[0]], a) &&
                            Project(generated->cagePoints[edge[1]], b))
                            Line(context, a, b, m.gizmoWidth, theme.colors.gizmoY);
                    }
                }
                if (generated && settings.showBones) {
                    std::vector<Matrix4> models;
                    for (const auto &bone : generated->binding.bones) {
                        auto matrix = bone.reference.ToMatrix();
                        if (bone.parent < models.size())
                            matrix = models[bone.parent] * matrix;
                        models.emplace_back(matrix);
                        if (bone.parent < models.size() - 1) {
                            Vector2     a, b;
                            const auto &p = models[bone.parent][3];
                            if (Project({p.x, p.y, p.z}, a) && Project({matrix[3].x, matrix[3].y, matrix[3].z}, b))
                                Line(context, a, b, m.gizmoWidth, theme.colors.accent);
                        }
                    }
                }
                for (const auto &point : source->controls) {
                    Vector2 screen;
                    if (!Project(dragging && point.id == moving.id ? moving.position : point.position, screen))
                        continue;
                    const float    radius = m.selectionMarkerRadius;
                    const uint32_t color  = point.id == document.Selected()       ? theme.colors.gizmoHighlight
                                            : point.mode == MeshControlMode::Free ? theme.colors.accent
                                                                                  : theme.colors.gizmoX;
                    context.AddRoundedRect({screen.x - radius, screen.y - radius, screen.x + radius, screen.y + radius}, color, radius);
                }
                if (skin.Text())
                    uidraw::Text(context, "Alt + L/M/R: orbit/pan/dolly | Wheel: zoom | F: frame", theme.fonts.small,
                                 {image.left + m.controlPad, image.top + m.controlPad, image.right - m.controlPad, image.top + m.rowHeight},
                                 theme.colors.text, skin.Text());
                if (generated && skin.Text()) {
                    float y = image.top + m.rowHeight + m.controlPad;
                    if (!settings.showDiagnostics) {
                        const auto summary = std::to_string(generated->cagePoints.size()) + " controls  /  " +
                                             std::to_string(generated->tetrahedra.size()) + " tetrahedra";
                        uidraw::Text(context, summary, theme.fonts.small, {image.left + m.controlPad, y, image.right - m.controlPad, y + m.rowHeight},
                                     theme.colors.text, skin.Text());
                    } else
                        for (const auto &[label, value] : generated->diagnostics) {
                            uidraw::Text(context, label + ": " + value, theme.fonts.small,
                                         {image.left + m.controlPad, y, image.right - m.controlPad, y + m.rowHeight}, theme.colors.text, skin.Text());
                            y += m.rowHeight;
                        }
                }
                context.PopClip();
            }

        private:
            struct SurfaceKey {
                uint64_t revision;
                Vector3  cameraPosition;
                float    yaw, pitch, fov, nearPlane, farPlane;
                uint32_t width, height, bone;
                bool     operator==(const SurfaceKey &) const = default;
            };
            void DrawNotice(UIPaintContext &context, const UIRect &rect, const std::string &message)
            {
                if (!skin.Text())
                    return;
                const auto &theme = skin.Theme();
                uidraw::Text(context, message, theme.fonts.label,
                             {rect.left + theme.metrics.controlPad, rect.bottom - theme.metrics.rowHeight * 2.f, rect.right, rect.bottom},
                             theme.colors.text, skin.Text());
            }
            void DrawMaterial(UIPaintContext &context, const UIRect &rect)
            {
                if (!(rect.Width() > 0.f && rect.Height() > 0.f))
                    return;
                const float scale  = std::min(1.f, 4096.f / std::max(rect.Width(), rect.Height()));
                const auto *source = document.Source();
                const auto &status = materialPreview.Update(document.MeshAsset(), document.Camera(),
                                                            static_cast<uint32_t>(std::max(1.f, std::round(rect.Width() * scale))),
                                                            static_cast<uint32_t>(std::max(1.f, std::round(rect.Height() * scale))),
                                                            source ? source->sourceKey : std::string_view{}, document.SubjectBounds());
                document.UseProductBounds(status.bounds);
                if (status.texture && status.state == AssetPreviewState::Ready)
                    context.AddTexturedQuad(rect, {0.f, 0.f, 1.f, 1.f}, status.texture, skin.Theme().colors.white);
                if (!status.diagnostic.empty() || status.state != AssetPreviewState::Ready || status.bounds.empty())
                    DrawNotice(context, rect, status.diagnostic.empty() ? "Loading material preview..." : status.diagnostic);
            }
            void
            DrawWeights(UIPaintContext &context, const UIRect &rect, const phy::AuthoringMeshSource &mesh, const phy::AuthoringMeshBinding &binding)
            {
                auto *registry = surfaceRegistry.Get();
                if (!registry || !(rect.Width() > 0.f && rect.Height() > 0.f))
                    return;
                const auto &camera = document.Camera();
                const auto  now    = std::chrono::steady_clock::now();
                if (!weightsDisplayed)
                    lastCameraMove = {};
                else if (surfaceTexture != UI_INVALID_TEXTURE &&
                         (surfaceKey.cameraPosition != camera.position || surfaceKey.pitch != camera.pitch || surfaceKey.yaw != camera.yaw))
                    lastCameraMove = now;
                // Orbiting still draws every triangle; the full inspection resolution returns once the camera settles.
                const float      maxExtent = now - lastCameraMove < std::chrono::milliseconds(120) ? 384.f : 1024.f;
                const float      scale     = std::min(1.f, maxExtent / std::max(rect.Width(), rect.Height()));
                const auto      &settings  = document.ViewSettings();
                const SurfaceKey key{document.Revision(),
                                     camera.position,
                                     camera.yaw,
                                     camera.pitch,
                                     camera.verticalFov,
                                     camera.nearPlane,
                                     camera.farPlane,
                                     static_cast<uint32_t>(std::max(1.f, std::round(rect.Width() * scale))),
                                     static_cast<uint32_t>(std::max(1.f, std::round(rect.Height() * scale))),
                                     settings.weightBone};
                if (surfaceTexture == UI_INVALID_TEXTURE || !(surfaceKey == key)) {
                    const auto pixels = RasterizeMeshWeights(mesh, binding, camera, key.width, key.height, key.bone);
                    if (pixels.pixels.empty()) {
                        uidraw::Text(context, "Mesh surface preview unavailable", skin.Theme().fonts.small, rect, skin.Theme().colors.text,
                                     skin.Text());
                        return;
                    }
                    if (surfaceTexture == UI_INVALID_TEXTURE)
                        surfaceTexture = registry->RegisterTexture(pixels);
                    else
                        registry->UpdateTexture(surfaceTexture, pixels);
                    if (surfaceTexture == UI_INVALID_TEXTURE) {
                        uidraw::Text(context, "Could not allocate the mesh preview texture", skin.Theme().fonts.small, rect, skin.Theme().colors.text,
                                     skin.Text());
                        return;
                    }
                    surfaceKey = key;
                }
                context.AddTexturedQuad(rect, {0.f, 0.f, 1.f, 1.f}, surfaceTexture, 0xffffffffu);
            }
            MeshMaterialPreview                   materialPreview;
            UITextSystem::SharedRegistryReference surfaceRegistry;
            SurfaceKey                            surfaceKey{};
            std::chrono::steady_clock::time_point lastCameraMove{};
            UITextureId                           surfaceTexture = UI_INVALID_TEXTURE;
            Document                             &document;
            UiSkin                                skin;
            MeshControlPoint                      moving;
            Vector2                               previous;
            uint32_t                              pointer = 0;
            AssetPreviewCameraInput               navigation;
            bool                                  dragging = false, weightsDisplayed = false;
        };

        bool Document::IsEditing() const
        {
            const auto *form   = static_cast<const MeshPanel *>(panel.GetAlive());
            const auto *canvas = static_cast<const MeshViewport *>(viewport.GetAlive());
            return (form && form->Editing()) || (canvas && canvas->Editing());
        }
        void Document::CancelInteractions()
        {
            if (auto *form = static_cast<MeshPanel *>(panel.GetAlive()))
                form->Cancel();
            if (auto *canvas = static_cast<MeshViewport *>(viewport.GetAlive()))
                canvas->OnPointerCancel(0);
        }
        std::unique_ptr<UIElement> Document::CreateContent(UITextSystem *text)
        {
            auto view = std::make_unique<MeshPanel>(*this, text);
            panel     = view->GetInputReference();
            RegisterActions();
            return view;
        }
        std::unique_ptr<UIElement> Document::CreatePreview(UITextSystem *text)
        {
            auto view = std::make_unique<MeshViewport>(*this, text);
            viewport  = view->GetInputReference();
            return view;
        }
        void Document::RegisterActions()
        {
            if (actionsReady)
                return;
            actionsReady                    = true;
            const std::weak_ptr<void> alive = lifetime;
            const auto                add = [&](std::string id, std::string label, std::string icon, EditorGroup group, std::function<void()> invoke,
                                 std::function<bool()> enabled) {
                EditorAction action;
                action.id      = "aurora.mesh." + id;
                action.label   = std::move(label);
                action.icon    = std::move(icon);
                action.scope   = scope;
                action.group   = group;
                action.menu    = EditorMenu::Edit;
                action.submenu = "Mesh binding";
                action.invoke  = [alive, invoke = std::move(invoke)] {
                    if (!alive.expired())
                        invoke();
                };
                action.enabled = [alive, enabled = std::move(enabled)] { return !alive.expired() && enabled(); };
                actionIds.push_back(action.id);
                EditorActionRegistry::Get()->Add(std::move(action));
            };
            add(
                "generate", "Generate binding", "play", EditorGroup::Play, [this] { Notice(RequestGenerate()); },
                [this] { return loaded && sourceCurrent && !Busy() && !IsEditing() && AssetGeneratorRegistry::Get()->IsCurrent(state.generator); });
            add("cancel", "Cancel generation", "stop", EditorGroup::Play, [this] { CancelGeneration(); }, [this] { return Busy(); });
            add("add", "Add control", "add", EditorGroup::Viewport, [this] { AddControl(); }, [this] { return loaded && !IsEditing(); });
            add(
                "remove", "Remove selected control", "delete", EditorGroup::Viewport, [this] { RemoveControl(); },
                [this] { return !selected.empty() && !IsEditing(); });
            add("focus", "Frame mesh", "focus", EditorGroup::Navigation, [this] { Focus(); }, [this] { return loaded; });
            RefreshGeneratorActions();
        }
        void Document::RefreshGeneratorActions()
        {
            if (!actionsReady || !loaded)
                return;
            auto *registry = AssetGeneratorRegistry::Get();
            if (listedRegistryRevision == registry->Revision() && listedInputRevision == revision)
                return;
            listedRegistryRevision                         = registry->Revision();
            listedInputRevision                            = revision;
            const auto                          generators = registry->List(Context());
            std::vector<AssetGeneratorIdentity> identities;
            for (const auto &generator : generators)
                identities.emplace_back(generator.identity);
            if (identities == listedGenerators && !generatorActionIds.empty())
                return;
            for (const auto &id : generatorActionIds)
                EditorActionRegistry::Get()->Remove(scope, id);
            generatorActionIds.clear();
            listedGenerators                = identities;
            const std::weak_ptr<void> alive = lifetime;
            for (const auto &generator : generators) {
                EditorAction action;
                action.id        = "aurora.mesh.generator." + generator.identity.id;
                action.label     = generator.descriptor.label;
                action.scope     = scope;
                action.menu      = EditorMenu::Edit;
                action.submenu   = "Generator";
                action.checkable = true;
                action.checked   = [this, alive, identity = generator.identity] { return !alive.expired() && state.generator.identity == identity; };
                action.enabled   = [this, alive, generator] {
                    return !alive.expired() && !IsEditing() && !Busy() && AssetGeneratorRegistry::Get()->IsCurrent(generator);
                };
                action.invoke = [this, alive, identity = generator.identity] {
                    if (!alive.expired() && AssetGeneratorRegistry::Get()->IsCurrent({identity, {}}))
                        Notice(SelectGenerator(identity.id));
                };
                generatorActionIds.push_back(action.id);
                EditorActionRegistry::Get()->Add(std::move(action));
            }
            for (const auto &source : sources) {
                EditorAction action;
                action.id        = "aurora.mesh.source." + source.key;
                action.label     = source.key;
                action.scope     = scope;
                action.menu      = EditorMenu::Edit;
                action.submenu   = "Source mesh";
                action.checkable = true;
                action.checked   = [this, alive, key = source.key] { return !alive.expired() && state.sourceKey == key; };
                action.enabled   = [this, alive] { return !alive.expired() && !IsEditing() && !Busy(); };
                action.invoke    = [this, alive, key = source.key] {
                    if (alive.expired())
                        return;
                    auto changed      = state;
                    changed.sourceKey = key;
                    changed.controls.clear();
                    changed.candidate.reset();
                    changed.candidateCurrent = false;
                    selected.clear();
                    ApplyEdit("Select source mesh", std::move(changed));
                };
                generatorActionIds.push_back(action.id);
                EditorActionRegistry::Get()->Add(std::move(action));
            }
        }
        void Document::Tick(float)
        {
            if (loadPending)
                Load();
            const auto now = std::chrono::steady_clock::now();
            if (loaded && sourceCurrent && (Busy() || now >= nextSourceCheck)) {
                nextSourceCheck = now + std::chrono::milliseconds(250);
                SourceStamp current;
                if (!ReadStamp(input.sourceOwner, current) || current != sourceStamp ||
                    (savedOwner && (!ReadStamp(savedOwner, current) || current != savedStamp))) {
                    sourceCurrent          = false;
                    state.candidateCurrent = false;
                    ++revision;
                    CancelGeneration();
                    diagnostic = "Source changed or was removed; close and reopen this document before generating";
                }
            }
            if (generationPending && !IsEditing()) {
                generationPending = false;
                const AssetGenerationInput generation{Context(), state.settings, std::make_shared<InputPayload>(input)};
                const auto                 status = AssetGeneratorRegistry::Get()->Start(state.generator, generation, pool, request);
                if (!status) {
                    restoringBinding = false;
                    diagnostic       = status.message;
                }
            }
            if (request) {
                const auto progress = request->Poll();
                if (progress.state == AssetGenerationState::Pending || progress.state == AssetGenerationState::Running) {
                    if (sourceCurrent)
                        diagnostic = "Generating " + std::to_string(static_cast<uint32_t>(progress.fraction * 100.f)) + "% " + progress.stage;
                } else {
                    request->Wait();
                    AssetGenerationResult result;
                    auto                  status = request->TakeResult(Context(), result);
                    request.reset();
                    if (status) {
                        const auto *generated = Output(result.payload);
                        status                = generated ? ValidateOutput(input, *generated)
                                                          : AssetGeneratorStatus{AssetGeneratorCode::Failed, "Generator did not provide a mesh binding result"};
                    }
                    if (status && restoringBinding) {
                        state.candidate        = std::move(result.payload);
                        state.candidateCurrent = true;
                        ++viewRevision;
                        diagnostic = "Saved binding ready";
                    } else if (status) {
                        auto changed             = state;
                        changed.candidate        = std::move(result.payload);
                        changed.candidateCurrent = true;
                        ApplyEdit("Generate complete deformation binding", std::move(changed));
                    } else
                        diagnostic = status.message;
                    restoringBinding = false;
                }
            }
            if (savePending && !IsEditing())
                Save();
            RefreshGeneratorActions();
            if (auto *form = static_cast<MeshPanel *>(panel.GetAlive()))
                form->Sync();
            if (auto *canvas = viewport.GetAlive())
                canvas->MarkPaintDirty();
        }

        class MeshViewProvider final : public IAssetViewProvider {
        public:
            bool UsesDocumentWindow() const override
            {
                return true;
            }
            AssetViewDocument OpenDocument(const Uuid &asset, const std::string &) override
            {
                return {CreateMeshGeneratorDocument(asset)};
            }
        };
        MeshViewProvider provider;
    } // namespace

    std::unique_ptr<IMeshGeneratorDocument> CreateMeshGeneratorDocument(const Uuid &asset)
    {
        return std::make_unique<Document>(asset);
    }
    void ReflectMeshGeneratorDocument(SerializationContext *context)
    {
        const auto readOnly = static_cast<uint32_t>(CommonPropertyKey::READONLY);
        context->Register<MeshControlMode>("AuroraMeshControlMode")
            .Enum(MeshControlMode::Free, "Dynamic")
            .Enum(MeshControlMode::WorldFixed, "World fixed")
            .Enum(MeshControlMode::RootDriven, "Root driven");
        context->Register<MeshControlPoint>("AuroraMeshControlPoint")
            .Member<&MeshControlPoint::id>("id")
            .Property(readOnly, Any(true))
            .Member<&MeshControlPoint::position>("position")
            .Member<&MeshControlPoint::mode>("mode");
        context->Register<MeshDocumentViewSettings>("AuroraMeshDocumentViewSettings")
            .Member<&MeshDocumentViewSettings::savePath>("savePath")
            .Member<&MeshDocumentViewSettings::showCage>("showCage")
            .Member<&MeshDocumentViewSettings::showBones>("showBones")
            .Member<&MeshDocumentViewSettings::showWeights>("showWeights")
            .Member<&MeshDocumentViewSettings::weightBone>("weightBone")
            .Member<&MeshDocumentViewSettings::showDiagnostics>("showDiagnostics");
    }
    void RegisterMeshGeneratorDocument()
    {
        AssetViewProviderRegistry::Get()->Register("AuroraMesh", &provider);
    }
    void UnregisterMeshGeneratorDocument()
    {
        AssetViewProviderRegistry::Get()->Unregister("AuroraMesh", &provider);
    }
} // namespace sky::aurora::editor
