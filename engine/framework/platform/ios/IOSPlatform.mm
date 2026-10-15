//
// Native UIKit platform implementation (iOS).
//

#include "IOSPlatform.h"
#include "IOSWindow.h"

#include <framework/platform/IOSSceneHost.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <mach/mach_time.h>

// CADisplayLink target: forwards each tick to the C++ platform.
@interface                                      SkyDisplayLinkTarget : NSObject
@property (nonatomic, assign) sky::IOSPlatform *owner;
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
        nativeApp        = info.application != nullptr ? info.application : (__bridge void *)[UIApplication sharedApplication];
        bundleFileSystem = new NativeFileSystem(FilePath(GetBundlePath()));
        return nativeApp != nullptr;
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

    FileSystemPtr IOSPlatform::GetBundleFileSystem()
    {
        return bundleFileSystem;
    }

    void *IOSPlatform::GetMainWinHandle() const
    {
        return mainNativeWindow != nullptr ? mainNativeWindow->GetNativeHandle() : nullptr;
    }

    std::string IOSPlatform::GetUserConfigPath() const
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        if (paths.count == 0) {
            return {};
        }
        const std::filesystem::path configDir = std::filesystem::path([[paths objectAtIndex:0] UTF8String]) / "SkyEngine" / "SkyGame";

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
        (void)cmd;
        (void)out;
        return false; // iOS applications cannot launch arbitrary subprocesses.
    }

    char *IOSPlatform::GetClipBoardText()
    {
        NSString *text = [UIPasteboard generalPasteboard].string;
        if (text == nil) {
            return nullptr;
        }
        const char *utf8 = text.UTF8String;
        if (utf8 == nullptr) {
            return nullptr;
        }
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
        NSString *nativeText = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
        if (nativeText != nil) {
            [UIPasteboard generalPasteboard].string = nativeText;
        }
    }

    void IOSPlatform::PollEvent(bool &exit)
    {
        // UIKit owns the run loop; events are delivered via view/controller
        // callbacks and frames via CADisplayLink. Nothing to pump here.
        (void)exit;
    }

    bool IOSPlatform::IsFrameActive() const
    {
        return frameState.CanTick();
    }

    bool IOSPlatform::ConnectScene(void *handle)
    {
        if (![NSThread isMainThread] || handle == nullptr) {
            return false;
        }
        const uintptr_t identity = reinterpret_cast<uintptr_t>(handle);
        const uintptr_t current  = frameState.GetSceneIdentity();
        if (current != 0) {
            return current == identity;
        }
        UIWindowScene *scene = (__bridge UIWindowScene *)handle;
        if (![scene isKindOfClass:[UIWindowScene class]] || ![scene.session.role isEqualToString:UIWindowSceneSessionRoleApplication]) {
            return false;
        }
        if (!frameState.ConnectScene(identity)) {
            return false;
        }
        windowScene = (__bridge_retained void *)scene;
        if (mainNativeWindow != nullptr && !mainNativeWindow->AttachScene(handle)) {
            DisconnectScene(handle);
            return false;
        }
        return true;
    }

    void IOSPlatform::SetSceneActive(void *scene, bool active)
    {
        if (![NSThread isMainThread] || !frameState.SetSceneActive(reinterpret_cast<uintptr_t>(scene), active)) {
            return;
        }
        RefreshFrameLoop();
        if (mainNativeWindow != nullptr) {
            mainNativeWindow->SetFocused(active);
        }
    }

    void IOSPlatform::DisconnectScene(void *scene)
    {
        if (![NSThread isMainThread] || !frameState.DisconnectScene(reinterpret_cast<uintptr_t>(scene))) {
            return;
        }
        UIWindowScene *previousScene = (__bridge_transfer UIWindowScene *)windowScene;
        windowScene                  = nullptr;
        RefreshFrameLoop();
        (void)previousScene; // Keep the scene alive through surface teardown.
        if (mainNativeWindow != nullptr) {
            mainNativeWindow->DetachScene();
        }
    }

    bool IOSPlatform::RegisterMainWindow(IOSWindow *window)
    {
        if (windowScene == nullptr || window == nullptr || (mainNativeWindow != nullptr && mainNativeWindow != window)) {
            return false;
        }
        mainNativeWindow = window;
        return true;
    }

    void IOSPlatform::SetWindowSurface(const IOSWindow *window, void *handle)
    {
        if (mainNativeWindow != window) {
            return;
        }
        frameState.SetSurfaceAvailable(handle != nullptr);
        RefreshFrameLoop();
        if (handle != nullptr) {
            IOSWindow *owner = mainNativeWindow;
            owner->SetTextInputRect(textInputRect[0], textInputRect[1], textInputRect[2], textInputRect[3]);
            if (mainNativeWindow == owner) {
                owner->SetTextInputEnabled(textInputEnabled);
            }
        }
    }

    void IOSPlatform::ClearMainWindow(const IOSWindow *window)
    {
        if (mainNativeWindow != window) {
            return;
        }
        mainNativeWindow = nullptr;
        frameState.SetSurfaceAvailable(false);
        RefreshFrameLoop();
    }

    void IOSPlatform::StartTextInput()
    {
        textInputEnabled = true;
        if (mainNativeWindow != nullptr) {
            mainNativeWindow->SetTextInputEnabled(true);
        }
    }

    void IOSPlatform::StopTextInput()
    {
        textInputEnabled = false;
        if (mainNativeWindow != nullptr) {
            mainNativeWindow->SetTextInputEnabled(false);
        }
    }

    void IOSPlatform::SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        textInputRect[0] = x;
        textInputRect[1] = y;
        textInputRect[2] = width;
        textInputRect[3] = height;
        if (mainNativeWindow != nullptr) {
            mainNativeWindow->SetTextInputRect(x, y, width, height);
        }
    }

    void IOSPlatform::SetFrameCallback(std::function<void()> callback)
    {
        frameCallback = std::move(callback);
        RefreshFrameLoop();
    }

    void IOSPlatform::StartFrameLoop()
    {
        frameState.Start();
        RefreshFrameLoop();
    }

    void IOSPlatform::RefreshFrameLoop()
    {
        if (!frameState.ShouldRunLoop() || frameCallback == nullptr) {
            InvalidateDisplayLink();
            return;
        }
        if (displayLink != nullptr) {
            return;
        }
        auto *target        = [[SkyDisplayLinkTarget alloc] init];
        target.owner        = this;
        CADisplayLink *link = [CADisplayLink displayLinkWithTarget:target selector:@selector(tick:)];
        [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
        linkTarget  = (__bridge_retained void *)target;
        displayLink = (__bridge_retained void *)link;
    }

    void IOSPlatform::StopFrameLoop()
    {
        frameState.Stop();
        InvalidateDisplayLink();
    }

    void IOSPlatform::InvalidateDisplayLink()
    {
        if (displayLink != nullptr) {
            auto *link = (__bridge_transfer CADisplayLink *)displayLink;
            [link invalidate];
            displayLink = nullptr;
        }
        if (linkTarget != nullptr) {
            SkyDisplayLinkTarget *target = (__bridge_transfer SkyDisplayLinkTarget *)linkTarget;
            target.owner                 = nullptr;
            linkTarget                   = nullptr;
        }
    }

    void IOSPlatform::OnDisplayTick()
    {
        if (frameState.ShouldRunLoop() && frameCallback) {
            // The callback may stop the loop or clear itself during shutdown.
            const auto callback = frameCallback;
            callback();
        }
    }

    IOSPlatform::~IOSPlatform()
    {
        StopFrameLoop();
        frameCallback = {};
        if (windowScene != nullptr) {
            DisconnectScene(windowScene);
        }
    }

} // namespace sky

namespace sky::ios {

    namespace {
        IOSPlatform *GetScenePlatform()
        {
            Platform *platform = Platform::Get();
            return platform->GetImpl() != nullptr && platform->GetType() == PlatformType::IOS ? static_cast<IOSPlatform *>(platform->GetImpl())
                                                                                              : nullptr;
        }
    } // namespace

    bool ConnectScene(void *windowScene)
    {
        IOSPlatform *platform = GetScenePlatform();
        return platform != nullptr && platform->ConnectScene(windowScene);
    }

    void SetSceneActive(void *windowScene, bool active)
    {
        if (IOSPlatform *platform = GetScenePlatform()) {
            platform->SetSceneActive(windowScene, active);
        }
    }

    void DisconnectScene(void *windowScene)
    {
        if (IOSPlatform *platform = GetScenePlatform()) {
            platform->DisconnectScene(windowScene);
        }
    }

} // namespace sky::ios
