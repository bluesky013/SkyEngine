//
// Created by Zach Lee on 2021/11/11.
//

#include <core/util/DynamicModule.h>
#include <vector>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

static const std::string APPLE_DYN_PREFIX = "lib";
static const std::string APPLE_DYN_SUFFIX = ".dylib";

namespace sky {

    DynamicModule::DynamicModule(const std::string &str) : name(str), handle(nullptr)
    {
    }

    DynamicModule::~DynamicModule()
    {
        Unload();
    }

    bool DynamicModule::Load()
    {
        std::vector<std::string> names = {name, name + "d"};

        for (auto &ptr : names) {
#ifdef _WIN32
            handle = ::LoadLibraryExA(ptr.c_str(), nullptr, 0);
#else
            std::string libName = APPLE_DYN_PREFIX + ptr + APPLE_DYN_SUFFIX;
            handle              = dlopen(libName.c_str(), RTLD_LOCAL | RTLD_LAZY);
#endif
            if (handle != nullptr) {
                break;
            }
        }
        return handle != nullptr;
    }

    void DynamicModule::Unload()
    {
        if (handle != nullptr) {
#ifdef _WIN32
            ::FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
        }
        handle = nullptr;
    }

    void *DynamicModule::GetAddress(const std::string &str) const
    {
        if (handle == nullptr) {
            return nullptr;
        }
#ifdef _WIN32
        return ::GetProcAddress((HMODULE)handle, str.c_str());
#else
        return dlsym(handle, str.c_str());
#endif
    }

    bool DynamicModule::IsLoaded() const
    {
        return handle != nullptr;
    }

} // namespace sky