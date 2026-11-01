#pragma once

#include <core/environment/Singleton.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace sky {
    enum class ScanCode : uint32_t;

    // The process-owned receiver survives sessions; sinks must reject obsolete host session tokens.
    class OHOSIME final : public Singleton<OHOSIME> {
    public:
        using TextSink = std::function<void(std::u16string)>;
        using KeySink  = std::function<void(ScanCode, uint32_t)>;

        ~OHOSIME() override;
        bool Start(uint32_t windowId, TextSink text, KeySink key);
        bool ShowKeyboard();
        bool Stop();
        bool SetRect(int32_t x, int32_t y, int32_t width, int32_t height);

    private:
        friend class Singleton<OHOSIME>;
        using Singleton<OHOSIME>::Destroy;
        OHOSIME();
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
} // namespace sky
