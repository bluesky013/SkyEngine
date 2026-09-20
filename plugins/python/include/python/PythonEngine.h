//
// Created by blues on 2024/6/2.
//

#pragma once

#include <core/environment/Singleton.h>
#include <string>
#include <string_view>

namespace sky::py {

    class PythonEngine : public Singleton<PythonEngine> {
    public:
        PythonEngine() = default;
        ~PythonEngine() override = default;

        bool Init();
        void Shutdown();

        bool RunString(std::string_view source);
        bool RunFile(const std::string &path);

        bool IsInited() const { return isInited; }

    private:
        static std::string ResolveHome();

        bool isInited = false;
    };

} // namespace sky::py
