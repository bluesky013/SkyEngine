//
// Created by blues on 2024/6/2.
//

#pragma once

#include <core/environment/Singleton.h>

namespace sky::py {

    class PythonEngine : public Singleton<PythonEngine> {
    public:
        PythonEngine() = default;
        ~PythonEngine() override = default;

        bool Init();
        void Shutdown();

    private:
        bool isInited = false;
    };

} // namespace sky::py