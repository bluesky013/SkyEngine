#pragma once

#include <core/util/Uuid.h>
#include <editor/core/preferences/PreferenceResult.h>

namespace sky {
    class SerializationContext;
}
namespace sky::aurora::editor {
    struct MeshCreationSettings {
        Uuid material;
    };
    void                          ReflectMeshCreationSettings(SerializationContext *context);
    sky::editor::PreferenceResult ValidateMeshCreationSettings(const MeshCreationSettings &settings);
} // namespace sky::aurora::editor
