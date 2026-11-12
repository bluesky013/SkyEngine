#include "MeshMaterialPreview.h"

#include <aurora/adaptor/components/SkeletalMeshComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/renderer/ImportedSkinBinding.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/world/ComponentFactory.h>

namespace sky::aurora::editor {
    using namespace sky::editor;

    namespace {
        AssetPreviewError Invalid(std::string message)
        {
            return {AssetPreviewErrorCode::InvalidArgument, std::move(message)};
        }

        template <typename T>
        T *Add(Actor &actor)
        {
            const auto &id = TypeInfo<T>::RegisteredId();
            return id && ComponentFactory::Get()->Find(id) ? actor.AddComponent<T>() : nullptr;
        }

        AssetPreviewResult Open(IAssetPreviewProvider     &provider,
                                const Uuid                &mesh,
                                std::string_view           sourceKey,
                                const EditorCamera        &camera,
                                uint32_t                   width,
                                uint32_t                   height,
                                const std::optional<AABB> &subjectBounds)
        {
            Uuid  owner;
            auto *database = AssetDataBase::Get();
            if (!database->GetOwner(mesh, owner))
                return Invalid("The mesh no longer belongs to an active asset source");
            std::map<std::string, Uuid> defaults;
            database->GetGeneratedDefaults(owner, defaults);
            ImportedSkinBinding binding;
            const std::string   prefix = "binding:" + std::string(sourceKey) + ":";
            const auto          named  = defaults.find(prefix + "mesh");
            if (named != defaults.end()) {
                if (named->second != mesh)
                    return Invalid("The cooked mesh and selected source binding do not match");
                const auto skin = defaults.find(prefix + "skin"), skeleton = defaults.find(prefix + "skeleton");
                if (skin != defaults.end() || skeleton != defaults.end()) {
                    if (skin == defaults.end() || skeleton == defaults.end() || !skin->second || !skeleton->second)
                        return Invalid("The selected mesh has an incomplete skin or skeleton binding");
                    binding = {skin->second, skeleton->second};
                }
            } else if (ResolveImportedSkinBinding(mesh, defaults, binding) == ImportedSkinBindingStatus::Ambiguous) {
                return Invalid("The mesh has multiple reference-pose skin bindings");
            }

            auto world = WorldPtr(World::CreateWorld());
            auto actor = std::make_unique<Actor>(Uuid::Create());
            actor->SetName("Mesh preview");
            if (binding.skin) {
                auto *component = Add<SkeletalMeshComponent>(*actor);
                if (!component)
                    return Invalid("The Aurora skeletal mesh component is unavailable");
                component->SetConfig({.mesh          = mesh,
                                      .skin          = binding.skin,
                                      .skeleton      = binding.skeleton,
                                      .pose          = SkeletalPoseMode::ReferencePose,
                                      .castShadow    = true,
                                      .receiveShadow = false});
            } else {
                auto *component = Add<StaticMeshComponent>(*actor);
                if (!component)
                    return Invalid("The Aurora static mesh component is unavailable");
                component->SetMeshUuid(mesh);
                component->SetCastShadow(true);
            }
            if (!world->AttachToWorld(std::move(actor)))
                return Invalid("Could not attach the preview mesh: " + world->GetLastError());
            return provider.CreateSession({std::move(world), camera, width, height, true, subjectBounds});
        }

        AssetPreviewStatus ReadStatus(AssetPreviewStatusResult result)
        {
            if (const auto *error = std::get_if<AssetPreviewError>(&result))
                return {.state = AssetPreviewState::Failed, .diagnostic = error->message};
            return std::get<AssetPreviewStatus>(std::move(result));
        }
    } // namespace

    std::string ImportedMeshSourceKey(const Uuid &owner, const Uuid &mesh)
    {
        constexpr std::string_view  prefix = "binding:", suffix = ":mesh";
        std::map<std::string, Uuid> defaults;
        if (AssetDataBase::Get()->GetGeneratedDefaults(owner, defaults)) {
            for (const auto &[role, product] : defaults) {
                if (product == mesh && role.size() > prefix.size() + suffix.size() && role.starts_with(prefix) && role.ends_with(suffix))
                    return role.substr(prefix.size(), role.size() - prefix.size() - suffix.size());
            }
        }
        return {};
    }

    Uuid SelectedMeshProduct(const Uuid &openedMesh, std::string_view openedSourceKey, const Uuid &sourceOwner, std::string_view selectedSourceKey)
    {
        if (selectedSourceKey == openedSourceKey)
            return openedMesh;
        std::map<std::string, Uuid> defaults;
        if (AssetDataBase::Get()->GetGeneratedDefaults(sourceOwner, defaults)) {
            const auto found = defaults.find("binding:" + std::string(selectedSourceKey) + ":mesh");
            if (found != defaults.end())
                return found->second;
        }
        return {};
    }

    MeshMaterialPreview::~MeshMaterialPreview()
    {
        Close();
    }

    void MeshMaterialPreview::Close()
    {
        if (session.session && owner == AssetPreviewProviderRegistry::Get()->GetProvider())
            owner->CloseSession(session);
        session = {};
        status  = {};
        product = {};
        sourceKey.clear();
        owner = nullptr;
        width = height = 0;
    }

    const AssetPreviewStatus &MeshMaterialPreview::Update(const Uuid                &mesh,
                                                          const EditorCamera        &camera,
                                                          uint32_t                   targetWidth,
                                                          uint32_t                   targetHeight,
                                                          std::string_view           selectedSourceKey,
                                                          const std::optional<AABB> &subjectBounds)
    {
        auto *provider = AssetPreviewProviderRegistry::Get()->GetProvider();
        if (provider != owner || mesh != product || selectedSourceKey != sourceKey) {
            Close();
            owner     = provider;
            product   = mesh;
            sourceKey = selectedSourceKey;
        }
        if (!owner || !mesh) {
            status = {.state      = AssetPreviewState::Failed,
                      .diagnostic = !owner ? "Aurora preview renderer is unavailable" : "No mesh product selected"};
            return status;
        }
        if (!session.session) {
            const auto result = Open(*owner, mesh, sourceKey, camera, targetWidth, targetHeight, subjectBounds);
            if (const auto *error = std::get_if<AssetPreviewError>(&result)) {
                status = {.state = AssetPreviewState::Failed, .diagnostic = error->message};
                return status;
            }
            session = std::get<AssetPreviewHandle>(result);
            width   = targetWidth;
            height  = targetHeight;
        }
        if (width != targetWidth || height != targetHeight) {
            status = ReadStatus(owner->Resize(session, targetWidth, targetHeight));
            if (status.state == AssetPreviewState::Failed)
                return status;
            width  = targetWidth;
            height = targetHeight;
        }
        status = ReadStatus(owner->Update(session, camera));
        return status;
    }
} // namespace sky::aurora::editor
