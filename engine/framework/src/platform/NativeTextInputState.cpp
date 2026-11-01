#include "NativeTextInputState.h"

#include <core/logger/Logger.h>
#include <core/util/String.h>
#include <limits>
#include <utility>

namespace sky {
    namespace {
        bool ValidText(const std::string &text, uint32_t limit = 0)
        {
            if (text.find('\0') != std::string::npos)
                return false;
            const auto units = Utf8ToUtf16U(text);
            if (units.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()) || Utf16ToUtf8(units) != text)
                return false;
            size_t count = 0;
            for (const char16_t unit : units) {
                if (unit < 0xDC00 || unit > 0xDFFF)
                    ++count;
            }
            return limit == 0 || count <= limit;
        }
    } // namespace
    uint64_t NativeTextInputState::Begin(const NativeTextInputOptions &options, NativeTextInputCallback next)
    {
        std::lock_guard lock(mutex);
        if (!enabled || active != 0 || !next || (options.multiline && options.password) || !ValidText(options.text, options.maxLength) ||
            !ValidText(options.placeholder) || !ValidText(options.title) ||
            options.maxLength > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
            return 0;
        callback       = std::move(next);
        text           = options.text;
        maxLength      = options.maxLength;
        owner          = std::this_thread::get_id();
        active         = ++serial;
        terminalQueued = false;
        return active;
    }

    bool NativeTextInputState::Post(uint64_t token, NativeTextInputEvent event)
    {
        std::lock_guard lock(mutex);
        return PostLocked(token, std::move(event));
    }

    bool NativeTextInputState::Fail(uint64_t token, int32_t nativeError)
    {
        std::lock_guard lock(mutex);
        return PostLocked(token, {NativeTextInputEventType::ERROR, text, nativeError});
    }

    bool NativeTextInputState::PostLocked(uint64_t token, NativeTextInputEvent event)
    {
        if (token == 0 || token != active || terminalQueued || event.type < NativeTextInputEventType::CHANGED ||
            event.type >= NativeTextInputEventType::END)
            return false;
        if (!ValidText(event.text, maxLength)) {
            if (event.type == NativeTextInputEventType::CHANGED)
                return false;
            // Native controls may already be closed; rejected terminal text must still complete the request.
            event.nativeError = event.type == NativeTextInputEventType::ERROR && event.nativeError != 0 ? event.nativeError : -2;
            event.type        = NativeTextInputEventType::ERROR;
            event.text        = text;
        }
        if (event.type == NativeTextInputEventType::CHANGED && event.text == text)
            return true;
        terminalQueued = event.type != NativeTextInputEventType::CHANGED;
        text           = event.text;
        events.push_back({token, std::move(event)});
        return true;
    }

    void NativeTextInputState::Dispatch()
    {
        for (;;) {
            NativeTextInputCallback notify;
            NativeTextInputEvent    event;
            {
                std::lock_guard lock(mutex);
                if (events.empty())
                    return;
                if (owner != std::this_thread::get_id()) {
                    LOG_E("NativeTextInput", "Dialog callbacks require the engine owner thread");
                    return;
                }
                auto pending = std::move(events.front());
                events.pop_front();
                if (pending.token != active)
                    continue;
                event  = std::move(pending.event);
                notify = callback;
                if (event.type != NativeTextInputEventType::CHANGED) {
                    active   = 0;
                    callback = {};
                    text.clear();
                }
            }
            notify(event);
        }
    }

    void NativeTextInputState::Cancel()
    {
        RequestCancel();
        Dispatch();
    }

    void NativeTextInputState::RequestCancel()
    {
        {
            std::lock_guard lock(mutex);
            if (active == 0)
                return;
            if (!terminalQueued) {
                terminalQueued = true;
                events.push_back({active, {NativeTextInputEventType::CANCELLED, text, 0}});
            }
        }
    }

    uint64_t NativeTextInputState::ActiveToken() const
    {
        std::lock_guard lock(mutex);
        return active;
    }

    void NativeTextInputState::SetEnabled(bool value)
    {
        std::lock_guard lock(mutex);
        enabled = value;
    }
} // namespace sky
