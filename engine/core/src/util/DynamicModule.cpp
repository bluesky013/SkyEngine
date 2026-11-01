//
// Created by Zach Lee on 2021/11/11.
//

#include <core/util/DynamicModule.h>

#include <core/environment/Singleton.h>
#include <mutex>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif
#ifdef __APPLE__
    #include <TargetConditionals.h>
    #include <mach-o/dyld.h>
#endif

#ifdef _WIN32
static const std::string DYN_PREFIX = "";
static const std::string DYN_SUFFIX = ".dll";
#elif __APPLE__
static const std::string DYN_PREFIX = "lib";
static const std::string DYN_SUFFIX = ".dylib";
#elif defined(__ANDROID__) || defined(__linux__)
static const std::string DYN_PREFIX = "lib";
static const std::string DYN_SUFFIX = ".so";
#endif

namespace sky {

#ifdef __APPLE__
    static std::string GetEmbeddedModulePath(const std::string &name, const std::string &libraryName)
    {
        uint32_t pathSize = 0;
        _NSGetExecutablePath(nullptr, &pathSize);
        std::vector<char> executablePath(pathSize);
        if (pathSize == 0 || _NSGetExecutablePath(executablePath.data(), &pathSize) != 0) {
            return {};
        }

        const std::string path(executablePath.data());
        const auto        separator = path.find_last_of('/');
        if (separator == std::string::npos) {
            return {};
        }
        const std::string executableDirectory = path.substr(0, separator + 1);
    #if TARGET_OS_IPHONE
        return executableDirectory + "Frameworks/" + name + ".framework/" + name;
    #else
        return executableDirectory + libraryName;
    #endif
    }
#endif

    namespace {
        class ResidentLibraries : public Singleton<ResidentLibraries> {
        public:
            void *Load(const std::string &name)
            {
                {
                    std::lock_guard lock(mutex);
                    if (const auto found = handles.find(name); found != handles.end())
                        return found->second;
                }
                const std::string libraryName = DYN_PREFIX + name + DYN_SUFFIX;
                void             *loaded      = nullptr;
                // Native loading may execute initializers; do not hold the registry lock across it.
#ifdef _WIN32
                loaded = ::LoadLibraryExA(libraryName.c_str(), nullptr, 0);
#elif defined(__APPLE__)
                const auto embeddedPath = GetEmbeddedModulePath(name, libraryName);
                if (!embeddedPath.empty())
                    loaded = dlopen(embeddedPath.c_str(), RTLD_LOCAL | RTLD_LAZY);
    #if !TARGET_OS_IPHONE
                if (!loaded)
                    loaded = dlopen(libraryName.c_str(), RTLD_LOCAL | RTLD_LAZY);
    #endif
#else
                loaded = dlopen(libraryName.c_str(), RTLD_LOCAL | RTLD_LAZY);
#endif
                if (!loaded)
                    return nullptr;
                std::lock_guard lock(mutex);
                return handles.emplace(name, loaded).first->second;
            }

        private:
            friend class Singleton<ResidentLibraries>;
            ResidentLibraries() = default;
            std::mutex mutex;
            // Native references deliberately live until process exit, including concurrent first loads.
            std::unordered_map<std::string, void *> handles;
        };
    } // namespace

    DynamicModule::DynamicModule(const std::string &str) : name(str), handle(nullptr)
    {
    }

    DynamicModule::~DynamicModule()
    {
        Unload();
    }

    bool DynamicModule::Load()
    {
        if (!handle)
            handle = ResidentLibraries::Get()->Load(name);
        return handle != nullptr;
    }

    void DynamicModule::Unload()
    {
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

    std::string DynamicModule::GetLastError() const
    {
        std::string res;
#ifdef _WIN32
        DWORD err = ::GetLastError();
        switch (err) {
        case ERROR_MOD_NOT_FOUND: res = "Not Found"; break;
        case ERROR_BAD_EXE_FORMAT: res = "Arch Not Match"; break;
        case ERROR_ACCESS_DENIED: res = "Access Denied"; break;
        default: res = "Unknown"; break;
        }
#else
        const char *error = dlerror();
        if (error != nullptr) {
            res = error;
        }
#endif
        return res;
    }

} // namespace sky
