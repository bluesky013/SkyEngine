//
// {{PROJECT_NAME}} game module entry.
//

#pragma once

#include <framework/interface/IModule.h>

namespace sky {

    class {{PROJECT_NAME}}Module : public IModule {
    public:
        {{PROJECT_NAME}}Module()           = default;
        ~{{PROJECT_NAME}}Module() override = default;

        bool Init(const StartArguments &args) override;
        void Start() override;
        void Tick(float delta) override;
        void Shutdown() override;
    };

} // namespace sky
