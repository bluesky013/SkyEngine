#pragma once

#include <memory>
#include <string>

namespace sky {

    enum class FileOperationError { None, Unsupported, InvalidPath, IOError, Busy, AlreadyExists };

    struct FileOperationStatus {
        FileOperationError error = FileOperationError::None;
        std::string        message;
        explicit           operator bool() const
        {
            return error == FileOperationError::None;
        }
    };

    // The OS releases this lock on destruction or process exit; the lock file is persistent.
    class IFileLock {
    public:
        virtual ~IFileLock() = default;
    };
    using FileLockPtr = std::unique_ptr<IFileLock>;

} // namespace sky
