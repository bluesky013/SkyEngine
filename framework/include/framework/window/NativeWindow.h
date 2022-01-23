//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include <string>

namespace sky {
    class Application;
    class ApplicationImpl;
    class NativeWindowImpl;
    class IWindowEvent;

    class NativeWindow {
    public:
        NativeWindow();
        ~NativeWindow();

        struct Descriptor {
            uint32_t width = 1366;
            uint32_t height = 768;
            std::string className;
            std::string titleName;
        };

        static NativeWindow* Create(const Descriptor&);

        void* GetNativeHandle() const;

        void SetEventHandler(IWindowEvent&);

        void SetApplication(Application&);

    private:
        bool Init(const Descriptor&);

        NativeWindowImpl* impl;
    };

    class NativeWindowImpl {
    public:
        NativeWindowImpl() = default;
        virtual ~NativeWindowImpl() = default;

        static NativeWindowImpl* Create(const NativeWindow::Descriptor&);

        virtual void* GetNativeHandle() const = 0;

        virtual void SetEventHandler(IWindowEvent&) = 0;

        virtual void SetApplication(ApplicationImpl& application) = 0;
    };

}