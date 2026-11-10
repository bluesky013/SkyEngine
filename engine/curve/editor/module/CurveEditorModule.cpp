#include <curve/editor/CurveEditor.h>
#include <editor/ui/AssetViewProvider.h>
#include <framework/interface/IModule.h>

namespace sky::curve::editor {
    class CurveEditorModule final : public IModule {
    public:
        bool Init(const StartArguments &) override
        {
            if (provider || sky::editor::AssetViewProviderRegistry::Get()->Find("Curve"))
                return false;
            provider = CreateCurveViewProvider();
            if (!provider)
                return false;
            sky::editor::AssetViewProviderRegistry::Get()->Register("Curve", provider.get());
            return true;
        }
        void Shutdown() override
        {
            if (!provider)
                return;
            sky::editor::AssetViewProviderRegistry::Get()->Unregister("Curve", provider.get());
            provider.reset();
        }

    private:
        std::unique_ptr<sky::editor::IAssetViewProvider> provider;
    };
} // namespace sky::curve::editor
REGISTER_MODULE_TAGGED(sky::curve::editor::CurveEditorModule)
