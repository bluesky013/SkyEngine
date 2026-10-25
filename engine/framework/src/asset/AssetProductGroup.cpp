#include <framework/asset/AssetProductGroup.h>

#include <algorithm>
#include <array>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/serialization/BinaryArchive.h>
#include <unordered_set>

namespace sky {
    namespace {
        constexpr uint32_t GROUP_MAGIC   = 0x47505244;
        constexpr uint32_t GROUP_VERSION = 1;
        constexpr uint32_t MAX_MEMBERS   = 4096;

        FilePath GroupPath(const Uuid &owner, const std::string &file)
        {
            return FilePath(".groups/" + owner.ToString() + "/" + file);
        }

        FilePath OwnerPath(const Uuid &member)
        {
            return FilePath(".groups/owners/" + member.ToString() + ".bin");
        }

        FilePath LegacyPath(const Uuid &member)
        {
            const auto id = member.ToString();
            return FilePath(id.substr(0, 2) + "/" + id + ".bin");
        }

        FilePath MemberPath(const ProductGroupSnapshot &snapshot, const Uuid &member)
        {
            return GroupPath(snapshot.owner, snapshot.generation.ToString() + "/" + member.ToString() + ".bin");
        }

        ProductStatus Convert(const FileOperationStatus &status)
        {
            if (status) {
                return {};
            }
            return {status.error == FileOperationError::Unsupported ? ProductError::Unsupported
                    : status.error == FileOperationError::Busy      ? ProductError::Busy
                                                                    : ProductError::IOError,
                    status.message};
        }

        uint64_t Hash(std::span<const uint8_t> bytes, uint64_t value = 14695981039346656037ULL)
        {
            for (const auto byte : bytes) {
                value = (value ^ byte) * 1099511628211ULL;
            }
            return value;
        }

        ProductStatus ReadMetadata(const FileSystemPtr &fs, const FilePath &path, std::vector<uint8_t> &out)
        {
            const auto file = fs->OpenFile(path);
            if (!file) {
                return {ProductError::NotFound, "product metadata is missing: " + path.GetStr()};
            }
            const auto input = file->ReadAsArchive();
            if (!input || !input->IsOpen()) {
                return {ProductError::IOError, "cannot read product metadata"};
            }
            auto &stream = input->GetStream();
            stream.seekg(0, std::ios::end);
            const auto size = stream.tellg();
            if (!stream || size < 0 || size > 1024 * 1024) {
                return {ProductError::InvalidData, "invalid product metadata length"};
            }
            out.resize(static_cast<size_t>(size));
            stream.seekg(0);
            if (!input->LoadRaw(reinterpret_cast<char *>(out.data()), out.size())) {
                return {ProductError::IOError, "truncated product metadata"};
            }
            return {};
        }

        ProductStatus ReadUuid(const FileSystemPtr &fs, const FilePath &path, Uuid &out)
        {
            std::vector<uint8_t> bytes;
            if (const auto status = ReadMetadata(fs, path, bytes); !status) {
                return status;
            }
            if (bytes.size() != sizeof(Uuid)) {
                return {ProductError::InvalidData, "invalid product owner or request record"};
            }
            std::memcpy(out.data, bytes.data(), bytes.size());
            return out ? ProductStatus{} : ProductStatus{ProductError::InvalidData, "empty product identity"};
        }

        ProductStatus ReadOwner(const FileSystemPtr &fs, const Uuid &member, Uuid &owner, bool &legacy)
        {
            std::vector<uint8_t> bytes;
            if (const auto status = ReadMetadata(fs, OwnerPath(member), bytes); !status) {
                return status;
            }
            if (bytes.size() != sizeof(Uuid) + sizeof(uint32_t)) {
                return {ProductError::InvalidData, "invalid product ownership record"};
            }
            uint32_t flag = 0;
            std::memcpy(owner.data, bytes.data(), sizeof(Uuid));
            std::memcpy(&flag, bytes.data() + sizeof(Uuid), sizeof(flag));
            if (!owner || flag > 1) {
                return {ProductError::InvalidData, "invalid product ownership metadata"};
            }
            legacy = flag != 0;
            return {};
        }

        ProductStatus Verify(const FilePtr &file, const ProductGroupMember &member)
        {
            const auto input = file ? file->ReadAsArchive() : IStreamArchivePtr{};
            if (!input || !input->IsOpen()) {
                return {ProductError::IOError, "product generation file is missing"};
            }
            auto &stream = input->GetStream();
            stream.seekg(0, std::ios::end);
            const auto size = stream.tellg();
            if (!stream || size < 0 || static_cast<uint64_t>(size) != member.size) {
                return {ProductError::InvalidData, "product generation length mismatch"};
            }
            stream.seekg(0);
            std::array<uint8_t, 16384> buffer{};
            uint64_t                   remaining = member.size;
            uint64_t                   checksum  = 14695981039346656037ULL;
            while (remaining != 0) {
                const auto count = static_cast<size_t>(std::min<uint64_t>(remaining, buffer.size()));
                if (!input->LoadRaw(reinterpret_cast<char *>(buffer.data()), count)) {
                    return {ProductError::IOError, "product readback failed"};
                }
                checksum = Hash(std::span(buffer.data(), count), checksum);
                remaining -= count;
            }
            return checksum == member.checksum ? ProductStatus{} : ProductStatus{ProductError::InvalidData, "product checksum mismatch"};
        }

        ProductStatus
        WritePointer(const FileSystemPtr &fs, const Uuid &owner, const std::string &name, const Uuid &generation, std::span<const uint8_t> bytes)
        {
            const auto staging = GroupPath(owner, name + "." + generation.ToString() + ".tmp");
            if (const auto status = fs->WriteFileExclusive(staging, bytes); !status) {
                return Convert(status);
            }
            std::vector<uint8_t> check;
            if (const auto status = ReadMetadata(fs, staging, check); !status) {
                return status;
            }
            if (!std::equal(bytes.begin(), bytes.end(), check.begin(), check.end())) {
                return {ProductError::IOError, "product metadata readback differs"};
            }
            return Convert(fs->ReplaceFileAtomic(staging, GroupPath(owner, name)));
        }
    } // namespace

    ProductStatus AssetProductGroupStore::Begin(const Uuid &owner, std::span<const Uuid> members, ProductGroupTransaction &out) const
    {
        const std::unordered_set<Uuid> uniqueMembers(members.begin(), members.end());
        if (!fs || !owner || members.empty() || members.size() > MAX_MEMBERS || uniqueMembers.size() != members.size() ||
            uniqueMembers.contains(Uuid{})) {
            return {ProductError::InvalidData, "product group requires filesystem and owner"};
        }
        FileLockPtr lock;
        if (const auto status = fs->AcquireFileLock(".groups/publish.lock", lock); !status) {
            return Convert(status);
        }
        ProductGroupTransaction next;
        next.filesystem = fs;
        next.owner      = owner;
        next.generation = Uuid::Create();
        next.expectedMembers.assign(members.begin(), members.end());
        if (const auto status = WritePointer(fs, owner, "request.bin", next.generation, next.generation.data); !status) {
            return status;
        }
        out = std::move(next);
        return {};
    }

    ProductStatus AssetProductGroupStore::Stage(ProductGroupTransaction &transaction, const Uuid &member, std::span<const uint8_t> bytes) const
    {
        if (transaction.filesystem.Get() != fs.Get() || !transaction.generation || transaction.committed || transaction.failed || !member ||
            bytes.empty() || transaction.members.size() >= MAX_MEMBERS ||
            std::find(transaction.expectedMembers.begin(), transaction.expectedMembers.end(), member) == transaction.expectedMembers.end() ||
            std::any_of(transaction.members.begin(), transaction.members.end(), [&member](const auto &entry) { return entry.uuid == member; })) {
            return {ProductError::InvalidData, "invalid product group staging request"};
        }
        const ProductGroupMember   record{member, bytes.size(), Hash(bytes)};
        const ProductGroupSnapshot snapshot{transaction.owner, transaction.generation};
        const auto                 path = MemberPath(snapshot, member);
        if (const auto status = fs->WriteFileExclusive(path, bytes); !status) {
            transaction.failed = true;
            return Convert(status);
        }
        if (const auto status = Verify(fs->OpenFile(path), record); !status) {
            transaction.failed = true;
            return status;
        }
        transaction.members.push_back(record);
        return {};
    }

    ProductStatus AssetProductGroupStore::Pin(const Uuid &owner, ProductGroupSnapshotPtr &out) const
    {
        if (!fs || !owner) {
            return {ProductError::InvalidData, "invalid product group owner"};
        }
        std::vector<uint8_t> bytes;
        if (const auto status = ReadMetadata(fs, GroupPath(owner, "current.bin"), bytes); !status) {
            return status;
        }
        IMemoryArchive     input(bytes.data(), bytes.size());
        BinaryInputArchive archive(input);
        uint32_t           magic = 0, version = 0, count = 0;
        auto               snapshot = std::make_shared<ProductGroupSnapshot>();
        archive.LoadValue(magic);
        archive.LoadValue(version);
        archive.LoadValue(snapshot->owner);
        archive.LoadValue(snapshot->generation);
        archive.LoadValue(snapshot->revision);
        archive.LoadValue(count);
        if (!archive.IsValid() || magic != GROUP_MAGIC || version != GROUP_VERSION || snapshot->owner != owner || !snapshot->generation ||
            snapshot->revision == 0 || count == 0 || count > MAX_MEMBERS || bytes.size() != 52ULL + 32ULL * count) {
            return {ProductError::InvalidData, "invalid product group manifest; recook required"};
        }
        std::unordered_set<Uuid> identities;
        snapshot->members.resize(count);
        for (auto &member : snapshot->members) {
            archive.LoadValue(member.uuid);
            archive.LoadValue(member.size);
            archive.LoadValue(member.checksum);
            if (!member.uuid || member.size == 0 || !identities.insert(member.uuid).second) {
                return {ProductError::InvalidData, "invalid or duplicate product group member"};
            }
        }
        if (!archive.IsValid()) {
            return {ProductError::InvalidData, "truncated product group manifest"};
        }
        out = std::move(snapshot);
        return {};
    }

    ProductStatus AssetProductGroupStore::Commit(ProductGroupTransaction &transaction, ProductGroupSnapshotPtr &out) const
    {
        if (transaction.filesystem.Get() != fs.Get() || !transaction.generation || transaction.committed || transaction.failed ||
            transaction.members.empty() || transaction.members.size() != transaction.expectedMembers.size()) {
            return {ProductError::InvalidData, "product group is not ready to commit"};
        }
        FileLockPtr lock;
        if (const auto status = fs->AcquireFileLock(".groups/publish.lock", lock); !status) {
            return Convert(status);
        }
        Uuid latest;
        if (const auto status = ReadUuid(fs, GroupPath(transaction.owner, "request.bin"), latest); !status) {
            return status;
        }
        if (latest != transaction.generation) {
            return {ProductError::StaleRequest, "a newer cook request superseded this group"};
        }
        ProductGroupSnapshotPtr previous;
        if (const auto status = Pin(transaction.owner, previous); !status && status.error != ProductError::NotFound) {
            return status;
        }
        if (previous && previous->revision == UINT64_MAX) {
            return {ProductError::InvalidData, "product revision exhausted"};
        }
        auto snapshot = std::make_shared<ProductGroupSnapshot>(
            ProductGroupSnapshot{transaction.owner, transaction.generation, previous ? previous->revision + 1 : 1, transaction.members});
        for (const auto &member : snapshot->members) {
            if (const auto status = Verify(fs->OpenFile(MemberPath(*snapshot, member.uuid)), member); !status) {
                return status;
            }
            Uuid       owner;
            bool       legacy = false;
            const auto status = ReadOwner(fs, member.uuid, owner, legacy);
            if (!status && status.error != ProductError::NotFound) {
                return status;
            }
            if (status && owner != transaction.owner) {
                return {ProductError::OwnerConflict, "product UUID is already owned by another source"};
            }
        }
        // Ownership is registered before publication. Readers still require a committed member.
        for (const auto &member : snapshot->members) {
            if (!HasOwnerRecord(member.uuid)) {
                const auto          temporary = FilePath(OwnerPath(member.uuid).GetStr() + "." + transaction.generation.ToString() + ".tmp");
                OMemoryArchive      ownerBytes;
                BinaryOutputArchive ownerArchive(ownerBytes);
                ownerArchive.SaveValue(snapshot->owner);
                ownerArchive.SaveValue(fs->FileExist(LegacyPath(member.uuid)) ? 1U : 0U);
                if (!ownerArchive.IsValid()) {
                    return {ProductError::IOError, "product ownership encoding failed"};
                }
                if (const auto status = fs->WriteFileExclusive(temporary, {reinterpret_cast<const uint8_t *>(ownerBytes.Data()), ownerBytes.Size()});
                    !status) {
                    return Convert(status);
                }
                if (const auto status = fs->ReplaceFileAtomic(temporary, OwnerPath(member.uuid)); !status) {
                    return Convert(status);
                }
            }
        }
        OMemoryArchive      output;
        BinaryOutputArchive archive(output);
        archive.SaveValue(GROUP_MAGIC);
        archive.SaveValue(GROUP_VERSION);
        archive.SaveValue(snapshot->owner);
        archive.SaveValue(snapshot->generation);
        archive.SaveValue(snapshot->revision);
        archive.SaveValue(static_cast<uint32_t>(snapshot->members.size()));
        for (const auto &member : snapshot->members) {
            archive.SaveValue(member.uuid);
            archive.SaveValue(member.size);
            archive.SaveValue(member.checksum);
        }
        if (!archive.IsValid()) {
            return {ProductError::IOError, "product manifest encoding failed"};
        }
        if (const auto status = WritePointer(fs, transaction.owner, "current.bin", transaction.generation,
                                             {reinterpret_cast<const uint8_t *>(output.Data()), output.Size()});
            !status) {
            return status;
        }
        transaction.committed = true;
        out                   = std::move(snapshot);
        return {};
    }

    bool AssetProductGroupStore::HasOwnerRecord(const Uuid &member) const
    {
        return fs && fs->FileExist(OwnerPath(member));
    }

    bool AssetProductGroupStore::HasLegacyFallback(const Uuid &member) const
    {
        if (!fs || !member) {
            return false;
        }
        Uuid owner;
        bool legacy = false;
        if (!ReadOwner(fs, member, owner, legacy) || !legacy) {
            return false;
        }
        ProductGroupSnapshotPtr snapshot;
        return Pin(owner, snapshot).error == ProductError::NotFound && fs->FileExist(LegacyPath(member));
    }

    ProductStatus AssetProductGroupStore::Resolve(const Uuid &member, ProductGroupSnapshotPtr &out) const
    {
        if (!fs || !member) {
            return {ProductError::InvalidData, "invalid product member"};
        }
        Uuid owner;
        bool legacy = false;
        if (const auto status = ReadOwner(fs, member, owner, legacy); !status) {
            return status;
        }
        ProductGroupSnapshotPtr snapshot;
        if (const auto status = Pin(owner, snapshot); !status) {
            return status;
        }
        if (std::none_of(snapshot->members.begin(), snapshot->members.end(), [&member](const auto &entry) { return entry.uuid == member; })) {
            return {ProductError::NotFound, "product member was removed from its source group"};
        }
        out = std::move(snapshot);
        return {};
    }

    ProductStatus AssetProductGroupStore::Open(const ProductGroupSnapshot &snapshot, const Uuid &member, FilePtr &out) const
    {
        if (!fs || !snapshot.owner || !snapshot.generation || snapshot.revision == 0) {
            return {ProductError::InvalidData, "invalid pinned product group"};
        }
        const auto entry =
            std::find_if(snapshot.members.begin(), snapshot.members.end(), [&member](const auto &value) { return value.uuid == member; });
        if (entry == snapshot.members.end()) {
            return {ProductError::NotFound, "member does not belong to pinned product group"};
        }
        const auto file = fs->OpenFile(MemberPath(snapshot, member));
        if (const auto status = Verify(file, *entry); !status) {
            return status;
        }
        out = file;
        return {};
    }
} // namespace sky
