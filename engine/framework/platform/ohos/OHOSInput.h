#pragma once

#include <core/util/String.h>
#include <framework/window/IWindowEvent.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sky {

    enum class OHOSTouchAction { DOWN, MOVE, UP, CANCEL, END };

    struct OHOSMouseChange {
        enum class Type { MOTION, DOWN, UP, WHEEL, END };
        Type            type   = Type::MOTION;
        MouseButtonType button = MouseButtonType::LEFT;
        int32_t         x      = 0;
        int32_t         y      = 0;
        int32_t         relX   = 0;
        int32_t         relY   = 0;
        float           scroll = 0.f;
    };

    // The worker owns this state; native callbacks only enqueue validated pixel coordinates.
    class OHOSPointerState {
    public:
        std::vector<OHOSMouseChange> Touch(OHOSTouchAction action, int32_t identity, int32_t nextX, int32_t nextY)
        {
            std::vector<OHOSMouseChange> changes;
            if (action == OHOSTouchAction::CANCEL) {
                primary.reset();
                contacts.clear();
                UpdateButtons(mouseButtons, changes);
                positioned = false;
                return changes;
            }
            if (action < OHOSTouchAction::DOWN || action >= OHOSTouchAction::END) {
                return changes;
            }
            const auto iter = std::find(contacts.begin(), contacts.end(), identity);
            if (action == OHOSTouchAction::DOWN) {
                if (iter != contacts.end()) {
                    return changes;
                }
                // Existing secondary contacts cannot start a replacement drag when the primary lifts.
                if (contacts.empty()) {
                    primary = identity;
                    Move(nextX, nextY, changes);
                    UpdateButtons(mouseButtons | 1U, changes);
                }
                contacts.push_back(identity);
            } else if (action == OHOSTouchAction::MOVE) {
                if (primary && *primary == identity) {
                    Move(nextX, nextY, changes);
                }
            } else if (action == OHOSTouchAction::UP && iter != contacts.end()) {
                if (primary && *primary == identity) {
                    Move(nextX, nextY, changes);
                    primary.reset();
                    UpdateButtons(mouseButtons, changes);
                    positioned = false;
                }
                contacts.erase(iter);
            }
            return changes;
        }

        // Buttons use the engine's normalized left/right/middle bits (1, 2, 4).
        std::vector<OHOSMouseChange> Mouse(int32_t nextX, int32_t nextY, uint32_t nextButtons, float scroll = 0.f)
        {
            std::vector<OHOSMouseChange> changes;
            Move(nextX, nextY, changes);
            mouseButtons = nextButtons & 7U;
            UpdateButtons(mouseButtons | (primary ? 1U : 0U), changes);
            if (std::isfinite(scroll) && scroll != 0.f) {
                OHOSMouseChange change;
                change.type   = OHOSMouseChange::Type::WHEEL;
                change.x      = x;
                change.y      = y;
                change.scroll = scroll;
                changes.push_back(change);
            }
            return changes;
        }

        std::vector<OHOSMouseChange> Cancel()
        {
            std::vector<OHOSMouseChange> changes;
            UpdateButtons(0, changes);
            primary.reset();
            contacts.clear();
            mouseButtons = 0;
            positioned   = false;
            return changes;
        }

    private:
        static int32_t Delta(int32_t next, int32_t previous)
        {
            const int64_t difference = static_cast<int64_t>(next) - previous;
            return static_cast<int32_t>(std::clamp(difference, static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
                                                   static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
        }

        void Move(int32_t nextX, int32_t nextY, std::vector<OHOSMouseChange> &changes)
        {
            if (!positioned || nextX != x || nextY != y) {
                OHOSMouseChange change;
                change.x    = nextX;
                change.y    = nextY;
                change.relX = positioned ? Delta(nextX, x) : 0;
                change.relY = positioned ? Delta(nextY, y) : 0;
                changes.push_back(change);
            }
            x          = nextX;
            y          = nextY;
            positioned = true;
        }

        void UpdateButtons(uint32_t next, std::vector<OHOSMouseChange> &changes)
        {
            const MouseButtonType mapped[] = {MouseButtonType::LEFT, MouseButtonType::RIGHT, MouseButtonType::MIDDLE};
            for (uint32_t index = 0; index != 3; ++index) {
                const uint32_t bit = 1U << index;
                if ((buttons & bit) != (next & bit)) {
                    OHOSMouseChange change;
                    change.type   = (next & bit) != 0 ? OHOSMouseChange::Type::DOWN : OHOSMouseChange::Type::UP;
                    change.button = mapped[index];
                    change.x      = x;
                    change.y      = y;
                    changes.push_back(change);
                }
            }
            buttons = next;
        }

        std::optional<int32_t> primary;
        std::vector<int32_t>   contacts;
        int32_t                x            = 0;
        int32_t                y            = 0;
        uint32_t               buttons      = 0;
        uint32_t               mouseButtons = 0;
        bool                   positioned   = false;
    };

    class OHOSKeyState {
    public:
        bool Apply(ScanCode code, bool down, KeyModFlags modifiers)
        {
            if (code < ScanCode::KEY_A || code >= ScanCode::KEY_NUM) {
                return false;
            }
            const auto iter = std::find_if(pressed.begin(), pressed.end(), [code](const KeyboardEvent &event) { return event.scanCode == code; });
            if (down) {
                if (iter == pressed.end()) {
                    pressed.push_back({INVALID_WIN_ID, code, modifiers});
                } else {
                    iter->mod = modifiers;
                }
                return true;
            }
            if (iter == pressed.end()) {
                return false;
            }
            pressed.erase(iter);
            return true;
        }

        std::vector<KeyboardEvent> Cancel()
        {
            auto released = std::exchange(pressed, {});
            for (auto &event : released) {
                event.mod = {};
            }
            return released;
        }

    private:
        std::vector<KeyboardEvent> pressed;
    };

    // Native IME commits are deltas, so identical successive commits must both be delivered.
    inline std::string OHOSCommittedText(std::u16string_view text, bool composing = false)
    {
        if (composing) {
            return {};
        }
        auto result = Utf16ToUtf8(std::u16string(text));
        // Engine text callbacks accept C strings; embedded NUL cannot be delivered.
        result.erase(std::remove(result.begin(), result.end(), '\0'), result.end());
        return result;
    }

} // namespace sky
