//
// Created by Zach Lee on 2021/11/28.
//

#include "application/Application.h"
#include <AppKit/NSApplication.h>
#import <Cocoa/Cocoa.h>

namespace sky {

    class MacosApplicationImpl : public Application::Impl {
    public:
        MacosApplicationImpl() = default;

        virtual ~MacosApplicationImpl() = default;

        void PumpMessages() override;

        bool IsExit() const override;

        void SetExit() override;

    private:
        bool PumpMessageInternal();

        bool exit = false;
    };

    Application::Impl *Application::Impl::Create()
    {
        return new MacosApplicationImpl();
    }

    void MacosApplicationImpl::PumpMessages()
    {
        while (PumpMessageInternal());
    }

    bool MacosApplicationImpl::PumpMessageInternal()
    {
        @autoreleasepool
        {
            NSEvent* event = [NSApp nextEventMatchingMask: NSEventMaskAny
                untilDate: [NSDate distantPast]
                inMode: NSDefaultRunLoopMode
                dequeue: YES];

            if (event != nil) {
                [NSApp sendEvent: event];
                return true;
            } else {
                return false;
            }
        }
    }

    bool MacosApplicationImpl::IsExit() const
    {
        return exit;
    }

    void MacosApplicationImpl::SetExit()
    {
        exit = true;
    }
}
