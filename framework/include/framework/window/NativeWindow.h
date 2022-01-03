//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include <string>

namespace sky {
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

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            static Impl* Create(const Descriptor&);

            virtual void* GetNativeHandle() const = 0;

            virtual void SetEventHandler(IWindowEvent&) = 0;
        };

        void* GetNativeHandle() const;

        void SetEventHandler(IWindowEvent&);

    private:
        bool Init(const Descriptor&);

        Impl* impl;
    };

}