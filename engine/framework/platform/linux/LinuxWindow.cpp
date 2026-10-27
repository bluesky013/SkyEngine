//
// Native X11 window implementation (no SDL).
//

#include "LinuxWindow.h"
#include "LinuxX11.h"

#include <core/logger/Logger.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindowManager.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

static const char *TAG = "LinuxWindow";

namespace sky {

    namespace {

        ScanCode FromKeysym(KeySym sym)
        {
            if (sym >= XK_a && sym <= XK_z) {
                return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_A) + (sym - XK_a));
            }
            if (sym >= XK_A && sym <= XK_Z) {
                return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_A) + (sym - XK_A));
            }
            if (sym >= XK_F1 && sym <= XK_F12) {
                return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_F1) + (sym - XK_F1));
            }
            if (sym >= XK_0 && sym <= XK_9) {
                if (sym == XK_0) {
                    return ScanCode::KEY_0;
                }
                return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_1) + (sym - XK_1));
            }

            switch (sym) {
            case XK_Return:
            case XK_KP_Enter: return ScanCode::KEY_RETURN;
            case XK_Escape: return ScanCode::KEY_ESCAPE;
            case XK_BackSpace: return ScanCode::KEY_BACKSPACE;
            case XK_Tab: return ScanCode::KEY_TAB;
            case XK_space: return ScanCode::KEY_SPACE;
            case XK_minus: return ScanCode::KEY_MINUS;
            case XK_equal: return ScanCode::KEY_EQUALS;
            case XK_bracketleft: return ScanCode::KEY_LEFTBRACKET;
            case XK_bracketright: return ScanCode::KEY_RIGHTBRACKET;
            case XK_backslash: return ScanCode::KEY_BACKSLASH;
            case XK_semicolon: return ScanCode::KEY_SEMICOLON;
            case XK_apostrophe: return ScanCode::KEY_APOSTROPHE;
            case XK_grave: return ScanCode::KEY_GRAVE;
            case XK_comma: return ScanCode::KEY_COMMA;
            case XK_period: return ScanCode::KEY_PERIOD;
            case XK_slash: return ScanCode::KEY_SLASH;
            case XK_Caps_Lock: return ScanCode::KEY_CAPSLOCK;
            case XK_Print: return ScanCode::KEY_PRINTSCREEN;
            case XK_Scroll_Lock: return ScanCode::KEY_SCROLLLOCK;
            case XK_Pause: return ScanCode::KEY_PAUSE;
            case XK_Insert: return ScanCode::KEY_INSERT;
            case XK_Home: return ScanCode::KEY_HOME;
            case XK_Prior: return ScanCode::KEY_PAGEUP;
            case XK_Delete: return ScanCode::KEY_DELETE;
            case XK_End: return ScanCode::KEY_END;
            case XK_Next: return ScanCode::KEY_PAGEDOWN;
            case XK_Right: return ScanCode::KEY_RIGHT;
            case XK_Left: return ScanCode::KEY_LEFT;
            case XK_Down: return ScanCode::KEY_DOWN;
            case XK_Up: return ScanCode::KEY_UP;
            case XK_Num_Lock: return ScanCode::KEY_NUMLOCKCLEAR;
            case XK_KP_Divide: return ScanCode::KEY_KP_DIVIDE;
            case XK_KP_Multiply: return ScanCode::KEY_KP_MULTIPLY;
            case XK_KP_Subtract: return ScanCode::KEY_KP_MINUS;
            case XK_KP_Add: return ScanCode::KEY_KP_PLUS;
            case XK_KP_1: return ScanCode::KEY_KP_1;
            case XK_KP_2: return ScanCode::KEY_KP_2;
            case XK_KP_3: return ScanCode::KEY_KP_3;
            case XK_KP_4: return ScanCode::KEY_KP_4;
            case XK_KP_5: return ScanCode::KEY_KP_5;
            case XK_KP_6: return ScanCode::KEY_KP_6;
            case XK_KP_7: return ScanCode::KEY_KP_7;
            case XK_KP_8: return ScanCode::KEY_KP_8;
            case XK_KP_9: return ScanCode::KEY_KP_9;
            case XK_KP_0: return ScanCode::KEY_KP_0;
            case XK_KP_Decimal: return ScanCode::KEY_KP_PERIOD;
            default: break;
            }
            return ScanCode::KEY_NUM;
        }

        KeyModFlags ModsFromState(unsigned int state)
        {
            KeyModFlags mods;
            if ((state & ShiftMask) != 0) {
                mods.SetBit(KeyMod::LEFT_SHIFT);
            }
            if ((state & ControlMask) != 0) {
                mods.SetBit(KeyMod::LEFT_CTRL);
            }
            if ((state & Mod1Mask) != 0) {
                mods.SetBit(KeyMod::LEFT_ALT);
            }
            if ((state & Mod4Mask) != 0) {
                mods.SetBit(KeyMod::LEFT_GUI);
            }
            return mods;
        }

        unsigned int ToXCursorShape(StandardCursor cursor)
        {
            switch (cursor) {
            case StandardCursor::ResizeHorizontal: return XC_sb_h_double_arrow;
            case StandardCursor::ResizeVertical: return XC_sb_v_double_arrow;
            case StandardCursor::ResizeAll: return XC_fleur;
            case StandardCursor::Text: return XC_xterm;
            case StandardCursor::Hand: return XC_hand2;
            case StandardCursor::Arrow: break;
            }
            return XC_left_ptr;
        }

    } // namespace

    LinuxWindow::~LinuxWindow()
    {
        auto *display = static_cast<Display *>(this->display);
        if (display != nullptr && xwindow != 0) {
            XDestroyWindow(display, xwindow);
            XFlush(display);
        }
        if (cursorHandle != 0 && display != nullptr) {
            XFreeCursor(display, cursorHandle);
        }
    }

    bool LinuxWindow::Init(const Descriptor &desc)
    {
        descriptor = desc;

        Display *xDisplay = x11::GetDisplay();
        if (xDisplay == nullptr) {
            LOG_E(TAG, "cannot create window: X11 display is not available");
            return false;
        }
        display = xDisplay;

        xwindow = XCreateSimpleWindow(xDisplay, DefaultRootWindow(xDisplay), 0, 0,
            static_cast<unsigned int>(desc.width), static_cast<unsigned int>(desc.height),
            0, BlackPixel(xDisplay, 0), WhitePixel(xDisplay, 0));
        if (xwindow == 0) {
            LOG_E(TAG, "XCreateSimpleWindow failed");
            return false;
        }

        XStoreName(xDisplay, xwindow, desc.titleName.c_str());

        const Atom deleteAtom = XInternAtom(xDisplay, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(xDisplay, xwindow, const_cast<Atom *>(&deleteAtom), 1);

        XSelectInput(xDisplay, xwindow,
            StructureNotifyMask | ExposureMask | FocusChangeMask | KeyPressMask | KeyReleaseMask |
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

        XMapWindow(xDisplay, xwindow);
        XFlush(xDisplay);

        SetID(static_cast<WindowID>(xwindow));

        // The first native window in the process is the main window.
        mainWindow = NativeWindowManager::Get()->GetMainWindow() == nullptr;

        UpdateXcbHandle();
        ApplyCursor();

        NativeWindowManager::Get()->Register(this);
        return true;
    }

    void LinuxWindow::UpdateXcbHandle()
    {
        auto *xDisplay = static_cast<Display *>(display);
        xcbHandle.connection = xDisplay != nullptr ? x11::GetXcbConnection(xDisplay) : nullptr;
        xcbHandle.window     = reinterpret_cast<void *>(xwindow);
        winHandle            = &xcbHandle;
    }

    void LinuxWindow::DispatchNativeEvent(void *event)
    {
        auto &ev            = *static_cast<XEvent *>(event);
        const WindowID id   = GetWinId();

        switch (ev.type) {
        case ConfigureNotify: {
            const auto width  = static_cast<uint32_t>(ev.xconfigure.width);
            const auto height = static_cast<uint32_t>(ev.xconfigure.height);
            if (width > 0 && height > 0 && (width != descriptor.width || height != descriptor.height)) {
                NotifyResized(width, height);
                WindowResizeEvent resize = {};
                resize.winID             = id;
                resize.width             = width;
                resize.height            = height;
                Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowResize, resize);
            }

            WindowMoveEvent move = {};
            move.winID           = id;
            move.x               = ev.xconfigure.x;
            move.y               = ev.xconfigure.y;
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowMove, move);
            break;
        }
        case FocusIn:
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnFocusChanged, true);
            break;
        case FocusOut:
            Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnFocusChanged, false);
            break;
        case KeyPress:
        case KeyRelease: {
            auto *xDisplay = static_cast<Display *>(display);
            const KeySym sym = XLookupKeysym(&ev.xkey, 0);

            KeyboardEvent key = {};
            key.winID         = id;
            key.scanCode      = FromKeysym(sym);
            key.mod           = ModsFromState(ev.xkey.state);
            if (ev.type == KeyPress) {
                Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyDown, key);
            } else {
                Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, key);
            }

            if (ev.type == KeyPress) {
                char buffer[32] = {};
                KeySym ignored  = 0;
                const int count = XLookupString(&ev.xkey, buffer, sizeof(buffer) - 1, &ignored, nullptr);
                if (count > 0 && static_cast<unsigned char>(buffer[0]) >= 0x20 &&
                    static_cast<unsigned char>(buffer[0]) != 0x7f) {
                    buffer[count] = '\0';
                    Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnTextInput, id, buffer);
                }
            }
            (void)xDisplay;
            break;
        }
        case ButtonPress:
        case ButtonRelease: {
            const unsigned int button = ev.xbutton.button;
            MouseButtonEvent mouse    = {};
            mouse.winID               = id;
            mouse.x                   = ev.xbutton.x;
            mouse.y                   = ev.xbutton.y;
            mouse.clicks              = 1;
            mouse.modifiers           = static_cast<uint32_t>(ModsFromState(ev.xbutton.state).value);

            if (ev.type == ButtonPress && (button == Button4 || button == Button5)) {
                MouseWheelEvent wheel = {};
                wheel.winID           = id;
                wheel.x               = ev.xbutton.x;
                wheel.y               = ev.xbutton.y;
                wheel.delta           = button == Button4 ? 120.0f : -120.0f;
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseWheel, wheel);
                break;
            }
            if (button == Button1) {
                mouse.button = MouseButtonType::LEFT;
            } else if (button == Button2) {
                mouse.button = MouseButtonType::MIDDLE;
            } else if (button == Button3) {
                mouse.button = MouseButtonType::RIGHT;
            } else {
                break;
            }

            if (ev.type == ButtonPress) {
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonDown, mouse);
            } else {
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonUp, mouse);
            }
            break;
        }
        case MotionNotify: {
            MouseMotionEvent motion = {};
            motion.winID            = id;
            motion.x                = ev.xmotion.x;
            motion.y                = ev.xmotion.y;
            motion.relX             = 0;
            motion.relY             = 0;
            Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseMotion, motion);
            break;
        }
        case ClientMessage: {
            auto *xDisplay      = static_cast<Display *>(display);
            const Atom deleteAt = XInternAtom(xDisplay, "WM_DELETE_WINDOW", True);
            if (static_cast<Atom>(ev.xclient.data.l[0]) == deleteAt) {
                Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowClose, static_cast<const NativeWindow *>(this));
                if (mainWindow) {
                    x11::RequestExit();
                }
            }
            break;
        }
        case DestroyNotify:
            NativeWindowManager::Get()->UnRegister(this);
            xwindow = 0;
            break;
        default: break;
        }
    }

    bool LinuxWindow::GetPosition(int32_t &x, int32_t &y) const
    {
        auto *xDisplay = static_cast<Display *>(display);
        if (xDisplay == nullptr || xwindow == 0) {
            return false;
        }
        XWindowAttributes attributes;
        if (XGetWindowAttributes(xDisplay, xwindow, &attributes) == 0) {
            return false;
        }
        x = attributes.x;
        y = attributes.y;
        return true;
    }

    void LinuxWindow::SetPosition(int32_t x, int32_t y)
    {
        auto *xDisplay = static_cast<Display *>(display);
        if (xDisplay != nullptr && xwindow != 0) {
            XMoveWindow(xDisplay, xwindow, x, y);
        }
    }

    void LinuxWindow::SetPointerCapture(bool capture)
    {
        auto *xDisplay = static_cast<Display *>(display);
        if (xDisplay == nullptr || xwindow == 0) {
            return;
        }
        if (capture) {
            XGrabPointer(xDisplay, xwindow, False,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, xwindow, None, CurrentTime);
        } else {
            XUngrabPointer(xDisplay, CurrentTime);
        }
    }

    bool LinuxWindow::GetGlobalCursorPosition(int32_t &x, int32_t &y) const
    {
        auto *xDisplay = static_cast<Display *>(display);
        if (xDisplay == nullptr || xwindow == 0) {
            return false;
        }
        ::Window root   = 0;
        ::Window child  = 0;
        int rootX       = 0;
        int rootY       = 0;
        int winX        = 0;
        int winY        = 0;
        unsigned int mask = 0;
        const Bool ok = XQueryPointer(xDisplay, xwindow, &root, &child, &rootX, &rootY, &winX, &winY, &mask);
        if (ok == False) {
            return false;
        }
        x = rootX;
        y = rootY;
        return true;
    }

    void LinuxWindow::ApplyCursor()
    {
        auto *xDisplay = static_cast<Display *>(display);
        if (xDisplay == nullptr || xwindow == 0) {
            return;
        }
        if (cursorHandle != 0) {
            XFreeCursor(xDisplay, cursorHandle);
            cursorHandle = 0;
        }
        cursorHandle = XCreateFontCursor(xDisplay, ToXCursorShape(desiredCursor));
        XDefineCursor(xDisplay, xwindow, cursorHandle);
    }

    void LinuxWindow::SetCursor(StandardCursor cursor)
    {
        desiredCursor = cursor;
        ApplyCursor();
    }

    void LinuxWindow::SetTitle(const std::string &title)
    {
        auto *xDisplay = static_cast<Display *>(display);
        if (xDisplay != nullptr && xwindow != 0) {
            XStoreName(xDisplay, xwindow, title.c_str());
            descriptor.titleName = title;
        }
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new LinuxWindow();
        if (!window->Init(des)) {
            delete window;
            return nullptr;
        }
        return window;
    }

} // namespace sky
