//
// Created by blues on 2024/9/11.
//
//
// Created by Zach on 2024/3/17.
//

#include <framework/interface/IModule.h>
#include <freetype/FreeTypeFont.h>
#include <freetype/FreeTypeLibrary.h>
#include <freetype/FreeTypeFactory.h>
#include <render/text/TextRegistry.h>

namespace sky {

    class FreeTypeModule : public IModule {
    public:
        FreeTypeModule() = default;
        ~FreeTypeModule() override = default;

        void Start() override
        {
            FreeTypeLibrary::Get()->Init();
            TextRegistry::Get()->Register(new FreeTypeFactory());
        }

        void Shutdown() override
        {
            TextRegistry::Get()->UnRegister();
            FreeTypeLibrary::Destroy();
        }
    };
} // namespace sky
REGISTER_MODULE(sky::FreeTypeModule)