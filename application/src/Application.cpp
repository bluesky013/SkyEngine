//
// Created by Zach Lee on 2021/11/11.
//

#include "application/Application.h"

namespace sky {

    Application::Application() : impl(nullptr), window(nullptr)
    {
    }

    Application::~Application()
    {

    }

    bool Application::Init()
    {
        impl = Impl::Create();
        if (impl == nullptr) {
            return false;
        }

        NativeWindow::Descriptor des = {};
        des.titleName = "SkyEngine";
        des.className = "SkyEngine";

        window = NativeWindow::Create(des);
        if (window == nullptr) {
            return false;
        }

        return true;
    }

    void Application::Mainloop()
    {
        while (!impl->IsExit()) {
            impl->PumpMessages();
        }
    }


}
