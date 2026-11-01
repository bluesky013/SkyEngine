#include "OHOSBundleFileSystem.h"

#include <algorithm>
#include <core/logger/Logger.h>
#include <limits>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include <sstream>
#include <string_view>

namespace sky {

    namespace {
        constexpr const char *TAG = "OHOSRawfile";

        bool NormalizePath(std::string_view path, std::string &out)
        {
            out.clear();
            if ((!path.empty() && (path.front() == '/' || path.front() == '\\')) || path.find('\0') != std::string_view::npos ||
                path.find(':') != std::string_view::npos) {
                return false;
            }
            size_t offset = 0;
            while (offset < path.size()) {
                const size_t end     = path.find_first_of("/\\", offset);
                const auto   segment = path.substr(offset, (end == std::string_view::npos ? path.size() : end) - offset);
                if (segment == "..") {
                    out.clear();
                    return false;
                }
                if (!segment.empty() && segment != ".") {
                    if (!out.empty()) {
                        out += '/';
                    }
                    out.append(segment);
                }
                if (end == std::string_view::npos) {
                    break;
                }
                offset = end + 1;
            }
            return true;
        }

        struct RawFileCloser {
            void operator()(RawFile64 *file) const
            {
                OH_ResourceManager_CloseRawFile64(file);
            }
        };
        using RawFileHandle = std::unique_ptr<RawFile64, RawFileCloser>;

        bool ReadExact(RawFile64 *file, uint8_t *data, size_t size)
        {
            if (size != 0 && data == nullptr) {
                return false;
            }
            size_t offset = 0;
            while (offset < size) {
                const auto    chunk = static_cast<int64_t>(std::min(size - offset, size_t{1024 * 1024}));
                const int64_t count = OH_ResourceManager_ReadRawFile64(file, data + offset, chunk);
                if (count <= 0 || count > chunk) {
                    return false;
                }
                offset += static_cast<size_t>(count);
            }
            return true;
        }

        class RawfileArchive final : public IStreamArchive {
        public:
            explicit RawfileArchive(std::string data) : IStreamArchive(stream), stream(std::move(data), std::ios::in | std::ios::binary)
            {
            }

        private:
            std::istringstream stream;
        };

        class Rawfile final : public IFile {
        public:
            Rawfile(std::shared_ptr<NativeResourceManager> resources, std::string name) : manager(std::move(resources)), path(std::move(name))
            {
            }

            void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override
            {
                auto          file   = Open();
                const int64_t length = file ? OH_ResourceManager_GetRawFileSize64(file.get()) : -1;
                if (length < 0 || offset > static_cast<uint64_t>(length) || size > static_cast<uint64_t>(length) - offset ||
                    size > std::numeric_limits<size_t>::max() ||
                    OH_ResourceManager_SeekRawFile64(file.get(), static_cast<int64_t>(offset), SEEK_SET) != 0 ||
                    !ReadExact(file.get(), out, static_cast<size_t>(size))) {
                    LOG_E(TAG, "Cannot read rawfile range '%s'", path.c_str());
                }
            }

            bool ReadBin(std::vector<uint8_t> &out) override
            {
                out.clear();
                auto          file   = Open();
                const int64_t length = file ? OH_ResourceManager_GetRawFileSize64(file.get()) : -1;
                if (length < 0 || static_cast<uint64_t>(length) > out.max_size()) {
                    return false;
                }
                out.resize(static_cast<size_t>(length));
                if (!ReadExact(file.get(), out.data(), out.size())) {
                    out.clear();
                    return false;
                }
                return true;
            }

            BinaryDataPtr ReadBin() override
            {
                auto          file   = Open();
                const int64_t length = file ? OH_ResourceManager_GetRawFileSize64(file.get()) : -1;
                if (length < 0 || static_cast<uint64_t>(length) > std::numeric_limits<uint32_t>::max()) {
                    return {};
                }
                BinaryDataPtr data = new BinaryData(static_cast<uint32_t>(length));
                return ReadExact(file.get(), data->Data(), data->Size()) ? data : BinaryDataPtr{};
            }

            bool ReadString(std::string &out) override
            {
                out.clear();
                std::vector<uint8_t> bytes;
                if (!ReadBin(bytes) || bytes.size() > out.max_size()) {
                    return false;
                }
                if (!bytes.empty()) {
                    out.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
                }
                return true;
            }

            IStreamArchivePtr ReadAsArchive() override
            {
                std::string bytes;
                return ReadString(bytes) ? IStreamArchivePtr(new RawfileArchive(std::move(bytes))) : IStreamArchivePtr{};
            }
            OStreamArchivePtr WriteAsArchive() override
            {
                return {};
            }
            uint64_t AppendData(const char *, uint64_t) override
            {
                return 0;
            }
            std::string GetPath() const override
            {
                return path;
            }

        private:
            RawFileHandle Open() const
            {
                return RawFileHandle(OH_ResourceManager_OpenRawFile64(manager.get(), path.c_str()));
            }
            std::shared_ptr<NativeResourceManager> manager;
            std::string                            path;
        };

        class RawfileSystem final : public IFileSystem {
        public:
            RawfileSystem(std::shared_ptr<NativeResourceManager> resources, std::string root) : manager(std::move(resources)), base(std::move(root))
            {
            }

            bool FileExist(const FilePath &path) const override
            {
                std::string name;
                return Resolve(path.GetStr(), name) && RawFileHandle(OH_ResourceManager_OpenRawFile64(manager.get(), name.c_str())) != nullptr;
            }
            FilePtr OpenFile(const FilePath &path) override
            {
                std::string name;
                if (!Resolve(path.GetStr(), name) || !RawFileHandle(OH_ResourceManager_OpenRawFile64(manager.get(), name.c_str()))) {
                    return {};
                }
                return new Rawfile(manager, std::move(name));
            }
            FilePtr CreateOrOpenFile(const FilePath &) override
            {
                return {};
            }
            FileSystemPtr CreateSubSystem(const std::string &path, bool createDir) override
            {
                std::string name;
                if (createDir || !NormalizePath(path, name)) {
                    return {};
                }
                name = base + name;
                if (!name.empty() && name.back() != '/') {
                    name += '/';
                }
                return new RawfileSystem(manager, std::move(name));
            }

        private:
            bool Resolve(const std::string &path, std::string &out) const
            {
                if (!NormalizePath(path, out) || out.empty()) {
                    return false;
                }
                out = base + out;
                return true;
            }
            std::shared_ptr<NativeResourceManager> manager;
            std::string                            base;
        };
    } // namespace

    FileSystemPtr CreateOHOSBundleFileSystem(std::shared_ptr<NativeResourceManager> manager)
    {
        return manager ? FileSystemPtr(new RawfileSystem(std::move(manager), {})) : FileSystemPtr{};
    }

} // namespace sky
