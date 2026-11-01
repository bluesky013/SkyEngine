#pragma once

#include <framework/platform/NativeTextInput.h>

namespace sky {
    bool ShowOHOSTextInputDialog(const NativeTextInputOptions &options, NativeTextInputCallback callback);
    void HideOHOSTextInputDialog();
    void PollOHOSTextInputDialog();
} // namespace sky
