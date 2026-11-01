#pragma once

#include <core/environment/Singleton.h>
#include <framework/platform/NativeTextInput.h>

#include <deque>
#include <mutex>
#include <thread>

namespace sky {
    // UI callbacks retain only a token; the process receiver outlives platforms and runtime sessions.
    class NativeTextInputState final : public Singleton<NativeTextInputState> {
    public:
        uint64_t Begin(const NativeTextInputOptions &options, NativeTextInputCallback callback);
        bool     Post(uint64_t token, NativeTextInputEvent event);
        bool     Fail(uint64_t token, int32_t nativeError);
        void     Dispatch();
        void     Cancel();
        void     RequestCancel();
        uint64_t ActiveToken() const;
        void     SetEnabled(bool value);

    private:
        bool PostLocked(uint64_t token, NativeTextInputEvent event);
        struct Pending {
            uint64_t             token;
            NativeTextInputEvent event;
        };
        mutable std::mutex      mutex;
        std::deque<Pending>     events;
        NativeTextInputCallback callback;
        std::string             text;
        std::thread::id         owner;
        uint64_t                serial         = 0;
        uint64_t                active         = 0;
        uint32_t                maxLength      = 0;
        bool                    terminalQueued = false;
        bool                    enabled        = true;
    };
} // namespace sky
