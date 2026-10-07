//
// Native UIKit platform implementation (iOS).
//

#include "IOSPlatform.h"
#include "IOSWindow.h"

#include <core/logger/Logger.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <mach/mach_time.h>

static const char *TAG = "IOSPlatform";

namespace sky {

    // Ensures a UIApplication exists so window/view creation cannot fail on it.
    // UIApplicationMain normally owns this; calling it here is idempotent.
    bool EnsureUIApplication()
    {
        (void)[UIApplication sharedApplication];
        return true;
    }

} // namespace sky

// CADisplayLink target: forwards each tick to the C++ platform.
@interface SkyDisplayLinkTarget : NSObject
@property(nonatomic, assign) sky::IOSPlatform *owner;
@end

@implementation SkyDisplayLinkTarget
- (void)tick:(CADisplayLink *)link
{
    (void)link;
    if (_owner != nullptr) {
        _owner->OnDisplayTick();
    }
}
@end

namespace sky {

    bool Platform::Init(const PlatformInfo &info)
    {
        platform = std::make_unique<IOSPlatform>();
        return platform->Init(info);
    }

    bool IOSPlatform::Init(const PlatformInfo &info)
    {
        EnsureUIApplication();
        nativeApp = info.application != nullptr ? info.application : (__bridge void *)[UIApplication sharedApplication];

        // Pause/resume the frame loop across app lifecycle transitions so the
        // drawable is not held while backgrounded.
        NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
        id bgObserver = [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                                            object:nil
                                             queue:nil
                                        usingBlock:^(NSNotification *) {
                                            this->StopFrameLoop();
                                        }];
        backgroundObs = (__bridge_retained void *)bgObserver;

        id fgObserver = [center addObserverForName:UIApplicationWillEnterForegroundNotification
                                            object:nil
                                             queue:nil
                                        usingBlock:^(NSNotification *) {
                                            this->StartFrameLoop();
                                        }];
        foregroundObs = (__bridge_retained void *)fgObserver;
        return true;
    }

    uint64_t IOSPlatform::GetPerformanceFrequency() const
    {
        mach_timebase_info_data_t info = {};
        mach_timebase_info(&info);
        return static_cast<uint64_t>(1e9) * info.denom / info.numer;
    }

    uint64_t IOSPlatform::GetPerformanceCounter() const
    {
        return mach_absolute_time();
    }

    std::string IOSPlatform::GetInternalPath() const
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        if (paths.count == 0) {
            return {};
        }
        return [[paths objectAtIndex:0] UTF8String];
    }

    std::string IOSPlatform::GetBundlePath() const
    {
        return [[[NSBundle mainBundle] bundlePath] UTF8String];
    }

    std::string IOSPlatform::GetUserConfigPath() const
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        if (paths.count == 0) {
            return {};
        }
        const std::filesystem::path configDir =
            std::filesystem::path([[paths objectAtIndex:0] UTF8String]) / "SkyEngine" / "SkyGame";

        std::error_code ec;
        std::filesystem::create_directories(configDir, ec);

        std::string result = configDir.string();
        if (!result.empty() && result.back() != '/') {
            result += '/';
        }
        return result;
    }

    std::string IOSPlatform::GetEnvVariable(const std::string &env) const
    {
        const char *value = std::getenv(env.c_str());
        return value != nullptr ? std::string(value) : std::string();
    }

    bool IOSPlatform::RunCmd(const std::string &cmd, std::string &out) const
    {
        FILE *pipe = popen(cmd.c_str(), "r");
        if (pipe == nullptr) {
            return false;
        }
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            out += buffer;
        }
        pclose(pipe);
        return true;
    }

    char *IOSPlatform::GetClipBoardText()
    {
        NSString *text = [UIPasteboard generalPasteboard].string;
        if (text == nil) {
            return nullptr;
        }
        const char *utf8 = text.UTF8String;
        auto *result = static_cast<char *>(std::malloc(std::strlen(utf8) + 1));
        if (result != nullptr) {
            std::strcpy(result, utf8);
        }
        return result;
    }

    void IOSPlatform::FreeClipBoardText(char *text)
    {
        std::free(text);
    }

    void IOSPlatform::SetClipBoardText(const std::string &text)
    {
        [UIPasteboard generalPasteboard].string = [NSString stringWithUTF8String:text.c_str()];
    }

    void IOSPlatform::PollEvent(bool &exit)
    {
        // UIKit owns the run loop; events are delivered via view/controller
        // callbacks and frames via CADisplayLink. Nothing to pump here.
        (void)exit;
    }

    void IOSPlatform::SetFrameCallback(std::function<void()> callback)
    {
        frameCallback = std::move(callback);
    }

    void IOSPlatform::StartFrameLoop()
    {
        if (displayLink != nullptr || frameCallback == nullptr) {
            return;
        }
        auto *target = [[SkyDisplayLinkTarget alloc] init];
        target.owner = this;
        CADisplayLink *link = [CADisplayLink displayLinkWithTarget:target selector:@selector(tick:)];
        [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
        linkTarget  = (__bridge_retained void *)target;
        displayLink = (__bridge_retained void *)link;
    }

    void IOSPlatform::StopFrameLoop()
    {
        if (displayLink != nullptr) {
            auto *link = (__bridge_transfer CADisplayLink *)displayLink;
            [link invalidate];
            displayLink = nullptr;
        }
        if (linkTarget != nullptr) {
            (void)(__bridge_transfer SkyDisplayLinkTarget *)linkTarget;
            linkTarget = nullptr;
        }
    }

    void IOSPlatform::OnDisplayTick()
    {
        if (frameCallback) {
            frameCallback();
        }
    }

    IOSPlatform::~IOSPlatform()
    {
        StopFrameLoop();
        if (backgroundObs != nullptr) {
            [[NSNotificationCenter defaultCenter] removeObserver:(__bridge_transfer id)backgroundObs];
            backgroundObs = nullptr;
        }
        if (foregroundObs != nullptr) {
            [[NSNotificationCenter defaultCenter] removeObserver:(__bridge_transfer id)foregroundObs];
            foregroundObs = nullptr;
        }
    }

} // namespace sky
