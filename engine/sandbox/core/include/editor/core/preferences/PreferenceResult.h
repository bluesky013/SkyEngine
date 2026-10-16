#pragma once

#include <string>

namespace sky::editor {

    enum class PreferenceCode {
        Success,
        NotFound,
        ReadFailed,
        WriteFailed,
        InvalidJson,
        InvalidData,
        UnsupportedType,
        ValidationFailed,
        ApplyFailed,
        Conflict,
        Busy,
    };

    struct PreferenceResult {
        PreferenceCode code = PreferenceCode::Success;
        std::string    pageId;
        std::string    path;
        std::string    message;
        // Effects of THIS operation. Failure does not imply rollback. Setter/hook
        // flags become true on entry, because even rejection/throw may have
        // external effects; they do not assert that a value actually changed.
        bool storageWritten     = false;
        bool liveWritten        = false;
        bool applicationApplied = false;

        bool Succeeded() const
        {
            return code == PreferenceCode::Success;
        }
        bool HasPartialEffects() const
        {
            return !Succeeded() && (storageWritten || liveWritten || applicationApplied);
        }
    };

} // namespace sky::editor
