//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include <string>

namespace sky {

    class NativeWindow {
    public:
        NativeWindow();
        ~NativeWindow();

        struct Descriptor {
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
        };

        void* GetNativeHandle() const;

    private:
        bool Init(const Descriptor&);

        Impl* impl;
    };

}