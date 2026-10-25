//
// Aurora -> framework bridge entry point. Registers aurora scene types, asset
// handlers and components with the framework SerializationContext / component
// layer (mirrors legacy ReflectRenderAsset + RegisterComponents).
//

#pragma once

namespace sky {
    class SerializationContext;
}

namespace sky::aurora {

    // Owned by Aurora.Metadata; called once when its process definition is created.
    void AuroraReflection(sky::SerializationContext *context);

} // namespace sky::aurora
