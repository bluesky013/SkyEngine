#pragma once

#include <framework/platform/NativeTextInput.h>

namespace sky::ios {
    bool ShowNativeTextInput(void *scene, const NativeTextInputOptions &options, NativeTextInputCallback callback, void *&dialog);
    void HideNativeTextInput(void *&dialog);
    void DispatchNativeTextInput(void *dialog);
} // namespace sky::ios
