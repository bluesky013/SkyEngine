#include <curve/builder/CurveBuilder.h>
#include <framework/interface/IModule.h>

namespace sky::curve {
    class CurveBuilderModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterCurveBuilder();
        }
    };
} // namespace sky::curve
REGISTER_MODULE_TAGGED(sky::curve::CurveBuilderModule)
