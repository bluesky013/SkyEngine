#include <curve/assets/CurveAsset.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::curve {
    class CurveAssetsModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterCurveAssets(SerializationContext::Get());
        }
    };
} // namespace sky::curve
REGISTER_MODULE_TAGGED(sky::curve::CurveAssetsModule)
