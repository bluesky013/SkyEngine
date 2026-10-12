//
// Registers the material source reflection: `MaterialAssetData` / `MaterialInstanceSource` get
// hand-written JSON codecs (see MaterialReflection.cpp) so `.mat` / `.mati` are (de)serialized in a
// concise authored schema, bypassing the reflection classId/elements wrapper. The property value is
// a std::variant (see MaterialAsset.h); BIN codecs live on the data structs. Idempotent via sky::Once.
//

#pragma once

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::aurora {

    void RegisterMaterialReflection(sky::SerializationContext *context);

} // namespace sky::aurora
