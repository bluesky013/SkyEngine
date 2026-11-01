#include "HostAcceptance.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <core/logger/Logger.h>
#include <napi/native_api.h>

#include <algorithm>
#include <dlfcn.h>
#include <filesystem>
#include <limits>
#include <mutex>

namespace {
    using sky::Logger;
    constexpr const char                 *TAG               = "OHOSHostAcceptance";
    const char                            libraryAnchor     = 0;
    napi_env                              ownerEnv          = nullptr;
    napi_ref                              resourceReference = nullptr;
    sky::OHOSApplicationInfo              context;
    std::vector<host_acceptance::Surface> surfaces;
    std::mutex                            surfaceMutex;

    napi_value Boolean(napi_env env, bool value)
    {
        napi_value result = nullptr;
        return napi_get_boolean(env, value, &result) == napi_ok ? result : nullptr;
    }

    void Cleanup(void *)
    {
        sky::OHOSHost::Get()->Stop();
        std::lock_guard lock(surfaceMutex);
        for (const auto &surface : surfaces) {
            if (surface.window && !host_acceptance::ReleaseFixtureSurface(surface.window))
                LOG_E(TAG, "Fixture surface release failed");
        }
        surfaces.clear();
        context.resourceManager.reset();
        if (resourceReference && napi_delete_reference(ownerEnv, resourceReference) != napi_ok)
            LOG_E(TAG, "Fixture ResourceManager reference release failed");
        resourceReference = nullptr;
        ownerEnv          = nullptr;
    }

    bool ReadPath(napi_env env, napi_value value, std::string &path)
    {
        size_t size = 0;
        if (napi_get_value_string_utf8(env, value, nullptr, 0, &size) != napi_ok || size == 0)
            return false;
        path.resize(size + 1);
        if (napi_get_value_string_utf8(env, value, path.data(), path.size(), &size) != napi_ok)
            return false;
        path.resize(size);
        return path.find('\0') == std::string::npos && std::filesystem::path(path).is_absolute();
    }

    napi_value Configure(napi_env env, napi_callback_info callback)
    {
        napi_value args[2] = {};
        size_t     count   = 2;
        if (ownerEnv != env || context.resourceManager || napi_get_cb_info(env, callback, &count, args, nullptr, nullptr) != napi_ok || count != 2 ||
            !ReadPath(env, args[0], context.filesPath))
            return Boolean(env, false);
        Dl_info library{};
        if (!dladdr(&libraryAnchor, &library) || !library.dli_fname || !std::filesystem::path(library.dli_fname).is_absolute())
            return Boolean(env, false);
        context.nativeLibraryPath = std::filesystem::path(library.dli_fname).parent_path().string();
        if (napi_create_reference(env, args[1], 1, &resourceReference) != napi_ok)
            return Boolean(env, false);
        auto *manager = OH_ResourceManager_InitNativeResourceManager(env, args[1]);
        if (!manager) {
            if (napi_delete_reference(env, resourceReference) != napi_ok)
                LOG_E(TAG, "Rejected ResourceManager reference release failed");
            resourceReference = nullptr;
            return Boolean(env, false);
        }
        context.resourceManager = {manager, OH_ResourceManager_ReleaseNativeResourceManager};
        return Boolean(env, true);
    }

    napi_value Ready(napi_env env, napi_callback_info)
    {
        std::lock_guard lock(surfaceMutex);
        return Boolean(env, ownerEnv == env && context.resourceManager && surfaces.size() >= 2 &&
                                std::all_of(surfaces.begin(), surfaces.end(),
                                            [](const auto &surface) { return surface.window && surface.width && surface.height; }));
    }

    napi_value Run(napi_env env, napi_callback_info)
    {
        if (ownerEnv != env || !context.resourceManager)
            return nullptr;
        std::vector<host_acceptance::Surface> snapshot;
        {
            std::lock_guard lock(surfaceMutex);
            for (const auto &surface : surfaces) {
                if (!host_acceptance::RetainFixtureSurface(surface.window)) {
                    for (const auto &held : snapshot)
                        if (!host_acceptance::ReleaseFixtureSurface(held.window))
                            LOG_E(TAG, "Fixture snapshot release failed");
                    return nullptr;
                }
                snapshot.push_back(surface);
            }
        }
        const auto report = host_acceptance::Run(context, snapshot);
        for (const auto &held : snapshot)
            if (!host_acceptance::ReleaseFixtureSurface(held.window))
                LOG_E(TAG, "Fixture snapshot release failed");
        napi_value result = nullptr;
        return napi_create_string_utf8(env, report.data(), report.size(), &result) == napi_ok ? result : nullptr;
    }

    napi_value StartStress(napi_env env, napi_callback_info)
    {
        return Boolean(env, ownerEnv == env && context.resourceManager && host_acceptance::StartStress(context));
    }

    napi_value StressStats(napi_env env, napi_callback_info)
    {
        const auto report = host_acceptance::StressStats();
        napi_value result = nullptr;
        return napi_create_string_utf8(env, report.data(), report.size(), &result) == napi_ok ? result : nullptr;
    }

    napi_value StopStress(napi_env env, napi_callback_info)
    {
        const auto report = host_acceptance::StopStress();
        napi_value result = nullptr;
        return napi_create_string_utf8(env, report.data(), report.size(), &result) == napi_ok ? result : nullptr;
    }

    napi_value SetActive(napi_env env, napi_callback_info callback)
    {
        napi_value argument = nullptr;
        size_t     count    = 1;
        bool       active   = false;
        if (napi_get_cb_info(env, callback, &count, &argument, nullptr, nullptr) != napi_ok || count != 1 ||
            napi_get_value_bool(env, argument, &active) != napi_ok)
            return Boolean(env, false);
        host_acceptance::StressActive(active);
        return Boolean(env, true);
    }

    void SurfaceChanged(OH_NativeXComponent *component, void *window)
    {
        uint64_t width = 0, height = 0;
        if (!component || !window ||
            OH_NativeXComponent_GetXComponentSize(component, window, &width, &height) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
            width > std::numeric_limits<uint32_t>::max() || height > std::numeric_limits<uint32_t>::max()) {
            LOG_E(TAG, "Fixture requires a real SDK XComponent surface");
            return;
        }
        std::lock_guard lock(surfaceMutex);
        auto found = std::find_if(surfaces.begin(), surfaces.end(), [component](const auto &surface) { return surface.component == component; });
        if (found == surfaces.end()) {
            surfaces.push_back({component});
            found = surfaces.end() - 1;
        }
        if (found->window != window) {
            if (!host_acceptance::RetainFixtureSurface(static_cast<OHNativeWindow *>(window))) {
                LOG_E(TAG, "Fixture surface retain failed");
                return;
            }
            if (found->window && !host_acceptance::ReleaseFixtureSurface(found->window))
                LOG_E(TAG, "Fixture replaced surface release failed");
            found->window = static_cast<OHNativeWindow *>(window);
        }
        found->width  = static_cast<uint32_t>(width);
        found->height = static_cast<uint32_t>(height);
        host_acceptance::StressSurfaceChanged(*found);
    }

    void SurfaceDestroyed(OH_NativeXComponent *component, void *)
    {
        host_acceptance::StressSurfaceDestroyed(component);
        std::lock_guard lock(surfaceMutex);
        const auto      found =
            std::find_if(surfaces.begin(), surfaces.end(), [component](const auto &surface) { return surface.component == component; });
        if (found != surfaces.end()) {
            if (found->window && !host_acceptance::ReleaseFixtureSurface(found->window))
                LOG_E(TAG, "Fixture destroyed surface release failed");
            surfaces.erase(found);
        }
    }

    napi_value Init(napi_env env, napi_value exports)
    {
        if (ownerEnv && ownerEnv != env)
            return nullptr;
        if (!ownerEnv) {
            if (napi_add_env_cleanup_hook(env, Cleanup, nullptr) != napi_ok)
                return nullptr;
            ownerEnv = env;
        }
        const napi_property_descriptor properties[] = {{"configure", nullptr, Configure, nullptr, nullptr, nullptr, napi_default, nullptr},
                                                       {"ready", nullptr, Ready, nullptr, nullptr, nullptr, napi_default, nullptr},
                                                       {"run", nullptr, Run, nullptr, nullptr, nullptr, napi_default, nullptr},
                                                       {"startStress", nullptr, StartStress, nullptr, nullptr, nullptr, napi_default, nullptr},
                                                       {"stressStats", nullptr, StressStats, nullptr, nullptr, nullptr, napi_default, nullptr},
                                                       {"stopStress", nullptr, StopStress, nullptr, nullptr, nullptr, napi_default, nullptr},
                                                       {"setActive", nullptr, SetActive, nullptr, nullptr, nullptr, napi_default, nullptr}};
        if (napi_define_properties(env, exports, 7, properties) != napi_ok)
            return nullptr;
        bool hasComponent = false;
        if (napi_has_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &hasComponent) != napi_ok)
            return nullptr;
        if (hasComponent) {
            napi_value value     = nullptr;
            void      *component = nullptr;
            if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &value) != napi_ok ||
                napi_unwrap(env, value, &component) != napi_ok || !component)
                return nullptr;
            static OH_NativeXComponent_Callback callbacks{SurfaceChanged, SurfaceChanged, SurfaceDestroyed, nullptr};
            if (OH_NativeXComponent_RegisterCallback(static_cast<OH_NativeXComponent *>(component), &callbacks) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS)
                return nullptr;
        }
        return exports;
    }
    napi_module module = {1, 0, nullptr, Init, "OHOSHostAcceptance", nullptr, {0}};
} // namespace

extern "C" __attribute__((constructor)) void RegisterHostAcceptance()
{
    napi_module_register(&module);
}
