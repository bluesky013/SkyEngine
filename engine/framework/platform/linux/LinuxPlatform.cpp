//
// Native X11 platform implementation (no SDL).
//

#include "LinuxPlatform.h"
#include "LinuxWindow.h"
#include "LinuxX11.h"

#include <framework/window/NativeWindowManager.h>

#include <core/logger/Logger.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>

static const char *TAG = "LinuxPlatform";

namespace sky::x11 {

    namespace {
        Display *gDisplay = nullptr;
        bool     gExitRequested = false;
    } // namespace

    Display *GetDisplay()
    {
        return gDisplay;
    }

    void SetDisplay(Display *display)
    {
        gDisplay = display;
    }

    xcb_connection_t *GetXcbConnection(Display *display)
    {
        if (display == nullptr) {
            return nullptr;
        }

        using XGetXCBConnectionFn = xcb_connection_t *(*)(Display *);
        static XGetXCBConnectionFn fn = []() -> XGetXCBConnectionFn {
            void *lib = dlopen("libX11-xcb.so.1", RTLD_LAZY | RTLD_LOCAL);
            if (lib == nullptr) {
                LOG_E(TAG, "libX11-xcb.so.1 not found: %s", dlerror());
                return nullptr;
            }
            return reinterpret_cast<XGetXCBConnectionFn>(dlsym(lib, "XGetXCBConnection"));
        }();

        return fn != nullptr ? fn(display) : nullptr;
    }

    void RequestExit()
    {
        gExitRequested = true;
    }

    bool ConsumeExit()
    {
        const bool value = gExitRequested;
        gExitRequested   = false;
        return value;
    }

} // namespace sky::x11

namespace sky {

    namespace {
        std::string gClipboardText;
    } // namespace

    LinuxPlatform::~LinuxPlatform()
    {
        if (ownsDisplay) {
            if (Display *display = x11::GetDisplay()) {
                XCloseDisplay(display);
            }
            x11::SetDisplay(nullptr);
        }
    }

    bool Platform::Init(const PlatformInfo &info)
    {
        platform = std::make_unique<LinuxPlatform>();
        return platform->Init(info);
    }

    bool LinuxPlatform::Init(const PlatformInfo & /*info*/)
    {
        XInitThreads();
        Display *display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            // Headless hosts (cook/worker) still need paths and timing; window
            // creation will fail later with a clear error.
            LOG_W(TAG, "cannot open X11 display; running without windowing");
            return true;
        }

        x11::SetDisplay(display);
        ownsDisplay = true;
        return true;
    }

    PlatformType LinuxPlatform::GetType() const
    {
        return PlatformType::Linux;
    }

    uint64_t LinuxPlatform::GetPerformanceFrequency() const
    {
        return 1000000000ull;
    }

    uint64_t LinuxPlatform::GetPerformanceCounter() const
    {
        using Clock = std::chrono::steady_clock;
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
    }

    std::string LinuxPlatform::GetInternalPath() const
    {
        std::array<char, PATH_MAX + 1> buffer{};
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (length <= 0) {
            return {};
        }
        buffer[static_cast<size_t>(length)] = '\0';
        return std::filesystem::path(buffer.data()).parent_path().string();
    }

    std::string LinuxPlatform::GetBundlePath() const
    {
        return GetInternalPath();
    }

    std::string LinuxPlatform::GetUserConfigPath() const
    {
        const char *xdg = std::getenv("XDG_CONFIG_HOME");
        std::string base;
        if (xdg != nullptr && xdg[0] != '\0') {
            base = xdg;
        } else {
            const char *home = std::getenv("HOME");
            if (home == nullptr || home[0] == '\0') {
                return {};
            }
            base = std::string(home) + "/.config";
        }
        if (!base.empty() && base.back() != '/') {
            base += '/';
        }
        return base + "SkyEngine/";
    }

    std::string LinuxPlatform::GetEnvVariable(const std::string &env) const
    {
        const char *value = std::getenv(env.c_str());
        return value != nullptr ? std::string(value) : std::string();
    }

    bool LinuxPlatform::RunCmd(const std::string &str, std::string &out) const
    {
        FILE *pipe = ::popen(str.c_str(), "r");
        if (pipe == nullptr) {
            return false;
        }

        std::array<char, 256> buffer{};
        while (::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            out += buffer.data();
        }
        return ::pclose(pipe) == 0;
    }

    char *LinuxPlatform::GetClipBoardText()
    {
        if (gClipboardText.empty()) {
            return nullptr;
        }
        auto *text = static_cast<char *>(::malloc(gClipboardText.size() + 1));
        if (text == nullptr) {
            return nullptr;
        }
        std::memcpy(text, gClipboardText.c_str(), gClipboardText.size() + 1);
        return text;
    }

    void LinuxPlatform::FreeClipBoardText(char *text)
    {
        ::free(text);
    }

    void LinuxPlatform::SetClipBoardText(const std::string &text)
    {
        gClipboardText = text;
    }

    void LinuxPlatform::PollEvent(bool &exit)
    {
        if (Display *display = x11::GetDisplay()) {
            while (XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);
                auto *window = static_cast<LinuxWindow *>(NativeWindowManager::Get()->GetWindowByID(static_cast<WindowID>(event.xany.window)));
                if (window != nullptr) {
                    window->DispatchNativeEvent(&event);
                }
            }
            XFlush(display);
        }

        if (x11::ConsumeExit()) {
            exit = true;
        }
    }

    bool LinuxPlatform::ShowOpenFileDialog(void * /*owner*/, std::string & /*outPath*/, const std::string & /*title*/, const std::string & /*filter*/)
    {
        return false;
    }

    bool LinuxPlatform::ShowSaveFileDialog(void * /*owner*/, std::string & /*outPath*/, const std::string & /*title*/, const std::string & /*filter*/)
    {
        return false;
    }

    void LinuxPlatform::RevealInFileExplorer(const std::string & /*path*/)
    {
    }

} // namespace sky
