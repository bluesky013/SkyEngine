//
// Created by Zach Lee on 2021/11/10.
//

#include <core/platform/Platform.h>
#include <framework/window/NativeWindow.h>
#include <framework/window/IWindowEvent.h>
#include <windows.h>
#include <vector>

namespace sky {

    static std::wstring StrToWStr(const std::string& src)
    {
        int len = MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> wc(len + 1, 0);
        MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, wc.data(), len);
        return wc.data();
    }

    static LRESULT CALLBACK AppWndProc(HWND hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam);

    class Win32WindowImpl : public NativeWindowImpl {
    public:
        Win32WindowImpl() : hWnd(nullptr), hInstance(nullptr), handler(nullptr), app(nullptr) {}
        virtual ~Win32WindowImpl() = default;

        bool Init(const NativeWindow::Descriptor&);

        IWindowEvent* GetHandler() const;

    private:
        bool RegisterWin32Class();
        bool CreateWin32Window(const NativeWindow::Descriptor&);

        void* GetNativeHandle() const override;

        void SetEventHandler(IWindowEvent& handler);

        void SetApplication(ApplicationImpl&);
        HWND hWnd;
        HINSTANCE hInstance;
        std::string className;
        std::string titleName;
        IWindowEvent* handler;
        ApplicationImpl* app;
    };

    IWindowEvent* Win32WindowImpl::GetHandler() const
    {
        return handler;
    }

    void* Win32WindowImpl::GetNativeHandle() const
    {
        return hWnd;
    }

    void Win32WindowImpl::SetEventHandler(IWindowEvent& h)
    {
        handler = &h;
    }

    void Win32WindowImpl::SetApplication(ApplicationImpl& application)
    {
        app = &application;
    }

    bool Win32WindowImpl::RegisterWin32Class()
    {
        hInstance = GetModuleHandle(nullptr);

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        windowClass.lpfnWndProc = &AppWndProc;
        windowClass.cbClsExtra = 0;
        windowClass.cbWndExtra = 0;
        windowClass.hInstance = hInstance;
        windowClass.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        windowClass.lpszMenuName = nullptr;
        windowClass.lpszClassName = className.c_str();
        windowClass.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);

        if (!RegisterClassExA(&windowClass)) {
            return false;
        }

        return true;
    }

    bool Win32WindowImpl::CreateWin32Window(const NativeWindow::Descriptor& des)
    {
        hWnd = CreateWindowA(className.c_str(), titleName.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

        if (!hWnd) {
            return FALSE;
        }
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);

        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        return true;
    }

    bool Win32WindowImpl::Init(const NativeWindow::Descriptor& des)
    {
        className = des.className;
        titleName = des.titleName;

        if (!RegisterWin32Class()) {
            return false;
        }

        if (!CreateWin32Window(des)) {
            return false;
        }

        return true;
    }

    LRESULT AppWndProc(HWND hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
    {
        Win32WindowImpl* nativeWindowImpl = reinterpret_cast<Win32WindowImpl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        IWindowEvent* handler = nativeWindowImpl != nullptr ? nativeWindowImpl->GetHandler() : nullptr;

        switch (msg) {
            case WM_SIZE:
                if (handler != nullptr) {
                    uint32_t width = LOWORD(lParam);
                    uint32_t height = HIWORD(lParam);
                    handler->OnWindowResize(hwnd, width, height);
                }
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            default:
                break;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

extern "C"
SKY_EXPORT sky::NativeWindowImpl* CreateNativeWindow(const sky::NativeWindow::Descriptor& des)
{
    auto impl = new sky::Win32WindowImpl();
    if (!impl->Init(des)) {
        delete impl;
        impl = nullptr;
    }
    return impl;
}