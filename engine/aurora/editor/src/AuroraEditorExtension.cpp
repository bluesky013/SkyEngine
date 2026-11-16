//
// Aurora editor extension (see AuroraEditorExtension.h).
//

#include <aurora/editor/AuroraEditorExtension.h>

#include <aurora/adaptor/assets/SceneAssets.h>
#include <aurora/adaptor/components/CameraComponent.h>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/PointLightComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/editor/AuroraAssetViews.h>
#include <core/logger/Logger.h>
#include <editor/core/actor/EditorActorCreation.h>
#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/core/preferences/ProjectEditorSettings.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/TransformComponent.h>

#include <functional>
#include <vector>

namespace sky::aurora::editor {

    const char *AuroraEditorExtension::GetName() const
    {
        return "AuroraEditor";
    }

    void AuroraEditorExtension::Register()
    {
        creationSettings = {};
        if (const auto source = AssetDataBase::Get()->FindAsset("engine", FilePath(scene_assets::LIT)))
            creationSettings.material = source->uuid;
        const auto plane = AssetDataBase::Get()->FindAsset("engine", FilePath(scene_assets::PLANE));
        if (!plane || !sky::editor::AssetPreviewProviderRegistry::Get()->SetGroundAssets(plane->uuid, creationSettings.material))
            LOG_W("AuroraEditor", "Preview ground assets are unavailable.");
        auto                       &pages = sky::editor::ProjectEditorSettings::Get()->Pages();
        sky::editor::PreferencePage page{"aurora.creation", "Object Creation / Mesh", &creationSettings,
                                         GetTypeNode(TypeInfo<MeshCreationSettings>::RegisteredId())};
        page.validate = [](const sky::editor::PreferencePage &value) {
            return ValidateMeshCreationSettings(*static_cast<const MeshCreationSettings *>(value.object));
        };
        settingsRegistration               = pages.Add(std::move(page));
        creationLifetime                   = std::make_shared<char>();
        const std::weak_ptr<void> lifetime = creationLifetime;
        for (const bool cube : {true, false}) {
            const char *id = cube ? "cube" : "plane";
            if (sky::editor::EditorActorCreation::Get()->RegisterCreation(
                    {id, cube ? "Cube" : "Plane", "Scene", cube ? 1 : 2, [this, cube, lifetime](Actor &actor) {
                         if (lifetime.expired())
                             return false;
                         if (!sky::editor::ProjectEditorSettings::Get()->IsReady()) {
                             LOG_E("AuroraEditor", "Repair the project editor settings and restart the editor before creating meshes.");
                             return false;
                         }
                         const auto status = ValidateMeshCreationSettings(creationSettings);
                         if (!status.Succeeded()) {
                             LOG_E("AuroraEditor", "Cannot create mesh: %s", status.message.c_str());
                             return false;
                         }
                         const auto source = AssetDataBase::Get()->FindAsset("engine", FilePath(cube ? scene_assets::CUBE : scene_assets::PLANE));
                         if (!source) {
                             LOG_E("AuroraEditor", "Missing mesh asset: %s", cube ? scene_assets::CUBE : scene_assets::PLANE);
                             return false;
                         }
                         actor.SetName(cube ? "Cube" : "Plane");
                         auto *mesh = actor.AddComponent<StaticMeshComponent>();
                         mesh->SetMeshUuid(source->uuid);
                         mesh->SetMaterialUuid(creationSettings.material);
                         mesh->SetCastShadow(true);
                         mesh->SetReceiveShadow(true);
                         if (!cube) {
                             auto *transform = actor.GetComponent<TransformComponent>();
                             if (!transform)
                                 transform = actor.AddComponent<TransformComponent>();
                             transform->SetLocalTranslation({0, -.5f, 0});
                         }
                         return true;
                     }}))
                creators.emplace_back(id);
        }
        const auto add = [&](const char *id, const char *label, int order, std::function<bool(Actor &)> create) {
            if (sky::editor::EditorActorCreation::Get()->RegisterCreation(
                    {id, label, "Scene", order,
                     [lifetime, create = std::move(create)](Actor &actor) { return !lifetime.expired() && create(actor); }}))
                creators.emplace_back(id);
        };
        add("point-light", "Point Light", 3, [](Actor &actor) {
            actor.SetName("Point Light");
            actor.AddComponent<PointLightComponent>();
            actor.GetComponent<TransformComponent>()->SetLocalTranslation({2, 2, 2});
            return true;
        });
        add("directional-light", "Directional Light", 4, [](Actor &actor) {
            actor.SetName("Directional Light");
            actor.AddComponent<DirectLightComponent>()->SetCastShadow(true);
            actor.GetComponent<TransformComponent>()->SetLocalRotationEuler({-40, 30, 0});
            return true;
        });
        add("camera", "Camera", 5, [](Actor &actor) {
            actor.SetName("Camera");
            actor.AddComponent<CameraComponent>();
            actor.GetComponent<TransformComponent>()->SetLocalTranslation({0, 0, 5});
            return true;
        });
        add("environment", "Environment", 6, [](Actor &actor) {
            actor.SetName("Environment");
            return actor.AddComponent<EnvironmentComponent>() != nullptr;
        });
        RegisterAuroraAssetViews();
    }

    void AuroraEditorExtension::Unregister()
    {
        sky::editor::AssetPreviewProviderRegistry::Get()->SetGroundAssets({}, {});
        creationLifetime.reset();
        for (const auto &id : creators)
            sky::editor::EditorActorCreation::Get()->UnRegisterCreation(id);
        creators.clear();
        UnregisterAuroraAssetViews();
        sky::editor::ProjectEditorSettings::Get()->Pages().RemoveRegistration(settingsRegistration);
        settingsRegistration = 0;
    }

    std::vector<std::string> AuroraEditorExtension::GetIconDirectories() const
    {
        return {"icons/aurora"};
    }

} // namespace sky::aurora::editor
