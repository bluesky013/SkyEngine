#include "OHOSDialog.h"
#include "platform/NativeTextInputState.h"

#include <core/logger/Logger.h>
#include <framework/platform/OHOSHost.h>
#include <framework/platform/OHOSNativeTextInput.h>

#include <mutex>
#include <utility>

namespace sky {
    namespace {
        struct DialogCommand {
            bool                   show;
            uint64_t               token;
            NativeTextInputOptions options;
        };

        bool SetString(napi_env env, napi_value object, const char *name, const std::string &text)
        {
            napi_value value = nullptr;
            return napi_create_string_utf8(env, text.data(), text.size(), &value) == napi_ok &&
                   napi_set_named_property(env, object, name, value) == napi_ok;
        }

        bool SetBool(napi_env env, napi_value object, const char *name, bool value)
        {
            napi_value native = nullptr;
            return napi_get_boolean(env, value, &native) == napi_ok && napi_set_named_property(env, object, name, native) == napi_ok;
        }

        void CallUI(napi_env env, napi_value callback, void *, void *data)
        {
            std::unique_ptr<DialogCommand> command(static_cast<DialogCommand *>(data));
            if (env == nullptr || callback == nullptr)
                return;
            napi_value  object   = nullptr;
            napi_value  maximum  = nullptr;
            napi_value  receiver = nullptr;
            napi_value  result   = nullptr;
            const auto &options  = command->options;
            const bool  prepared = napi_create_object(env, &object) == napi_ok && SetBool(env, object, "show", command->show) &&
                                  SetString(env, object, "token", std::to_string(command->token)) && SetString(env, object, "text", options.text) &&
                                  SetString(env, object, "placeholder", options.placeholder) && SetString(env, object, "title", options.title) &&
                                  SetBool(env, object, "multiline", options.multiline) && SetBool(env, object, "password", options.password) &&
                                  napi_create_uint32(env, options.maxLength, &maximum) == napi_ok &&
                                  napi_set_named_property(env, object, "maxLength", maximum) == napi_ok &&
                                  napi_get_undefined(env, &receiver) == napi_ok;
            const napi_status status = prepared ? napi_call_function(env, receiver, callback, 1, &object, &result) : napi_generic_failure;
            if (status != napi_ok) {
                bool pending = false;
                if (napi_is_exception_pending(env, &pending) == napi_ok && pending) {
                    napi_value error = nullptr;
                    if (napi_get_and_clear_last_exception(env, &error) != napi_ok) {
                        LOG_E("OHOSDialog", "Failed to clear the native UI error");
                    }
                }
                LOG_E("OHOSDialog", "Native text input UI command failed: %d", static_cast<int>(status));
                OHOSFailTextInput(command->token, static_cast<int32_t>(status));
            }
        }

        class DialogBridge final : public Singleton<DialogBridge> {
        public:
            bool Register(napi_env env, napi_value callback)
            {
                std::lock_guard lock(mutex);
                if (owner != nullptr && owner != env)
                    return false;
                if (function != nullptr)
                    return false;
                napi_valuetype type = napi_undefined;
                napi_value     name = nullptr;
                if (napi_typeof(env, callback, &type) != napi_ok || type != napi_function ||
                    napi_create_string_utf8(env, "SkyTextInput", NAPI_AUTO_LENGTH, &name) != napi_ok)
                    return false;
                if (napi_create_threadsafe_function(env, callback, nullptr, name, 0, 1, nullptr, nullptr, nullptr, CallUI, &function) != napi_ok) {
                    return false;
                }
                if (napi_unref_threadsafe_function(env, function) != napi_ok) {
                    if (napi_release_threadsafe_function(function, napi_tsfn_abort) != napi_ok) {
                        LOG_E("OHOSDialog", "Failed to release an unavailable UI bridge");
                    }
                    function = nullptr;
                    return false;
                }
                owner = env;
                return true;
            }

            void Release(napi_env env)
            {
                napi_threadsafe_function previous = nullptr;
                {
                    std::lock_guard lock(mutex);
                    if (owner != env)
                        return;
                    previous = std::exchange(function, nullptr);
                    owner    = nullptr;
                }
                if (previous != nullptr && napi_release_threadsafe_function(previous, napi_tsfn_abort) != napi_ok) {
                    LOG_E("OHOSDialog", "Failed to release text input UI bridge");
                }
                NativeTextInputState::Get()->RequestCancel();
                OHOSHost::Get()->NotifyTextInputDialog();
            }

            bool Send(std::unique_ptr<DialogCommand> command)
            {
                napi_threadsafe_function target = nullptr;
                {
                    std::lock_guard lock(mutex);
                    if (function == nullptr || napi_acquire_threadsafe_function(function) != napi_ok)
                        return false;
                    target = function;
                }
                const napi_status sent = napi_call_threadsafe_function(target, command.get(), napi_tsfn_nonblocking);
                if (sent == napi_ok)
                    command.release();
                if (napi_release_threadsafe_function(target, napi_tsfn_release) != napi_ok) {
                    LOG_E("OHOSDialog", "Failed to release a text input producer");
                }
                return sent == napi_ok;
            }

        private:
            std::mutex               mutex;
            napi_env                 owner    = nullptr;
            napi_threadsafe_function function = nullptr;
        };
    } // namespace

    bool OHOSRegisterTextInputHandler(napi_env environment, napi_value callback)
    {
        return DialogBridge::Get()->Register(environment, callback);
    }

    void OHOSReleaseTextInputHandler(napi_env environment)
    {
        DialogBridge::Get()->Release(environment);
    }

    bool OHOSPostTextInputEvent(uint64_t token, NativeTextInputEvent event)
    {
        if (!NativeTextInputState::Get()->Post(token, std::move(event)))
            return false;
        OHOSHost::Get()->NotifyTextInputDialog();
        return true;
    }

    bool OHOSFailTextInput(uint64_t token, int32_t nativeError)
    {
        if (!NativeTextInputState::Get()->Fail(token, nativeError))
            return false;
        OHOSHost::Get()->NotifyTextInputDialog();
        return true;
    }

    bool ShowOHOSTextInputDialog(const NativeTextInputOptions &options, NativeTextInputCallback callback)
    {
        const uint64_t token = NativeTextInputState::Get()->Begin(options, std::move(callback));
        if (token == 0)
            return false;
        if (!DialogBridge::Get()->Send(std::make_unique<DialogCommand>(DialogCommand{true, token, options}))) {
            OHOSPostTextInputEvent(token, {NativeTextInputEventType::ERROR, options.text, -1});
        }
        return true;
    }

    void HideOHOSTextInputDialog()
    {
        auto          *state = NativeTextInputState::Get();
        const uint64_t token = state->ActiveToken();
        if (token != 0 && !DialogBridge::Get()->Send(std::make_unique<DialogCommand>(DialogCommand{false, token, {}}))) {
            LOG_E("OHOSDialog", "Native text input UI dismissal could not be queued");
        }
        state->Cancel();
    }

    void PollOHOSTextInputDialog()
    {
        NativeTextInputState::Get()->Dispatch();
    }
} // namespace sky
