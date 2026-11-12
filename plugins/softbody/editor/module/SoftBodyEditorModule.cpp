#include <aurora/editor/MeshGeneratorDocumentContract.h>
#include <framework/interface/IModule.h>
#include <softbody/assets/SoftBodyRecipe.h>
#include <softbody/editor/SoftBodyGenerator.h>

namespace sky::softbody {
    class SoftBodyEditorModule final : public IModule {
    public:
        bool Init(const StartArguments &) override
        {
            auto *generators = editor::AssetGeneratorRegistry::Get();
            if (!generators->Register(owner, CreateSoftBodyGenerator())) {
                return false;
            }
            if (!aurora::editor::MeshGeneratorRecipeRegistry::Get()->Register(RIG_RECIPE_EXTENSION, owner, CreateSoftBodyRecipeCodec())) {
                generators->UnregisterOwner(owner);
                return false;
            }
            return true;
        }
        void Shutdown() override
        {
            editor::AssetGeneratorRegistry::Get()->UnregisterOwner(owner);
            aurora::editor::MeshGeneratorRecipeRegistry::Get()->UnregisterOwner(owner);
        }

    private:
        Uuid owner = Uuid::Create();
    };
} // namespace sky::softbody
REGISTER_MODULE_TAGGED(sky::softbody::SoftBodyEditorModule)
