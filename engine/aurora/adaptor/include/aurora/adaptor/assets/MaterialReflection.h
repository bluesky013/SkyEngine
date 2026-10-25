//
// Registers the material source reflection: `MaterialAssetData` / `MaterialInstanceSource` get
// hand-written JSON codecs (see MaterialReflection.cpp) so `.mat` / `.mati` are (de)serialized in a
// concise authored schema, bypassing the reflection classId/elements wrapper. The property value is
// a std::variant (see MaterialAsset.h); BIN codecs live on the data structs. Owned by Aurora.Metadata.
//

#pragma once

#include <memory>

namespace sky {
    class SerializationContext;
    struct TypeNode;
} // namespace sky

namespace sky::aurora {

    struct MaterialPropertyData;
    std::unique_ptr<sky::TypeNode> MakeMaterialPropertyReflection(const MaterialPropertyData &);
    void                           RegisterMaterialReflection(sky::SerializationContext *context);

} // namespace sky::aurora
