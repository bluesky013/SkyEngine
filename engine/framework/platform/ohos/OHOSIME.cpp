#include "OHOSIME.h"
#include "OHOSInput.h"

#include <core/logger/Logger.h>
#include <framework/window/IWindowEvent.h>
#include <inputmethod/inputmethod_controller_capi.h>
#include <inputmethod/inputmethod_cursor_info_capi.h>
#include <inputmethod/inputmethod_text_config_capi.h>

#include <algorithm>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <utility>

namespace sky {
    namespace {
        constexpr const char *TAG         = "OHOSIME";
        constexpr size_t      MAX_CONTEXT = 8192;

        bool Check(InputMethod_ErrorCode code, const char *operation)
        {
            if (code == IME_ERR_OK) {
                return true;
            }
            LOG_E(TAG, "%s failed: %d", operation, static_cast<int>(code));
            return false;
        }

        bool High(char16_t unit)
        {
            return unit >= 0xD800 && unit <= 0xDBFF;
        }

        bool Low(char16_t unit)
        {
            return unit >= 0xDC00 && unit <= 0xDFFF;
        }

        size_t Previous(const std::u16string &text, size_t cursor)
        {
            return cursor == 0 ? 0 : cursor > 1 && Low(text[cursor - 1]) && High(text[cursor - 2]) ? cursor - 2 : cursor - 1;
        }

        size_t Next(const std::u16string &text, size_t cursor)
        {
            return cursor == text.size() ? cursor : cursor + 1 < text.size() && High(text[cursor]) && Low(text[cursor + 1]) ? cursor + 2 : cursor + 1;
        }
    } // namespace

    struct OHOSIME::Impl {
        bool Matches(InputMethod_TextEditorProxy *value) const
        {
            return accepting && editor == value;
        }

        bool Matches(InputMethod_TextEditorProxy *value, uint64_t token) const
        {
            return Matches(value) && generation == token;
        }

        uint64_t Capture(InputMethod_TextEditorProxy *value)
        {
            std::lock_guard lock(mutex);
            return Matches(value) ? generation : 0;
        }

        static Impl &Receiver()
        {
            return *OHOSIME::Get()->impl;
        }

        static void TrimContext(std::u16string &text, size_t &position)
        {
            if (text.size() <= MAX_CONTEXT) {
                return;
            }
            size_t remove = std::min(position, text.size() - MAX_CONTEXT);
            if (remove != 0 && remove < text.size() && Low(text[remove]) && High(text[remove - 1])) {
                ++remove;
            }
            text.erase(0, remove);
            position -= std::min(position, remove);
            if (text.size() > MAX_CONTEXT) {
                size_t end = MAX_CONTEXT;
                if (High(text[end - 1]) && Low(text[end])) {
                    --end;
                }
                text.resize(end);
                position = std::min(position, end);
            }
        }

        std::pair<std::u16string, size_t> Context() const
        {
            std::u16string text = buffer;
            text.insert(cursor, preview);
            size_t position = cursor + preview.size();
            TrimContext(text, position);
            return {std::move(text), position};
        }

        bool PublishSelection(InputMethod_TextEditorProxy *source, uint64_t token, bool wait = false)
        {
            std::unique_lock notification(notificationMutex, std::defer_lock);
            {
                std::lock_guard lock(mutex);
                if (!Matches(source, token)) {
                    return false;
                }
                selectionPending = true;
                if (!notification.try_lock() && !wait) {
                    return true;
                }
            }
            if (!notification.owns_lock()) {
                notification.lock();
            }
            bool result = true;
            while (true) {
                InputMethod_InputMethodProxy *proxy = nullptr;
                std::u16string                text;
                int32_t                       position = 0;
                {
                    std::lock_guard lock(mutex);
                    if (!Matches(source, token) || input == nullptr || !selectionPending) {
                        // Unlock while holding the state mutex so a coalesced update cannot lose its publisher.
                        notification.unlock();
                        return Matches(source, token) && result;
                    }
                    selectionPending = false;
                    proxy            = input;
                    auto context     = Context();
                    text             = std::move(context.first);
                    position         = static_cast<int32_t>(context.second);
                    ++nativeCalls;
                }
                result =
                    Check(OH_InputMethodProxy_NotifySelectionChange(proxy, text.data(), text.size(), position, position), "NotifySelectionChange") &&
                    result;
                {
                    std::lock_guard lock(mutex);
                    --nativeCalls;
                    drained.notify_all();
                }
            }
        }

        static void GetConfig(InputMethod_TextEditorProxy *source, InputMethod_TextConfig *config)
        {
            auto           &self = Receiver();
            std::lock_guard lock(self.mutex);
            if (!self.Matches(source)) {
                return;
            }
            if (config == nullptr) {
                self.configValid = false;
                LOG_E(TAG, "IME requested a null text configuration");
                return;
            }
            bool valid             = Check(OH_TextConfig_SetInputType(config, IME_TEXT_INPUT_TYPE_TEXT), "SetInputType");
            valid                  = Check(OH_TextConfig_SetEnterKeyType(config, IME_ENTER_KEY_DONE), "SetEnterKeyType") && valid;
            valid                  = Check(OH_TextConfig_SetPreviewTextSupport(config, true), "SetPreviewTextSupport") && valid;
            valid                  = Check(OH_TextConfig_SetConsumeKeyEvents(config, false), "SetConsumeKeyEvents") && valid;
            const int32_t position = static_cast<int32_t>(self.Context().second);
            valid                  = Check(OH_TextConfig_SetSelection(config, position, position), "SetSelection") && valid;
            if (self.windowId != 0) {
                valid = Check(OH_TextConfig_SetWindowId(config, static_cast<int32_t>(self.windowId)), "SetWindowId") && valid;
            }
            InputMethod_CursorInfo *info      = nullptr;
            const bool              gotCursor = Check(OH_TextConfig_GetCursorInfo(config, &info), "GetCursorInfo");
            valid                             = gotCursor && info != nullptr && valid;
            if (gotCursor && info == nullptr) {
                LOG_E(TAG, "Text configuration returned no cursor info");
            }
            if (gotCursor && info != nullptr) {
                valid = Check(OH_CursorInfo_SetRect(info, self.x, self.y, self.width, self.height), "SetCursorRect") && valid;
            }
            self.configValid = self.configValid && valid;
        }

        static void Insert(InputMethod_TextEditorProxy *source, const char16_t *text, size_t length)
        {
            if (text == nullptr || length == 0 || length > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
                if (length != 0) {
                    LOG_E(TAG, "Invalid IME commit buffer");
                }
                return;
            }
            auto            &self  = Receiver();
            const uint64_t   token = self.Capture(source);
            std::unique_lock delivery(self.deliveryMutex);
            TextSink         sink;
            std::u16string   committed = Utf8ToUtf16U(OHOSCommittedText(std::u16string_view(text, length)));
            {
                std::lock_guard lock(self.mutex);
                if (!self.Matches(source, token)) {
                    return;
                }
                self.preview.clear();
                self.buffer.insert(self.cursor, committed);
                self.cursor += committed.size();
                TrimContext(self.buffer, self.cursor);
                sink = self.textSink;
            }
            if (sink) {
                sink(std::move(committed));
            }
            delivery.unlock();
            self.PublishSelection(source, token);
        }

        static void Delete(InputMethod_TextEditorProxy *source, int32_t length, bool before)
        {
            if (length <= 0) {
                if (length < 0) {
                    LOG_E(TAG, "Invalid negative IME deletion length");
                }
                return;
            }
            auto            &self  = Receiver();
            const uint64_t   token = self.Capture(source);
            std::unique_lock delivery(self.deliveryMutex);
            KeySink          sink;
            uint32_t         count = 0;
            {
                std::lock_guard lock(self.mutex);
                if (!self.Matches(source, token)) {
                    return;
                }
                size_t remaining = static_cast<size_t>(length);
                if (before && !self.preview.empty()) {
                    size_t start = self.preview.size();
                    while (start != 0 && self.preview.size() - start < remaining) {
                        start = Previous(self.preview, start);
                    }
                    remaining -= std::min(remaining, self.preview.size() - start);
                    self.preview.resize(start);
                }
                const size_t origin   = self.cursor;
                size_t       boundary = origin;
                while ((before ? boundary != 0 : boundary != self.buffer.size()) && (before ? origin - boundary : boundary - origin) < remaining) {
                    boundary = before ? Previous(self.buffer, boundary) : Next(self.buffer, boundary);
                    ++count;
                }
                const size_t removed = before ? origin - boundary : boundary - origin;
                count += static_cast<uint32_t>(remaining - std::min(remaining, removed));
                self.buffer.erase(before ? boundary : origin, removed);
                if (before) {
                    self.cursor = boundary;
                }
                sink = self.keySink;
            }
            if (sink && count != 0) {
                sink(before ? ScanCode::KEY_BACKSPACE : ScanCode::KEY_DELETE, count);
            }
            delivery.unlock();
            self.PublishSelection(source, token);
        }

        static void DeleteForward(InputMethod_TextEditorProxy *source, int32_t length)
        {
            Delete(source, length, true);
        }

        static void DeleteBackward(InputMethod_TextEditorProxy *source, int32_t length)
        {
            Delete(source, length, false);
        }

        static void Enter(InputMethod_TextEditorProxy *source, InputMethod_EnterKeyType type)
        {
            switch (type) {
            case IME_ENTER_KEY_UNSPECIFIED:
            case IME_ENTER_KEY_NONE:
            case IME_ENTER_KEY_GO:
            case IME_ENTER_KEY_SEARCH:
            case IME_ENTER_KEY_SEND:
            case IME_ENTER_KEY_NEXT:
            case IME_ENTER_KEY_DONE:
            case IME_ENTER_KEY_PREVIOUS:
            case IME_ENTER_KEY_NEWLINE: break;
            default: LOG_E(TAG, "Unsupported IME enter action: %d", static_cast<int>(type)); return;
            }
            auto           &self  = Receiver();
            const uint64_t  token = self.Capture(source);
            std::lock_guard delivery(self.deliveryMutex);
            KeySink         sink;
            {
                std::lock_guard lock(self.mutex);
                if (self.Matches(source, token)) {
                    sink = self.keySink;
                }
            }
            if (sink) {
                sink(ScanCode::KEY_RETURN, 1);
            }
        }

        static void MoveCursor(InputMethod_TextEditorProxy *source, InputMethod_Direction direction)
        {
            ScanCode code = ScanCode::KEY_NUM;
            switch (direction) {
            case IME_DIRECTION_LEFT: code = ScanCode::KEY_LEFT; break;
            case IME_DIRECTION_RIGHT: code = ScanCode::KEY_RIGHT; break;
            case IME_DIRECTION_UP: code = ScanCode::KEY_UP; break;
            case IME_DIRECTION_DOWN: code = ScanCode::KEY_DOWN; break;
            case IME_DIRECTION_NONE: return;
            default: LOG_E(TAG, "Unsupported IME cursor direction: %d", static_cast<int>(direction)); return;
            }
            auto            &self  = Receiver();
            const uint64_t   token = self.Capture(source);
            std::unique_lock delivery(self.deliveryMutex);
            KeySink          sink;
            {
                std::lock_guard lock(self.mutex);
                if (!self.Matches(source, token)) {
                    return;
                }
                self.preview.clear();
                if (direction == IME_DIRECTION_LEFT) {
                    self.cursor = Previous(self.buffer, self.cursor);
                } else if (direction == IME_DIRECTION_RIGHT) {
                    self.cursor = Next(self.buffer, self.cursor);
                } else {
                    self.buffer.clear();
                    self.cursor = 0;
                }
                sink = self.keySink;
            }
            if (sink) {
                sink(code, 1);
            }
            delivery.unlock();
            self.PublishSelection(source, token);
        }

        static void SetSelection(InputMethod_TextEditorProxy *source, int32_t start, int32_t end)
        {
            auto           &self = Receiver();
            std::lock_guard lock(self.mutex);
            if (self.Matches(source) && (start != end || start < 0 || static_cast<size_t>(start) != self.cursor)) {
                LOG_E(TAG, "IME selection changes are unsupported by the committed-text interface");
            }
        }

        static void Extend(InputMethod_TextEditorProxy *source, InputMethod_ExtendAction)
        {
            auto           &self = Receiver();
            std::lock_guard lock(self.mutex);
            if (self.Matches(source)) {
                LOG_E(TAG, "IME clipboard/selection actions are unsupported by the committed-text interface");
            }
        }

        static void GetText(InputMethod_TextEditorProxy *source, int32_t number, char16_t *output, size_t *length, bool before)
        {
            if (length == nullptr) {
                return;
            }
            *length = 0;
            if (output == nullptr || number <= 0) {
                return;
            }
            auto           &self = Receiver();
            std::lock_guard lock(self.mutex);
            if (!self.Matches(source)) {
                return;
            }
            const auto [text, cursor] = self.Context();
            const size_t available    = before ? cursor : text.size() - cursor;
            size_t       count        = std::min(available, static_cast<size_t>(number));
            size_t       begin        = before ? cursor - count : cursor;
            if (before && begin != 0 && count != 0 && Low(text[begin]) && High(text[begin - 1])) {
                ++begin;
                --count;
            } else if (!before && count != 0 && begin + count < text.size() && High(text[begin + count - 1]) && Low(text[begin + count])) {
                --count;
            }
            std::copy_n(text.data() + begin, count, output);
            *length = count;
        }

        static void Left(InputMethod_TextEditorProxy *source, int32_t number, char16_t *output, size_t *length)
        {
            GetText(source, number, output, length, true);
        }

        static void Right(InputMethod_TextEditorProxy *source, int32_t number, char16_t *output, size_t *length)
        {
            GetText(source, number, output, length, false);
        }

        static int32_t Cursor(InputMethod_TextEditorProxy *source)
        {
            auto           &self = Receiver();
            std::lock_guard lock(self.mutex);
            return self.Matches(source) ? static_cast<int32_t>(self.Context().second) : 0;
        }

        static int32_t Preview(InputMethod_TextEditorProxy *source, const char16_t *text, size_t length, int32_t start, int32_t end)
        {
            auto          &self  = Receiver();
            const uint64_t token = self.Capture(source);
            {
                std::lock_guard delivery(self.deliveryMutex);
                std::lock_guard lock(self.mutex);
                if (!self.Matches(source, token)) {
                    return IME_ERR_DETACHED;
                }
                if ((text == nullptr && length != 0) || length > MAX_CONTEXT ||
                    !((start == -1 && end == -1) || (start == end && start >= 0 && static_cast<size_t>(start) == self.cursor))) {
                    return IME_ERR_PARAMCHECK;
                }
                self.preview = length != 0 ? std::u16string(text, length) : std::u16string{};
            }
            return self.PublishSelection(source, token) ? IME_ERR_OK : IME_ERR_IMCLIENT;
        }

        static void FinishPreview(InputMethod_TextEditorProxy *source)
        {
            auto            &self  = Receiver();
            const uint64_t   token = self.Capture(source);
            std::unique_lock delivery(self.deliveryMutex);
            TextSink         sink;
            std::u16string   committed;
            {
                std::lock_guard lock(self.mutex);
                if (!self.Matches(source, token) || self.preview.empty()) {
                    return;
                }
                committed = Utf8ToUtf16U(OHOSCommittedText(std::exchange(self.preview, {})));
                self.buffer.insert(self.cursor, committed);
                self.cursor += committed.size();
                TrimContext(self.buffer, self.cursor);
                sink = self.textSink;
            }
            if (sink) {
                sink(std::move(committed));
            }
            delivery.unlock();
            self.PublishSelection(source, token);
        }

        static void KeyboardStatus(InputMethod_TextEditorProxy *source, InputMethod_KeyboardStatus status)
        {
            auto           &self  = Receiver();
            const uint64_t  token = self.Capture(source);
            std::lock_guard delivery(self.deliveryMutex);
            std::lock_guard lock(self.mutex);
            if (self.Matches(source, token) && status == IME_KEYBOARD_STATUS_HIDE) {
                self.preview.clear();
            }
        }

        static int32_t PrivateCommand(InputMethod_TextEditorProxy *, InputMethod_PrivateCommand **, size_t)
        {
            return IME_ERR_UNDEFINED;
        }

        bool Register(InputMethod_TextEditorProxy *value)
        {
            return Check(OH_TextEditorProxy_SetCallbackInMainThread(value, false), "SetCallbackInMainThread") &&
                   Check(OH_TextEditorProxy_SetGetTextConfigFunc(value, GetConfig), "SetGetTextConfigFunc") &&
                   Check(OH_TextEditorProxy_SetInsertTextFunc(value, Insert), "SetInsertTextFunc") &&
                   Check(OH_TextEditorProxy_SetDeleteForwardFunc(value, DeleteForward), "SetDeleteForwardFunc") &&
                   Check(OH_TextEditorProxy_SetDeleteBackwardFunc(value, DeleteBackward), "SetDeleteBackwardFunc") &&
                   Check(OH_TextEditorProxy_SetSendKeyboardStatusFunc(value, KeyboardStatus), "SetSendKeyboardStatusFunc") &&
                   Check(OH_TextEditorProxy_SetSendEnterKeyFunc(value, Enter), "SetSendEnterKeyFunc") &&
                   Check(OH_TextEditorProxy_SetMoveCursorFunc(value, MoveCursor), "SetMoveCursorFunc") &&
                   Check(OH_TextEditorProxy_SetHandleSetSelectionFunc(value, SetSelection), "SetHandleSetSelectionFunc") &&
                   Check(OH_TextEditorProxy_SetHandleExtendActionFunc(value, Extend), "SetHandleExtendActionFunc") &&
                   Check(OH_TextEditorProxy_SetGetLeftTextOfCursorFunc(value, Left), "SetGetLeftTextOfCursorFunc") &&
                   Check(OH_TextEditorProxy_SetGetRightTextOfCursorFunc(value, Right), "SetGetRightTextOfCursorFunc") &&
                   Check(OH_TextEditorProxy_SetGetTextIndexAtCursorFunc(value, Cursor), "SetGetTextIndexAtCursorFunc") &&
                   Check(OH_TextEditorProxy_SetReceivePrivateCommandFunc(value, PrivateCommand), "SetReceivePrivateCommandFunc") &&
                   Check(OH_TextEditorProxy_SetSetPreviewTextFunc(value, Preview), "SetSetPreviewTextFunc") &&
                   Check(OH_TextEditorProxy_SetFinishTextPreviewFunc(value, FinishPreview), "SetFinishTextPreviewFunc");
        }

        // Called with operationMutex; never hold the callback mutex across attach/detach.
        bool StopSession()
        {
            InputMethod_TextEditorProxy  *oldEditor   = nullptr;
            InputMethod_InputMethodProxy *oldInput    = nullptr;
            bool                          wasAttached = false;
            {
                std::unique_lock lock(mutex);
                accepting = false;
                ++generation;
                textSink = {};
                keySink  = {};
                preview.clear();
                buffer.clear();
                cursor = 0;
                drained.wait(lock, [this] { return nativeCalls == 0; });
                oldEditor   = editor;
                oldInput    = input;
                wasAttached = attached;
            }
            bool hidden = true;
            if (wasAttached) {
                if (oldInput == nullptr) {
                    LOG_E(TAG, "Attached IME returned no controllable input proxy");
                    return false;
                }
                hidden = Check(OH_InputMethodProxy_HideKeyboard(oldInput), "HideKeyboard");
                if (!Check(OH_InputMethodController_Detach(oldInput), "Detach")) {
                    return false;
                }
            }
            {
                std::lock_guard lock(mutex);
                editor   = nullptr;
                input    = nullptr;
                attached = false;
            }
            if (oldEditor != nullptr) {
                OH_TextEditorProxy_Destroy(oldEditor);
            }
            return hidden;
        }

        bool Start(uint32_t id, TextSink text, KeySink key)
        {
            if (!text || !key || id > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
                return false;
            }
            std::lock_guard operationLock(operationMutex);
            if (!StopSession()) {
                return false;
            }
            auto *created = OH_TextEditorProxy_Create();
            auto *options = OH_AttachOptions_Create(true);
            if (created == nullptr || options == nullptr || !Register(created)) {
                if (created != nullptr) {
                    OH_TextEditorProxy_Destroy(created);
                }
                if (options != nullptr) {
                    OH_AttachOptions_Destroy(options);
                }
                LOG_E(TAG, "IME allocation or callback registration failed");
                return false;
            }
            {
                std::lock_guard lock(mutex);
                editor      = created;
                windowId    = id;
                textSink    = std::move(text);
                keySink     = std::move(key);
                configValid = true;
                accepting   = true;
            }
            InputMethod_InputMethodProxy *createdInput = nullptr;
            const bool                    success      = Check(OH_InputMethodController_Attach(created, options, &createdInput), "Attach");
            OH_AttachOptions_Destroy(options);
            bool valid = false;
            {
                std::lock_guard lock(mutex);
                input    = createdInput;
                attached = success;
                valid    = success && input != nullptr && configValid;
            }
            if (!valid) {
                if (!StopSession()) {
                    LOG_E(TAG, "IME attach failure cleanup remains pending");
                }
                return false;
            }
            if (!PublishSelection(created, Capture(created), true)) {
                if (!StopSession()) {
                    LOG_E(TAG, "IME initial selection failure cleanup remains pending");
                }
                return false;
            }
            return true;
        }

        bool ShowKeyboard()
        {
            std::lock_guard               operationLock(operationMutex);
            InputMethod_InputMethodProxy *proxy = nullptr;
            {
                std::lock_guard lock(mutex);
                if (!accepting || !attached || input == nullptr) {
                    return false;
                }
                proxy = input;
            }
            return Check(OH_InputMethodProxy_ShowKeyboard(proxy), "ShowKeyboard");
        }

        bool SetRect(int32_t left, int32_t top, int32_t w, int32_t h)
        {
            if (w < 0 || h < 0) {
                return false;
            }
            std::lock_guard               operationLock(operationMutex);
            InputMethod_InputMethodProxy *proxy = nullptr;
            {
                std::lock_guard lock(mutex);
                x      = left;
                y      = top;
                width  = w;
                height = h;
                if (accepting) {
                    proxy = input;
                }
            }
            if (proxy == nullptr) {
                return true;
            }
            auto *info = OH_CursorInfo_Create(left, top, w, h);
            if (info == nullptr) {
                LOG_E(TAG, "Cursor info allocation failed");
                return false;
            }
            const bool result = Check(OH_InputMethodProxy_NotifyCursorUpdate(proxy, info), "NotifyCursorUpdate");
            OH_CursorInfo_Destroy(info);
            return result;
        }

        // Stop never takes deliveryMutex; SDK IPC runs without the callback state/delivery locks.
        std::mutex                    operationMutex;
        std::mutex                    mutex;
        std::mutex                    deliveryMutex;
        std::mutex                    notificationMutex;
        std::condition_variable       drained;
        InputMethod_TextEditorProxy  *editor = nullptr;
        InputMethod_InputMethodProxy *input  = nullptr;
        TextSink                      textSink;
        KeySink                       keySink;
        // The engine exposes deltas, not the editor's full value; only session-local context can be mirrored.
        std::u16string buffer;
        std::u16string preview;
        size_t         cursor           = 0;
        uint32_t       windowId         = 0;
        uint32_t       nativeCalls      = 0;
        uint64_t       generation       = 1;
        int32_t        x                = 0;
        int32_t        y                = 0;
        int32_t        width            = 1;
        int32_t        height           = 1;
        bool           accepting        = false;
        bool           attached         = false;
        bool           configValid      = true;
        bool           selectionPending = false;
    };

    OHOSIME::OHOSIME() : impl(std::make_unique<Impl>())
    {
    }

    OHOSIME::~OHOSIME()
    {
        Stop();
    }

    bool OHOSIME::Start(uint32_t windowId, TextSink text, KeySink key)
    {
        return impl->Start(windowId, std::move(text), std::move(key));
    }

    bool OHOSIME::Stop()
    {
        std::lock_guard lock(impl->operationMutex);
        return impl->StopSession();
    }

    bool OHOSIME::ShowKeyboard()
    {
        return impl->ShowKeyboard();
    }

    bool OHOSIME::SetRect(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        return impl->SetRect(x, y, width, height);
    }
} // namespace sky
