//
// Created by Zach Lee on 2021/11/11.
//

#include <core/util/DynamicModule.h>
#ifdef _WIN32
#include <windows.h>
#else
#endif

namespace sky {

    DynamicModule::DynamicModule(const std::string& str) : name(str), handle(nullptr)
    {

    }

    DynamicModule::~DynamicModule()
    {
        Unload();
    }

    bool DynamicModule::Load()
    {
#ifdef _WIN32
        handle = ::LoadLibraryExA(name.c_str(), nullptr, 0);
#else
#endif
        return handle != nullptr;
    }

    void DynamicModule::Unload()
    {
        if (handle != nullptr) {
#ifdef _WIN32
            ::FreeLibrary((HMODULE)handle);
#else
#endif
        }
        handle = nullptr;
    }

    void* DynamicModule::GetAddress(const std::string& str) const
    {
        if (handle == nullptr) {
            return nullptr;
        }
#ifdef _WIN32
        return ::GetProcAddress((HMODULE)handle, str.c_str());
#elif
#endif
    }

    bool DynamicModule::IsLoaded() const
    {
        return handle != nullptr;
    }



}