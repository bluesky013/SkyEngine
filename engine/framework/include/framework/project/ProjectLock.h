//
// Created on 2026/10/05.
//

#pragma once

#include <string>

namespace sky {

    // Single-editor-instance-per-project lock. Writes the owner PID to
    // `<projectDir>/cache/editor.lock`; a second editor is refused while the owner
    // process is alive. A stale lock (owner gone) is reclaimed automatically.
    class ProjectLock {
    public:
        ProjectLock() = default;
        ~ProjectLock();

        ProjectLock(const ProjectLock &) = delete;
        ProjectLock &operator=(const ProjectLock &) = delete;

        // Returns false if the project is already locked by a live process.
        bool Acquire(const std::string &projectDir);
        void Release();

        bool IsLocked() const { return locked; }

    private:
        std::string lockPath;
        bool        locked = false;
    };

} // namespace sky
