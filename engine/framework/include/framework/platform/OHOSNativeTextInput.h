#pragma once

#include <framework/platform/NativeTextInput.h>
#include <napi/native_api.h>

namespace sky {
    bool OHOSRegisterTextInputHandler(napi_env environment, napi_value callback);
    void OHOSReleaseTextInputHandler(napi_env environment);
    bool OHOSPostTextInputEvent(uint64_t token, NativeTextInputEvent event);
    bool OHOSFailTextInput(uint64_t token, int32_t nativeError);
} // namespace sky
