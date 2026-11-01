#include <ace/xcomponent/native_interface_xcomponent.h>
#include <core/logger/Logger.h>
#include <framework/application/GameApplication.h>
#include <framework/platform/OHOSHost.h>
#include <framework/platform/OHOSNativeTextInput.h>
#include <napi/native_api.h>

#include <charconv>
#include <cmath>
#include <dlfcn.h>
#include <filesystem>
#include <limits>

namespace {
    using sky::Logger;
    constexpr const char *TAG           = "OHOSLauncher";
    const char            libraryAnchor = 0;

    sky::OHOSHost &Host()
    {
        return *sky::OHOSHost::Get();
    }

    std::unique_ptr<sky::Application> CreateApplication()
    {
        auto application = std::make_unique<sky::GameApplication>();
        application->SetExternalFrameLoop(true);
        char  rhi[]       = "--rhi";
        char  vulkan[]    = "vulkan";
        char *arguments[] = {rhi, vulkan};
        if (!application->Init(2, arguments)) {
            return {};
        }
        return application;
    }

    napi_env                               ownerEnv          = nullptr;
    napi_ref                               resourceReference = nullptr;
    std::shared_ptr<NativeResourceManager> resources;

    void CleanupSession()
    {
        Host().Stop();
        resources.reset();
        if (resourceReference != nullptr) {
            if (napi_delete_reference(ownerEnv, resourceReference) != napi_ok) {
                LOG_E(TAG, "Failed to release ResourceManager reference");
            }
            resourceReference = nullptr;
        }
    }

    void CleanupEnvironment(void *environment)
    {
        if (ownerEnv == static_cast<napi_env>(environment)) {
            CleanupSession();
            sky::OHOSReleaseTextInputHandler(ownerEnv);
            ownerEnv = nullptr;
        }
    }

    napi_value Boolean(napi_env env, bool value)
    {
        napi_value result = nullptr;
        return napi_get_boolean(env, value, &result) == napi_ok ? result : nullptr;
    }

    napi_value Configure(napi_env env, napi_callback_info callback)
    {
        napi_value args[2] = {};
        size_t     count   = 2;
        size_t     length  = 0;
        if (env != ownerEnv || resources || napi_get_cb_info(env, callback, &count, args, nullptr, nullptr) != napi_ok || count != 2 ||
            napi_get_value_string_utf8(env, args[0], nullptr, 0, &length) != napi_ok || length == 0) {
            return Boolean(env, false);
        }
        std::string path(length + 1, '\0');
        if (napi_get_value_string_utf8(env, args[0], path.data(), path.size(), &length) != napi_ok) {
            return Boolean(env, false);
        }
        path.resize(length);
        Dl_info library = {};
        if (path.find('\0') != std::string::npos || dladdr(&libraryAnchor, &library) == 0 || library.dli_fname == nullptr ||
            !std::filesystem::path(library.dli_fname).is_absolute()) {
            return Boolean(env, false);
        }
        if (napi_create_reference(env, args[1], 1, &resourceReference) != napi_ok) {
            return Boolean(env, false);
        }
        auto *manager = OH_ResourceManager_InitNativeResourceManager(env, args[1]);
        if (manager != nullptr) {
            resources = {manager, OH_ResourceManager_ReleaseNativeResourceManager};
        }
        if (!resources || !Host().Configure({path, std::filesystem::path(library.dli_fname).parent_path().string(), resources}, CreateApplication)) {
            CleanupSession();
            return Boolean(env, false);
        }
        return Boolean(env, true);
    }

    napi_value SetActive(napi_env env, napi_callback_info callback)
    {
        napi_value argument = nullptr;
        size_t     count    = 1;
        bool       active   = false;
        const bool valid    = env == ownerEnv && napi_get_cb_info(env, callback, &count, &argument, nullptr, nullptr) == napi_ok && count == 1 &&
                           napi_get_value_bool(env, argument, &active) == napi_ok;
        return Boolean(env, valid && Host().SetActive(active));
    }

    napi_value Shutdown(napi_env env, napi_callback_info)
    {
        if (env != ownerEnv) {
            return Boolean(env, false);
        }
        CleanupSession();
        return Boolean(env, true);
    }

    napi_value Status(napi_env env, napi_callback_info)
    {
        napi_value value = nullptr;
        return napi_create_int32(env, static_cast<int32_t>(Host().GetStatus()), &value) == napi_ok ? value : nullptr;
    }

    bool ReadString(napi_env env, napi_value value, std::string &text)
    {
        size_t length = 0;
        if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok)
            return false;
        text.resize(length + 1);
        if (napi_get_value_string_utf8(env, value, text.data(), text.size(), &length) != napi_ok)
            return false;
        text.resize(length);
        return true;
    }

    napi_value SetTextInputHandler(napi_env env, napi_callback_info callback)
    {
        napi_value     value = nullptr;
        size_t         count = 1;
        napi_valuetype type  = napi_undefined;
        if (env != ownerEnv || napi_get_cb_info(env, callback, &count, &value, nullptr, nullptr) != napi_ok || count != 1 ||
            napi_typeof(env, value, &type) != napi_ok)
            return Boolean(env, false);
        if (type == napi_undefined) {
            sky::OHOSReleaseTextInputHandler(env);
            return Boolean(env, true);
        }
        return Boolean(env, sky::OHOSRegisterTextInputHandler(env, value));
    }

    napi_value TextInputEvent(napi_env env, napi_callback_info callback)
    {
        napi_value  args[4] = {};
        size_t      count   = 4;
        std::string identity;
        std::string text;
        double      kind  = -1;
        double      error = 0;
        if (env != ownerEnv || napi_get_cb_info(env, callback, &count, args, nullptr, nullptr) != napi_ok || count != 4 ||
            !ReadString(env, args[0], identity) || napi_get_value_double(env, args[1], &kind) != napi_ok ||
            napi_get_value_double(env, args[3], &error) != napi_ok || !std::isfinite(kind) || std::trunc(kind) != kind || kind < 0 ||
            kind >= static_cast<double>(sky::NativeTextInputEventType::END) || !std::isfinite(error) || std::trunc(error) != error ||
            error < std::numeric_limits<int32_t>::min() || error > std::numeric_limits<int32_t>::max())
            return Boolean(env, false);
        uint64_t   token  = 0;
        const auto parsed = std::from_chars(identity.data(), identity.data() + identity.size(), token);
        if (parsed.ec != std::errc{} || parsed.ptr != identity.data() + identity.size() || token == 0)
            return Boolean(env, false);
        if (!ReadString(env, args[2], text)) {
            sky::OHOSFailTextInput(token, -2);
            return Boolean(env, false);
        }
        return Boolean(env, sky::OHOSPostTextInputEvent(
                                token, {static_cast<sky::NativeTextInputEventType>(kind), std::move(text), static_cast<int32_t>(error)}));
    }

    void Touch(OH_NativeXComponent *source, void *window)
    {
        Host().DispatchTouch(source, static_cast<OHNativeWindow *>(window));
    }

    void Mouse(OH_NativeXComponent *source, void *window)
    {
        Host().DispatchMouse(source, static_cast<OHNativeWindow *>(window));
    }

    bool Key(OH_NativeXComponent *source, void *window)
    {
        return Host().DispatchKey(source, static_cast<OHNativeWindow *>(window));
    }

    void Focus(OH_NativeXComponent *source, void *)
    {
        Host().SetFocus(source, true);
    }

    void Blur(OH_NativeXComponent *source, void *)
    {
        Host().SetFocus(source, false);
    }

    void Axis(OH_NativeXComponent *source, ArkUI_UIInputEvent *event, ArkUI_UIInputEvent_Type type)
    {
        if (type == ARKUI_UIINPUTEVENT_TYPE_AXIS)
            Host().DispatchAxis(source, event);
    }

    void SurfaceChanged(OH_NativeXComponent *component, void *window)
    {
        uint64_t width  = 0;
        uint64_t height = 0;
        if (OH_NativeXComponent_GetXComponentSize(component, window, &width, &height) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
            width > std::numeric_limits<uint32_t>::max() || height > std::numeric_limits<uint32_t>::max() ||
            !Host().SetSurface(component, static_cast<OHNativeWindow *>(window), static_cast<uint32_t>(width), static_cast<uint32_t>(height))) {
            LOG_E(TAG, "Surface creation/change was rejected");
        }
    }

    void SurfaceDestroyed(OH_NativeXComponent *component, void *)
    {
        if (!Host().SetSurface(component, nullptr, 0, 0)) {
            LOG_E(TAG, "Surface destruction was rejected");
        }
    }

    napi_value Init(napi_env env, napi_value exports)
    {
        if (ownerEnv != nullptr && ownerEnv != env) {
            LOG_E(TAG, "Only one ArkTS environment may own the engine host");
            return nullptr;
        }
        if (ownerEnv == nullptr) {
            if (napi_add_env_cleanup_hook(env, CleanupEnvironment, env) != napi_ok) {
                return nullptr;
            }
            ownerEnv = env;
        }
        const napi_property_descriptor properties[] = {
            {"configure", nullptr, Configure, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"setActive", nullptr, SetActive, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"shutdown", nullptr, Shutdown, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"status", nullptr, Status, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"setTextInputHandler", nullptr, SetTextInputHandler, nullptr, nullptr, nullptr, napi_default, nullptr},
            {"textInputEvent", nullptr, TextInputEvent, nullptr, nullptr, nullptr, napi_default, nullptr}};
        if (napi_define_properties(env, exports, 6, properties) != napi_ok) {
            return nullptr;
        }
        bool hasComponent = false;
        if (napi_has_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &hasComponent) != napi_ok) {
            return nullptr;
        }
        if (hasComponent) {
            napi_value object = nullptr;
            void      *native = nullptr;
            if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &object) != napi_ok || napi_unwrap(env, object, &native) != napi_ok ||
                native == nullptr) {
                return nullptr;
            }
            auto                                          *component = static_cast<OH_NativeXComponent *>(native);
            static OH_NativeXComponent_Callback            callbacks{SurfaceChanged, SurfaceChanged, SurfaceDestroyed, Touch};
            static OH_NativeXComponent_MouseEvent_Callback mouseCallbacks{Mouse, nullptr};
            if (!Host().Bind(component)) {
                LOG_E(TAG, "XComponent binding rejected");
                return nullptr;
            }
            if (OH_NativeXComponent_RegisterCallback(component, &callbacks) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
                OH_NativeXComponent_RegisterMouseEventCallback(component, &mouseCallbacks) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
                OH_NativeXComponent_RegisterKeyEventCallbackWithResult(component, Key) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
                OH_NativeXComponent_RegisterFocusEventCallback(component, Focus) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
                OH_NativeXComponent_RegisterBlurEventCallback(component, Blur) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
                OH_NativeXComponent_RegisterUIInputEventCallback(component, Axis, ARKUI_UIINPUTEVENT_TYPE_AXIS) !=
                    OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
                LOG_E(TAG, "XComponent registration failed");
                CleanupSession();
                return nullptr;
            }
        }
        return exports;
    }

    napi_module module = {1, 0, nullptr, Init, "{{LAUNCHER_LIB_NAME}}", nullptr, {0}};
} // namespace

extern "C" __attribute__((constructor)) void RegisterSkyLauncher()
{
    napi_module_register(&module);
}
