//
// Native Cocoa platform implementation (no SDL).
//

#include "MacosPlatform.h"
#include "CocoaWindow.h"

#include <core/logger/Logger.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#import <Cocoa/Cocoa.h>
#include <mach/mach_time.h>

static const char *TAG = "MacosPlatform";

namespace sky {

    namespace {

        // "*.png;*.jpg" -> ["png", "jpg"]; empty filter -> nil (all files)
        NSArray<NSString *> *DialogFileTypes(const std::string &filter)
        {
            if (filter.empty()) {
                return nil;
            }
            auto *types = [NSMutableArray array];
            std::string rest = filter;
            size_t pos = 0;
            while ((pos = rest.find(';')) != std::string::npos || !rest.empty()) {
                std::string token = rest.substr(0, pos);
                rest = (pos == std::string::npos) ? "" : rest.substr(pos + 1);
                const auto dot = token.find_last_of('.');
                if (dot != std::string::npos && dot + 1 < token.size()) {
                    [types addObject:[NSString stringWithUTF8String:token.substr(dot + 1).c_str()]];
                }
                if (pos == std::string::npos) {
                    break;
                }
            }
            return types.count > 0 ? types : nil;
        }

    } // namespace

    bool Platform::Init(const PlatformInfo &info)
    {
        platform = std::make_unique<MacosPlatform>();
        return platform->Init(info);
    }

    bool MacosPlatform::Init(const PlatformInfo & /*info*/)
    {
        // Pre-create/activate NSApplication so window creation cannot fail on it.
        return EnsureNSApplication();
    }

    uint64_t MacosPlatform::GetPerformanceFrequency() const
    {
        mach_timebase_info_data_t info = {};
        mach_timebase_info(&info);
        // ticks per second = 1e9 * denom / numer
        return static_cast<uint64_t>(1e9) * info.denom / info.numer;
    }

    uint64_t MacosPlatform::GetPerformanceCounter() const
    {
        return mach_absolute_time();
    }

    std::string MacosPlatform::GetInternalPath() const
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        return [documentsDirectory UTF8String];
    }

    std::string MacosPlatform::GetBundlePath() const
    {
        auto *path = [[NSBundle mainBundle] bundlePath];
        return [path UTF8String];
    }

    std::string MacosPlatform::GetUserConfigPath() const
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        if (paths.count == 0) {
            return {};
        }
        const std::filesystem::path configDir =
            std::filesystem::path([[paths objectAtIndex:0] UTF8String]) / "SkyEngine" / "SkyEditor";

        std::error_code ec;
        std::filesystem::create_directories(configDir, ec);

        std::string result = configDir.string();
        if (!result.empty() && result.back() != '/') {
            result += '/';
        }
        return result;
    }

    std::string MacosPlatform::GetEnvVariable(const std::string &env) const
    {
        const char *value = std::getenv(env.c_str());
        return value != nullptr ? std::string(value) : std::string();
    }

    bool MacosPlatform::RunCmd(const std::string &cmd, std::string &out) const
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

    char *MacosPlatform::GetClipBoardText()
    {
        NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
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

    void MacosPlatform::FreeClipBoardText(char *text)
    {
        std::free(text);
    }

    void MacosPlatform::SetClipBoardText(const std::string &text)
    {
        auto *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:[NSString stringWithUTF8String:text.c_str()]
                      forType:NSPasteboardTypeString];
    }

    void MacosPlatform::PollEvent(bool &exit)
    {
        // Drain the previous frame's pool and open a fresh one so autoreleased
        // objects created anywhere during this frame are reclaimed on the next
        // frame boundary (the loop itself has no @autoreleasepool).
        if (framePool != nullptr) {
            [(NSAutoreleasePool *)framePool drain];
        }
        framePool = [[NSAutoreleasePool alloc] init];

        @autoreleasepool {
            for (;;) {
                NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate distantPast]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event == nil) {
                    break;
                }
                [NSApp sendEvent:event];
            }
            [NSApp updateWindows];
        }
        if (exitRequested) {
            exit = true;
        }
    }

    MacosPlatform::~MacosPlatform()
    {
        if (framePool != nullptr) {
            [(NSAutoreleasePool *)framePool drain];
            framePool = nullptr;
        }
    }

    bool MacosPlatform::ShowOpenFileDialog(void * /*owner*/, std::string &outPath, const std::string &title,
                                           const std::string &filter)
    {
        auto *panel = [NSOpenPanel openPanel];
        if (!title.empty()) {
            [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        }
        [panel setAllowedFileTypes:DialogFileTypes(filter)];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanChooseDirectories:NO];
        if ([panel runModal] == NSModalResponseOK) {
            outPath = [[panel.URL path] UTF8String];
            return true;
        }
        return false;
    }

    bool MacosPlatform::ShowSaveFileDialog(void * /*owner*/, std::string &outPath, const std::string &title,
                                           const std::string &filter)
    {
        auto *panel = [NSSavePanel savePanel];
        if (!title.empty()) {
            [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        }
        [panel setAllowedFileTypes:DialogFileTypes(filter)];
        if ([panel runModal] == NSModalResponseOK) {
            outPath = [[panel.URL path] UTF8String];
            return true;
        }
        return false;
    }

} // namespace sky
