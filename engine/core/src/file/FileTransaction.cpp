#include <core/file/FileSystem.h>

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <limits>

#if defined(_WIN32)
    #include <Windows.h>
#elif defined(__unix__) || defined(__APPLE__)
    #include <fcntl.h>
    #include <sys/file.h>
    #include <unistd.h>
#endif

namespace sky {
    namespace {
        FileOperationStatus ResolvePath(const FilePath &root, const FilePath &path, std::filesystem::path &out)
        {
            const std::filesystem::path base(root.GetStr());
            const std::filesystem::path input(path.GetStr());
            const auto                  resolved = (input.is_absolute() ? input : base / input).lexically_normal();
            const auto                  relative = resolved.lexically_relative(base);
            if (relative.empty() || relative == "." || relative.is_absolute() ||
                std::any_of(relative.begin(), relative.end(), [](const auto &part) { return part == ".."; }) ||
                path.GetStr().find('\0') != std::string::npos) {
                return {FileOperationError::InvalidPath, "transaction path must name a file inside its filesystem"};
            }
            out = resolved;
            return {};
        }

        FileOperationStatus PreparePath(const FilePath &root, const FilePath &path, std::filesystem::path &out)
        {
            if (const auto status = ResolvePath(root, path, out); !status) {
                return status;
            }
            std::error_code error;
            std::filesystem::create_directories(out.parent_path(), error);
            return error ? FileOperationStatus{FileOperationError::IOError, error.message()} : FileOperationStatus{};
        }

#if defined(_WIN32)
        FileOperationStatus WinError(const char *operation, DWORD code = GetLastError())
        {
            const auto kind = code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS         ? FileOperationError::AlreadyExists
                              : code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION ? FileOperationError::Busy
                                                                                                : FileOperationError::IOError;
            return {kind, std::string(operation) + ": " + std::system_category().message(static_cast<int>(code))};
        }

        class NativeLock final : public IFileLock {
        public:
            explicit NativeLock(HANDLE value) : handle(value)
            {
            }
            ~NativeLock() override
            {
                CloseHandle(handle);
            }

        private:
            HANDLE handle;
        };
#elif defined(__unix__) || defined(__APPLE__)
        FileOperationStatus PosixError(const char *operation)
        {
            const int  code = errno;
            const auto kind = code == EEXIST                          ? FileOperationError::AlreadyExists
                              : code == EWOULDBLOCK || code == EAGAIN ? FileOperationError::Busy
                                                                      : FileOperationError::IOError;
            return {kind, std::string(operation) + ": " + std::generic_category().message(code)};
        }

        class NativeLock final : public IFileLock {
        public:
            explicit NativeLock(int value) : descriptor(value)
            {
            }
            ~NativeLock() override
            {
                close(descriptor);
            }

        private:
            int descriptor;
        };
#endif
    } // namespace

    FileOperationStatus NativeFileSystem::WriteFileExclusive(const FilePath &path, std::span<const uint8_t> bytes) const
    {
        std::filesystem::path resolved;
        if (const auto status = PreparePath(fsRoot, path, resolved); !status) {
            return status;
        }
#if defined(_WIN32)
        const HANDLE file = CreateFileW(resolved.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return WinError("create exclusive file");
        }
        FileOperationStatus status;
        size_t              offset = 0;
        while (offset < bytes.size()) {
            const auto size    = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, MAXDWORD));
            DWORD      written = 0;
            if (!WriteFile(file, bytes.data() + offset, size, &written, nullptr) || written != size) {
                status = WinError("write file");
                break;
            }
            offset += written;
        }
        if (status && !FlushFileBuffers(file)) {
            status = WinError("flush file");
        }
        if (!CloseHandle(file) && status) {
            status = WinError("close file");
        }
        return status;
#elif defined(__unix__) || defined(__APPLE__)
        const int file = open(resolved.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (file < 0) {
            return PosixError("create exclusive file");
        }
        FileOperationStatus status;
        size_t              offset = 0;
        while (offset < bytes.size()) {
            const auto written = write(file, bytes.data() + offset, std::min<size_t>(bytes.size() - offset, std::numeric_limits<ssize_t>::max()));
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written <= 0) {
                status = PosixError("write file");
                break;
            }
            offset += static_cast<size_t>(written);
        }
        if (status && fsync(file) != 0) {
            status = PosixError("flush file");
        }
        if (close(file) != 0 && status) {
            status = PosixError("close file");
        }
        return status;
#else
        return {FileOperationError::Unsupported, "durable exclusive writes are unavailable"};
#endif
    }

    FileOperationStatus NativeFileSystem::ReplaceFileAtomic(const FilePath &from, const FilePath &to) const
    {
        std::filesystem::path source, destination;
        if (const auto status = ResolvePath(fsRoot, from, source); !status) {
            return status;
        }
        if (const auto status = PreparePath(fsRoot, to, destination); !status) {
            return status;
        }
        if (source == destination) {
            return {FileOperationError::InvalidPath, "atomic replacement requires distinct files"};
        }
#if defined(_WIN32)
        return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? FileOperationStatus{}
                                                                                                                    : WinError("atomic replace");
#elif defined(__unix__) || defined(__APPLE__)
        // Rename is the commit point; a cross-device move fails without copying.
        return rename(source.c_str(), destination.c_str()) == 0 ? FileOperationStatus{} : PosixError("atomic replace");
#else
        return {FileOperationError::Unsupported, "atomic replacement is unavailable"};
#endif
    }

    FileOperationStatus NativeFileSystem::AcquireFileLock(const FilePath &path, FileLockPtr &out) const
    {
        std::filesystem::path resolved;
        if (const auto status = PreparePath(fsRoot, path, resolved); !status) {
            return status;
        }
#if defined(_WIN32)
        const HANDLE file = CreateFileW(resolved.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return WinError("acquire file lock");
        }
        out = std::make_unique<NativeLock>(file);
        return {};
#elif defined(__unix__) || defined(__APPLE__)
        const int file = open(resolved.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
        if (file < 0) {
            return PosixError("open lock file");
        }
        if (flock(file, LOCK_EX | LOCK_NB) != 0) {
            const auto status = PosixError("acquire file lock");
            close(file);
            return status;
        }
        out = std::make_unique<NativeLock>(file);
        return {};
#else
        return {FileOperationError::Unsupported, "exclusive file locks are unavailable"};
#endif
    }
} // namespace sky
