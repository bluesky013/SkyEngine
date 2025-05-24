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

#ifdef _WIN32
static const std::string DYN_PREFIX = "";
static const std::string DYN_SUFFIX = ".dll";
#elif __APPLE__
static const std::string DYN_PREFIX = "lib";
static const std::string DYN_SUFFIX = ".dylib";
#elif defined(__ANDROID__)
static const std::string DYN_PREFIX = "lib";
static const std::string DYN_SUFFIX = ".so";
#endif

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
            std::string libName = DYN_PREFIX + ptr + DYN_SUFFIX;
#ifdef _WIN32
            handle = ::LoadLibraryExA(libName.c_str(), nullptr, 0);
#else
            handle = dlopen(libName.c_str(), RTLD_LOCAL | RTLD_LAZY);
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
