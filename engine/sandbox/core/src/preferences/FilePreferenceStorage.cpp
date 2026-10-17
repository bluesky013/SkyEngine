#include <editor/core/preferences/FilePreferenceStorage.h>

#include <core/util/Uuid.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <memory>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <io.h>
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace sky::editor {
    namespace {
        namespace fs = std::filesystem;

        PreferenceResult Failure(PreferenceCode code, std::string message)
        {
            return {code, {}, {}, std::move(message)};
        }

        PreferenceResult IoFailure(PreferenceCode code, const char *operation, const std::error_code &error)
        {
            return Failure(code, std::string(operation) + ": " + error.message());
        }

        std::error_code IoError()
        {
            return {errno != 0 ? errno : EIO, std::generic_category()};
        }

        bool ValidPath(const fs::path &path)
        {
            return !path.empty() && path.has_filename() && path.filename() != "." && path.filename() != ".." &&
                   path.native().find(fs::path::value_type{}) == fs::path::string_type::npos;
        }

        // A missing leaf under a non-directory (e.g. `file/child`) is an invalid path, not
        // absent storage; classify structurally because the OS errno differs across platforms.
        bool LeafMissingUnderNonDirectory(const fs::path &path)
        {
            for (auto parent = path.parent_path(); !parent.empty(); parent = parent.parent_path()) {
                std::error_code error;
                const auto      status = fs::status(parent, error);
                if (error || status.type() == fs::file_type::not_found) {
                    continue;
                }
                return status.type() != fs::file_type::directory;
            }
            return false;
        }

        struct TemporaryFile {
            fs::path path;
            FILE    *stream    = nullptr;
            bool     owned     = false;
            bool     committed = false;

            ~TemporaryFile()
            {
                if (stream != nullptr)
                    std::fclose(stream);
                if (owned && !committed) {
                    std::error_code ignored;
                    fs::remove(path, ignored);
                }
            }

            bool Open()
            {
#if defined(_WIN32)
                const int descriptor = ::_wopen(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
#else
                const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
#endif
                if (descriptor < 0)
                    return false;
                owned = true;
#if defined(_WIN32)
                stream = ::_fdopen(descriptor, "wb");
#else
                stream = ::fdopen(descriptor, "wb");
#endif
                if (stream == nullptr) {
                    const int error = errno;
#if defined(_WIN32)
                    ::_close(descriptor);
#else
                    ::close(descriptor);
#endif
                    errno = error;
                }
                return stream != nullptr;
            }

            bool Close()
            {
                FILE *file = std::exchange(stream, nullptr);
                return std::fclose(file) == 0;
            }
        };

        std::error_code Replace(const fs::path &from, const fs::path &to)
        {
#if defined(_WIN32)
            // No COPY_ALLOWED: both names are in the same directory and the
            // old file must never be removed before the replacement is ready.
            if (!::MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                return {static_cast<int>(::GetLastError()), std::system_category()};
            }
#else
            if (std::rename(from.c_str(), to.c_str()) != 0) {
                return {errno, std::generic_category()};
            }
#endif
            return {};
        }
    } // namespace

    FilePreferenceStorage::FilePreferenceStorage(std::filesystem::path path) : path(std::move(path))
    {
    }

    const std::filesystem::path &FilePreferenceStorage::GetPath() const
    {
        return path;
    }

    PreferenceResult FilePreferenceStorage::Read(std::string &document)
    {
        if (!ValidPath(path))
            return Failure(PreferenceCode::ReadFailed, "Invalid preferences file path");
        std::error_code error;
        const auto      status = fs::status(path, error);
        if (!error && status.type() == fs::file_type::not_found) {
            if (LeafMissingUnderNonDirectory(path)) {
                return Failure(PreferenceCode::ReadFailed, "Preferences path component is not a directory");
            }
            return Failure(PreferenceCode::NotFound, "Preferences file does not exist");
        }
        if (error) {
            if (LeafMissingUnderNonDirectory(path)) {
                return Failure(PreferenceCode::ReadFailed, "Preferences path component is not a directory");
            }
            if (error == std::errc::no_such_file_or_directory) {
                return Failure(PreferenceCode::NotFound, "Preferences file does not exist");
            }
            return IoFailure(PreferenceCode::ReadFailed, "Could not inspect preferences file", error);
        }
        if (!fs::is_regular_file(status))
            return Failure(PreferenceCode::ReadFailed, "Preferences path is not a regular file");
        errno = 0;
#if defined(_WIN32)
        FILE *stream = ::_wfopen(path.c_str(), L"rb");
#else
        FILE *stream = std::fopen(path.c_str(), "rb");
#endif
        if (stream == nullptr)
            return IoFailure(PreferenceCode::ReadFailed, "Could not open preferences file", IoError());
        std::unique_ptr<FILE, decltype(&std::fclose)> input(stream, &std::fclose);
        std::string                                   next;
        std::array<char, 8192>                        buffer;
        for (;;) {
            errno            = 0;
            const auto count = std::fread(buffer.data(), 1, buffer.size(), input.get());
            if (std::ferror(input.get()))
                return IoFailure(PreferenceCode::ReadFailed, "Could not read complete preferences file", IoError());
            next.append(buffer.data(), count);
            if (std::feof(input.get()))
                break;
            if (count == 0)
                return IoFailure(PreferenceCode::ReadFailed, "Could not read complete preferences file", IoError());
        }
        document.swap(next);
        return {};
    }

    PreferenceResult FilePreferenceStorage::Write(const std::string &document)
    {
        if (!ValidPath(path))
            return Failure(PreferenceCode::WriteFailed, "Invalid preferences file path");
        auto parent = path.parent_path();
        if (parent.empty())
            parent = ".";
        std::error_code error;
        fs::create_directories(parent, error);
        if (error)
            return IoFailure(PreferenceCode::WriteFailed, "Could not create preferences directory", error);
        TemporaryFile temporary;
        for (unsigned attempt = 0; attempt < 32; ++attempt) {
            temporary.path = parent / (".sky-preferences-" + Uuid::Create().ToString() + ".tmp");
            if (temporary.Open())
                break;
            if (temporary.owned || errno != EEXIST) {
                return IoFailure(PreferenceCode::WriteFailed, "Could not create preferences temporary file", IoError());
            }
        }
        if (temporary.stream == nullptr)
            return Failure(PreferenceCode::WriteFailed, "Could not reserve a preferences temporary file");
        std::size_t offset = 0;
        while (offset < document.size()) {
            errno            = 0;
            const auto count = std::fwrite(document.data() + offset, 1, document.size() - offset, temporary.stream);
            if (count == 0 || std::ferror(temporary.stream)) {
                return IoFailure(PreferenceCode::WriteFailed, "Could not write complete preferences temporary file", IoError());
            }
            offset += count;
        }
        errno = 0;
        if (std::fflush(temporary.stream) != 0)
            return IoFailure(PreferenceCode::WriteFailed, "Could not flush preferences temporary file", IoError());
        errno = 0;
        if (!temporary.Close())
            return IoFailure(PreferenceCode::WriteFailed, "Could not close preferences temporary file", IoError());
        // Prepare the result before committing; no fallible work follows a
        // successful rename, so a returned failure cannot hide replacement.
        PreferenceResult result;
        result.storageWritten = true;
        error                 = Replace(temporary.path, path);
        if (error)
            return IoFailure(PreferenceCode::WriteFailed, "Could not replace preferences file", error);
        temporary.committed = true;
        return result;
    }
} // namespace sky::editor
