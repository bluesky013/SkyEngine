//
// Created by Zach Lee on 2021/11/11.
//

#include "application/Application.h"
#include <windows.h>

namespace sky {

    class Win32AppImpl : public Application::Impl {
    public:
        Win32AppImpl() = default;
        virtual ~Win32AppImpl() = default;

        void PumpMessages() override;

        bool IsExit() const override;

        void SetExit() override;

    private:
        bool exit = false;
    };

    Application::Impl* Application::Impl::Create()
    {
        return new Win32AppImpl();
    }

    bool Win32AppImpl::IsExit() const
    {
        return exit;
    }

    void Win32AppImpl::SetExit()
    {
        exit = true;
    }

    void Win32AppImpl::PumpMessages()
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                exit = true;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

}