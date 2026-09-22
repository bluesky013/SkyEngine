//
// Created by Zach Lee on 2022/9/25.
//
// Native Win32 window implementation (no SDL). windows.h is included here, after
// the engine headers, so its macros do not leak into them.
//

#include "Win32Window.h"
#include "Win32Platform.h"

#include <core/event/Event.h>
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindowManager.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>

static const char *TAG = "Win32Window";

namespace sky {

    namespace {

        const wchar_t *kWindowClass = L"SkyEngineWin32Window";

        ScanCode FromVirtualKey(WPARAM vk)
        {
            switch (vk) {
                case 'A': return ScanCode::KEY_A; case 'B': return ScanCode::KEY_B;
                case 'C': return ScanCode::KEY_C; case 'D': return ScanCode::KEY_D;
                case 'E': return ScanCode::KEY_E; case 'F': return ScanCode::KEY_F;
                case 'G': return ScanCode::KEY_G; case 'H': return ScanCode::KEY_H;
                case 'I': return ScanCode::KEY_I; case 'J': return ScanCode::KEY_J;
                case 'K': return ScanCode::KEY_K; case 'L': return ScanCode::KEY_L;
                case 'M': return ScanCode::KEY_M; case 'N': return ScanCode::KEY_N;
                case 'O': return ScanCode::KEY_O; case 'P': return ScanCode::KEY_P;
                case 'Q': return ScanCode::KEY_Q; case 'R': return ScanCode::KEY_R;
                case 'S': return ScanCode::KEY_S; case 'T': return ScanCode::KEY_T;
                case 'U': return ScanCode::KEY_U; case 'V': return ScanCode::KEY_V;
                case 'W': return ScanCode::KEY_W; case 'X': return ScanCode::KEY_X;
                case 'Y': return ScanCode::KEY_Y; case 'Z': return ScanCode::KEY_Z;
                case '1': return ScanCode::KEY_1; case '2': return ScanCode::KEY_2;
                case '3': return ScanCode::KEY_3; case '4': return ScanCode::KEY_4;
                case '5': return ScanCode::KEY_5; case '6': return ScanCode::KEY_6;
                case '7': return ScanCode::KEY_7; case '8': return ScanCode::KEY_8;
                case '9': return ScanCode::KEY_9; case '0': return ScanCode::KEY_0;
                case VK_RETURN: return ScanCode::KEY_RETURN;
                case VK_ESCAPE: return ScanCode::KEY_ESCAPE;
                case VK_BACK: return ScanCode::KEY_BACKSPACE;
                case VK_TAB: return ScanCode::KEY_TAB;
                case VK_SPACE: return ScanCode::KEY_SPACE;
                case VK_OEM_MINUS: return ScanCode::KEY_MINUS;
                case VK_OEM_PLUS: return ScanCode::KEY_EQUALS;
                case VK_OEM_4: return ScanCode::KEY_LEFTBRACKET;
                case VK_OEM_6: return ScanCode::KEY_RIGHTBRACKET;
                case VK_OEM_5: return ScanCode::KEY_BACKSLASH;
                case VK_OEM_1: return ScanCode::KEY_SEMICOLON;
                case VK_OEM_7: return ScanCode::KEY_APOSTROPHE;
                case VK_OEM_3: return ScanCode::KEY_GRAVE;
                case VK_OEM_COMMA: return ScanCode::KEY_COMMA;
                case VK_OEM_PERIOD: return ScanCode::KEY_PERIOD;
                case VK_OEM_2: return ScanCode::KEY_SLASH;
                case VK_CAPITAL: return ScanCode::KEY_CAPSLOCK;
                case VK_F1: return ScanCode::KEY_F1; case VK_F2: return ScanCode::KEY_F2;
                case VK_F3: return ScanCode::KEY_F3; case VK_F4: return ScanCode::KEY_F4;
                case VK_F5: return ScanCode::KEY_F5; case VK_F6: return ScanCode::KEY_F6;
                case VK_F7: return ScanCode::KEY_F7; case VK_F8: return ScanCode::KEY_F8;
                case VK_F9: return ScanCode::KEY_F9; case VK_F10: return ScanCode::KEY_F10;
                case VK_F11: return ScanCode::KEY_F11; case VK_F12: return ScanCode::KEY_F12;
                case VK_INSERT: return ScanCode::KEY_INSERT;
                case VK_HOME: return ScanCode::KEY_HOME;
                case VK_PRIOR: return ScanCode::KEY_PAGEUP;
                case VK_DELETE: return ScanCode::KEY_DELETE;
                case VK_END: return ScanCode::KEY_END;
                case VK_NEXT: return ScanCode::KEY_PAGEDOWN;
                case VK_RIGHT: return ScanCode::KEY_RIGHT;
                case VK_LEFT: return ScanCode::KEY_LEFT;
                case VK_DOWN: return ScanCode::KEY_DOWN;
                case VK_UP: return ScanCode::KEY_UP;
                case VK_NUMLOCK: return ScanCode::KEY_NUMLOCKCLEAR;
                case VK_DIVIDE: return ScanCode::KEY_KP_DIVIDE;
                case VK_MULTIPLY: return ScanCode::KEY_KP_MULTIPLY;
                case VK_SUBTRACT: return ScanCode::KEY_KP_MINUS;
                case VK_ADD: return ScanCode::KEY_KP_PLUS;
                case VK_DECIMAL: return ScanCode::KEY_KP_PERIOD;
                case VK_NUMPAD0: return ScanCode::KEY_KP_0; case VK_NUMPAD1: return ScanCode::KEY_KP_1;
                case VK_NUMPAD2: return ScanCode::KEY_KP_2; case VK_NUMPAD3: return ScanCode::KEY_KP_3;
                case VK_NUMPAD4: return ScanCode::KEY_KP_4; case VK_NUMPAD5: return ScanCode::KEY_KP_5;
                case VK_NUMPAD6: return ScanCode::KEY_KP_6; case VK_NUMPAD7: return ScanCode::KEY_KP_7;
                case VK_NUMPAD8: return ScanCode::KEY_KP_8; case VK_NUMPAD9: return ScanCode::KEY_KP_9;
                default: return ScanCode::KEY_NUM;
            }
        }

        KeyModFlags CurrentModifiers()
        {
            KeyModFlags mod;
            if (GetKeyState(VK_LSHIFT) & 0x8000) { mod |= KeyMod::LEFT_SHIFT; }
            if (GetKeyState(VK_RSHIFT) & 0x8000) { mod |= KeyMod::RIGHT_SHIFT; }
            if (GetKeyState(VK_LCONTROL) & 0x8000) { mod |= KeyMod::LEFT_CTRL; }
            if (GetKeyState(VK_RCONTROL) & 0x8000) { mod |= KeyMod::RIGHT_CTRL; }
            if (GetKeyState(VK_LMENU) & 0x8000) { mod |= KeyMod::LEFT_ALT; }
            if (GetKeyState(VK_RMENU) & 0x8000) { mod |= KeyMod::RIGHT_ALT; }
            if (GetKeyState(VK_LWIN) & 0x8000) { mod |= KeyMod::LEFT_GUI; }
            if (GetKeyState(VK_RWIN) & 0x8000) { mod |= KeyMod::RIGHT_GUI; }
            if (GetKeyState(VK_CAPITAL) & 0x0001) { mod |= KeyMod::CAPS; }
            return mod;
        }

        std::string WideCharToUtf8(wchar_t ch)
        {
            char buffer[8] = {0};
            const int length = ::WideCharToMultiByte(CP_UTF8, 0, &ch, 1, buffer, sizeof(buffer), nullptr, nullptr);
            return (length > 0) ? std::string(buffer, static_cast<size_t>(length)) : std::string();
        }

        void HandleMessage(Win32Window *window, HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            const WindowID winID = window->GetWinId();
            switch (msg) {
                case WM_SIZE: {
                    const uint32_t width  = LOWORD(lparam);
                    const uint32_t height = HIWORD(lparam);
                    if (width > 0 && height > 0) {
                        WindowResizeEvent event = {};
                        event.winID  = winID;
                        event.width  = width;
                        event.height = height;
                        Event<IWindowEvent>::BroadCast(window, &IWindowEvent::OnWindowResize, event);
                    }
                    break;
                }
                case WM_SETFOCUS:
                    Event<IWindowEvent>::BroadCast(window, &IWindowEvent::OnFocusChanged, true);
                    break;
                case WM_KILLFOCUS:
                    Event<IWindowEvent>::BroadCast(window, &IWindowEvent::OnFocusChanged, false);
                    break;
                case WM_KEYDOWN: {
                    KeyboardEvent event = {};
                    event.winID    = winID;
                    event.scanCode = FromVirtualKey(wparam);
                    event.mod      = CurrentModifiers();
                    Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyDown, event);
                    break;
                }
                case WM_KEYUP: {
                    KeyboardEvent event = {};
                    event.winID    = winID;
                    event.scanCode = FromVirtualKey(wparam);
                    event.mod      = CurrentModifiers();
                    Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, event);
                    break;
                }
                case WM_CHAR: {
                    const std::string text = WideCharToUtf8(static_cast<wchar_t>(wparam));
                    if (!text.empty()) {
                        Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnTextInput, winID, text.c_str());
                    }
                    break;
                }
                case WM_MOUSEMOVE: {
                    MouseMotionEvent event = {};
                    event.winID = winID;
                    event.x     = GET_X_LPARAM(lparam);
                    event.y     = GET_Y_LPARAM(lparam);
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseMotion, event);
                    break;
                }
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_MBUTTONUP: {
                    MouseButtonEvent event = {};
                    event.winID  = winID;
                    event.x      = GET_X_LPARAM(lparam);
                    event.y      = GET_Y_LPARAM(lparam);
                    event.clicks = 1;
                    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
                        event.button = MouseButtonType::LEFT;
                    } else if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) {
                        event.button = MouseButtonType::RIGHT;
                    } else {
                        event.button = MouseButtonType::MIDDLE;
                    }
                    if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) {
                        Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonDown, event);
                    } else {
                        Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonUp, event);
                    }
                    break;
                }
                case WM_MOUSEWHEEL: {
                    MouseWheelEvent event = {};
                    event.winID = winID;
                    event.y     = GET_WHEEL_DELTA_WPARAM(wparam);
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseWheel, event);
                    break;
                }
                default:
                    break;
            }
        }

        LRESULT CALLBACK SkyWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            auto *window = reinterpret_cast<Win32Window *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (window != nullptr) {
                HandleMessage(window, hwnd, msg, wparam, lparam);
            }
            return ::DefWindowProcW(hwnd, msg, wparam, lparam);
        }

    } // namespace

    bool Win32Window::EnsureWindowClass()
    {
        static bool registered = false;
        if (registered) {
            return true;
        }

        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = SkyWndProc;
        wc.hInstance     = ::GetModuleHandleW(nullptr);
        wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kWindowClass;

        if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            LOG_E(TAG, "RegisterClassExW failed: %lu", ::GetLastError());
            return false;
        }
        registered = true;
        return true;
    }

    bool Win32Window::Init(const Descriptor &desc)
    {
        if (!EnsureWindowClass()) {
            return false;
        }

        descriptor = desc;
        const std::wstring title(desc.titleName.begin(), desc.titleName.end());

        RECT rect = {0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
        ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        HWND handle = ::CreateWindowExW(0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                        CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr,
                                        nullptr, ::GetModuleHandleW(nullptr), nullptr);
        if (handle == nullptr) {
            LOG_E(TAG, "CreateWindowExW failed: %lu", ::GetLastError());
            return false;
        }

        ::SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        hwnd      = handle;
        winHandle = handle;
        SetID(static_cast<WindowID>(reinterpret_cast<uintptr_t>(handle)));
        NativeWindowManager::Get()->Register(this);

        if (auto *win32Platform = dynamic_cast<Win32Platform *>(Platform::Get()->GetImpl())) {
            win32Platform->SetMainWindow(handle);
        }

        ::ShowWindow(handle, SW_SHOW);
        ::UpdateWindow(handle);
        return true;
    }

    Win32Window::~Win32Window()
    {
        if (hwnd != nullptr) {
            NativeWindowManager::Get()->UnRegister(this);
            HWND handle = static_cast<HWND>(hwnd);
            ::SetWindowLongPtrW(handle, GWLP_USERDATA, 0);
            ::DestroyWindow(handle);
            hwnd      = nullptr;
            winHandle = nullptr;
        }
    }

    void *Win32Window::GetNativeHandle() const
    {
        return hwnd;
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new Win32Window();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }

} // namespace sky
