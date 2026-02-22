//
// Created by Zach Lee on 2021/11/11.
//

#pragma once

#include <string>

namespace sky {

    class DynamicModule {
    public:
        DynamicModule(const std::string &name);
        ~DynamicModule();

        bool Load();

        void Unload();

        template <typename Func>
        Func GetAddress(const std::string &str)
        {
            return reinterpret_cast<Func>(GetAddress(str));
        }

        void *GetAddress(const std::string &str) const;

        bool IsLoaded() const;

        std::string GetLastError() const;

    private:
        std::string name;
        void       *handle;
    };

} // namespace sky
