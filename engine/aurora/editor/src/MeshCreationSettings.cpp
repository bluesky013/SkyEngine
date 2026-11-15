#include <aurora/editor/MeshCreationSettings.h>

#include <editor/core/asset/EditorAssetCatalog.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::aurora::editor {
    void ReflectMeshCreationSettings(SerializationContext *context)
    {
        context->Register<MeshCreationSettings>("MeshCreationSettings")
            .Member<&MeshCreationSettings::material>("material")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Default material")))
            .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AuroraMaterial")))
            .Property(CommonPropertyKey::TOOLTIP,
                      Any(std::string("Material asset assigned to newly created meshes. Existing objects are unchanged.")));
    }
    sky::editor::PreferenceResult ValidateMeshCreationSettings(const MeshCreationSettings &settings)
    {
        sky::editor::EditorAssetItem item;
        const auto                  *catalog = sky::editor::GetEditorAssetCatalog();
        if (!settings.material || !catalog || !catalog->Find(settings.material, item))
            return {sky::editor::PreferenceCode::ValidationFailed, "aurora.creation", "material", "The default material asset is missing."};
        if (item.type != "AuroraMaterial")
            return {sky::editor::PreferenceCode::ValidationFailed, "aurora.creation", "material", "Select an Aurora material asset."};
        return {};
    }
} // namespace sky::aurora::editor
