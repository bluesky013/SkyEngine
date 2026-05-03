//
// Created on 2026/04/01.
//

#include <GLESFence.h>
#include <GLESLoader.h>

namespace sky::aurora {

    GLESFence::~GLESFence()
    {
        if (syncObj != nullptr) {
            glDeleteSync(syncObj);
        }
    }

    bool GLESFence::Init(const Descriptor &desc)
    {
        signaled = desc.createSignaled;
        return true;
    }

    void GLESFence::Wait()
    {
        if (signaled) {
            return;
        }
        if (syncObj != nullptr) {
            glWaitSync(syncObj, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(syncObj);
            syncObj = nullptr;
        }
        signaled = true;
    }

    void GLESFence::Reset()
    {
        if (syncObj != nullptr) {
            glDeleteSync(syncObj);
        }
        syncObj  = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        signaled = false;
    }

    bool GLESFence::IsSignaled()
    {
        if (signaled) {
            return true;
        }
        if (syncObj != nullptr) {
            const GLenum r = glClientWaitSync(syncObj, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
            if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) {
                signaled = true;
            }
        }
        return signaled;
    }

    bool GLESFence::WaitFor(uint64_t timeoutNs)
    {
        if (signaled) {
            return true;
        }
        if (syncObj != nullptr) {
            const GLenum r = glClientWaitSync(syncObj, GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNs);
            if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) {
                signaled = true;
                return true;
            }
        }
        return false;
    }

    void GLESFence::SignalFromQueue()
    {
        if (syncObj != nullptr) {
            glDeleteSync(syncObj);
        }
        syncObj  = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        signaled = false;
    }

} // namespace sky::aurora
