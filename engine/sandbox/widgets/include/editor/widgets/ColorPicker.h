//
// Created on 2026/10/04.
//

#pragma once

#include <ui/UIDrawData.h>
#include <ui/UIEvent.h>
#include <ui/UIRect.h>

#include <functional>

namespace sky::ui {
    class UIPaintContext;
    struct UIPointerEvent;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class UiSkin;

    // Plain component color (no engine type dependency); the host maps it to
    // whatever reflected struct/member order it has.
    struct ColorRGBA {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    // Blender-style reusable color picker popup: a generated hue/saturation disc
    // (textured, so colors are continuous), a value bar, an alpha bar, and
    // H/S/V + R/G/B/A readouts. Decoupled from any data model via callbacks.
    class ColorPicker {
    public:
        using LiveFn   = std::function<void(const ColorRGBA &)>; // live preview (no command)
        using CommitFn = std::function<void(const ColorRGBA &)>; // undoable commit

        ColorPicker()                               = default;
        ~ColorPicker()                              = default;
        ColorPicker(const ColorPicker &)            = delete;
        ColorPicker &operator=(const ColorPicker &) = delete;

        // Registry-owned immutable editor builtin, separate from glyph page ids
        // (0x40000000+) and automatically allocated surface textures (1+).
        static constexpr sky::ui::UITextureId WHEEL_TEXTURE_ID = 0x80000000u;

        void Open(const sky::ui::UIRect &anchor,
                  const sky::ui::UIRect &bounds,
                  const ColorRGBA       &current,
                  LiveFn                 onLive,
                  CommitFn               onCommit,
                  sky::ui::UITextSystem *text);
        // Close cancels a live preview. Abandon only drops callbacks when its
        // model is no longer valid; neither operation commits a command.
        void Close();
        void Abandon();
        bool IsOpen() const
        {
            return open;
        }

        void                   Draw(sky::ui::UIPaintContext &context, const UiSkin &skin);
        sky::ui::UIEventResult HandlePointer(const sky::ui::UIPointerEvent &event);
        bool                   HandleEscape();

    private:
        enum class Drag { None, Disc, Value, Alpha };

        void      EnsureWheelTexture();
        void      ApplyPixel(float x, float y);
        ColorRGBA WorkingColor() const;
        float     ComponentFromX(float x, float left, float right) const;

        sky::ui::UITextSystem *textSystem = nullptr;
        LiveFn                 live;
        CommitFn               commit;

        bool      open = false;
        ColorRGBA oldColor;

        sky::ui::UIRect popupRect;
        float           discCx = 0.0f;
        float           discCy = 0.0f;
        float           discR  = 0.0f;
        sky::ui::UIRect valueRect;
        sky::ui::UIRect alphaRect;
        sky::ui::UIRect hsvRect;
        sky::ui::UIRect rgbaRect;

        float workH = 0.0f;
        float workS = 1.0f;
        float workV = 1.0f;
        float workA = 1.0f;

        Drag                 drag         = Drag::None;
        sky::ui::UITextureId wheelTexture = sky::ui::UI_INVALID_TEXTURE;
    };

} // namespace sky::editor
