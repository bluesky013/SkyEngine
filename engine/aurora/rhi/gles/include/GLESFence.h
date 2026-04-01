//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Fence.h>
#include <GLESForward.h>

namespace sky::aurora {

    class GLESFence : public Fence {
    public:
        GLESFence() = default;
        ~GLESFence() override;

        bool Init(const Descriptor &desc);

        void Wait() override;
        void Reset() override;

    private:
        GLsync syncObj   = nullptr;
        bool   signaled  = false;
    };

} // namespace sky::aurora
