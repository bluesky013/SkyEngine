//
// ShaderCacheCodec: binary (de)serialization for the shader cache index/blob.
//

#include <aurora/shader/ShaderCacheStore.h>

#include <cstring>

namespace sky::aurora {

    namespace {

        // ---- little-endian byte writer/reader ----

        struct Writer {
            std::vector<uint8_t> buf;

            void U8(uint8_t v)
            {
                buf.push_back(v);
            }
            void U32(uint32_t v)
            {
                for (int i = 0; i < 4; ++i) {
                    buf.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFFu));
                }
            }
            void U64(uint64_t v)
            {
                for (int i = 0; i < 8; ++i) {
                    buf.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFFu));
                }
            }
            void Bytes(const void *data, size_t size)
            {
                const auto *p = static_cast<const uint8_t *>(data);
                buf.insert(buf.end(), p, p + size);
            }
            void Str(const std::string &s)
            {
                U32(static_cast<uint32_t>(s.size()));
                Bytes(s.data(), s.size());
            }
        };

        struct Reader {
            const uint8_t *p   = nullptr;
            size_t         n   = 0;
            size_t         pos = 0;
            bool           ok  = true;

            bool U8(uint8_t &v)
            {
                if (!ok || pos + 1 > n) {
                    ok = false;
                    return false;
                }
                v = p[pos++];
                return true;
            }
            bool U32(uint32_t &v)
            {
                if (!ok || pos + 4 > n) {
                    ok = false;
                    return false;
                }
                v = 0;
                for (int i = 0; i < 4; ++i) {
                    v |= static_cast<uint32_t>(p[pos++]) << (i * 8);
                }
                return true;
            }
            bool U64(uint64_t &v)
            {
                if (!ok || pos + 8 > n) {
                    ok = false;
                    return false;
                }
                v = 0;
                for (int i = 0; i < 8; ++i) {
                    v |= static_cast<uint64_t>(p[pos++]) << (i * 8);
                }
                return true;
            }
            bool Bytes(void *out, size_t size)
            {
                if (!ok || pos + size > n) {
                    ok = false;
                    return false;
                }
                std::memcpy(out, p + pos, size);
                pos += size;
                return true;
            }
            bool Str(std::string &s)
            {
                uint32_t size = 0;
                if (!U32(size) || !ok || pos + size > n) {
                    ok = false;
                    return false;
                }
                s.assign(reinterpret_cast<const char *>(p + pos), size);
                pos += size;
                return true;
            }
        };

        void WriteSchema(Writer &w, const ShaderVariantSchema &schema)
        {
            w.U32(static_cast<uint32_t>(schema.sources.size()));
            for (const auto &s : schema.sources) {
                w.Str(std::string(s.name.GetStr()));
                w.U32(s.bitOffset);
                w.U32(s.bitWidth);
            }
            w.U32(static_cast<uint32_t>(schema.entries.size()));
            for (const auto &e : schema.entries) {
                w.Str(std::string(e.key.GetStr()));
                w.Str(std::string(e.source.GetStr()));
                w.U32(e.bitOffset);
                w.U32(e.bitWidth);
                w.U32(e.defaultValue);
                w.U8(e.isSpec ? 1 : 0);
                w.U32(e.specId);
            }
            w.U32(schema.totalBits);
        }

        bool ReadSchema(Reader &r, ShaderVariantSchema &schema)
        {
            uint32_t count = 0;
            if (!r.U32(count)) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                std::string name;
                uint32_t    offset = 0;
                uint32_t    width  = 0;
                if (!r.Str(name) || !r.U32(offset) || !r.U32(width)) {
                    return false;
                }
                schema.sources.push_back({Name(name.c_str()), static_cast<uint16_t>(offset), static_cast<uint8_t>(width)});
            }
            if (!r.U32(count)) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                std::string                key;
                std::string                source;
                ShaderVariantSchema::Entry e{};
                uint32_t                   offset = 0;
                uint32_t                   width  = 0;
                uint8_t                    isSpec = 0;
                if (!r.Str(key) || !r.Str(source) || !r.U32(offset) || !r.U32(width) || !r.U32(e.defaultValue) || !r.U8(isSpec) || !r.U32(e.specId)) {
                    return false;
                }
                e.key       = Name(key.c_str());
                e.source    = Name(source.c_str());
                e.bitOffset = static_cast<uint16_t>(offset);
                e.bitWidth  = static_cast<uint8_t>(width);
                e.isSpec    = isSpec != 0;
                schema.entries.push_back(e);
            }
            return r.U32(schema.totalBits);
        }

        void WriteReflection(Writer &w, const ShaderReflection &refl)
        {
            w.U32(static_cast<uint32_t>(refl.resources.size()));
            for (const auto &res : refl.resources) {
                w.Str(res.name);
                w.U32(static_cast<uint32_t>(res.type));
                w.U32(res.set);
                w.U32(res.binding);
                w.U32(res.count);
                w.U32(static_cast<uint32_t>(res.texture.dimension));
                w.U32(static_cast<uint32_t>(res.texture.scalarType));
                w.U32(res.texture.components);
                w.U8(res.texture.arrayed);
                w.U8(res.texture.multisampled);
                w.U32(static_cast<uint32_t>(res.sampler));
                w.U8(res.descriptorArray);
            }
            w.U32(static_cast<uint32_t>(refl.blocks.size()));
            for (const auto &blk : refl.blocks) {
                w.Str(blk.name);
                w.Str(blk.structName);
                w.U32(blk.set);
                w.U32(blk.binding);
                w.U32(blk.size);
                w.U32(static_cast<uint32_t>(blk.members.size()));
                for (const auto &m : blk.members) {
                    w.Str(m.name);
                    w.U32(m.offset);
                    w.U32(m.size);
                    w.U32(static_cast<uint32_t>(m.scalarType));
                    w.U32(static_cast<uint32_t>(m.kind));
                    w.U32(m.rows);
                    w.U32(m.cols);
                }
            }
            w.U32(static_cast<uint32_t>(refl.pushConstants.size()));
            for (const auto &pc : refl.pushConstants) {
                w.U32(static_cast<uint32_t>(pc.stageFlags.value));
                w.U32(pc.offset);
                w.U32(pc.size);
            }
            w.U32(refl.threadGroupSize[0]);
            w.U32(refl.threadGroupSize[1]);
            w.U32(refl.threadGroupSize[2]);
        }

        bool ReadReflection(Reader &r, ShaderReflection &refl)
        {
            uint32_t count = 0;
            if (!r.U32(count)) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                ShaderResource res;
                uint32_t       type = 0;
                if (!r.Str(res.name) || !r.U32(type) || !r.U32(res.set) || !r.U32(res.binding) || !r.U32(res.count)) {
                    return false;
                }
                uint32_t dimension = 0, scalar = 0, sampler = 0;
                uint8_t  arrayed = 0, multisampled = 0, descriptorArray = 0;
                if (!r.U32(dimension) || !r.U32(scalar) || !r.U32(res.texture.components) || !r.U8(arrayed) || !r.U8(multisampled) ||
                    !r.U32(sampler) || !r.U8(descriptorArray) || type > static_cast<uint32_t>(ShaderResourceType::STORAGE_BUFFER_DYNAMIC) ||
                    dimension > static_cast<uint32_t>(ShaderTextureDimension::BUFFER) || scalar > static_cast<uint32_t>(ShaderScalarType::BOOL) ||
                    sampler > static_cast<uint32_t>(ShaderSamplerType::COMPARISON) || arrayed > 1 || multisampled > 1 || descriptorArray > 1)
                    return false;
                res.texture.dimension    = static_cast<ShaderTextureDimension>(dimension);
                res.texture.scalarType   = static_cast<ShaderScalarType>(scalar);
                res.texture.arrayed      = arrayed != 0;
                res.texture.multisampled = multisampled != 0;
                res.sampler              = static_cast<ShaderSamplerType>(sampler);
                res.descriptorArray      = descriptorArray != 0;
                res.type                 = static_cast<ShaderResourceType>(type);
                refl.resources.push_back(std::move(res));
            }
            if (!r.U32(count)) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                ShaderBlockLayout blk;
                if (!r.Str(blk.name) || !r.Str(blk.structName) || !r.U32(blk.set) || !r.U32(blk.binding) || !r.U32(blk.size)) {
                    return false;
                }
                uint32_t mcount = 0;
                if (!r.U32(mcount)) {
                    return false;
                }
                for (uint32_t j = 0; j < mcount; ++j) {
                    ShaderBlockMember m;
                    uint32_t          scalar = 0;
                    uint32_t          kind   = 0;
                    if (!r.Str(m.name) || !r.U32(m.offset) || !r.U32(m.size) || !r.U32(scalar) || !r.U32(kind) || !r.U32(m.rows) || !r.U32(m.cols)) {
                        return false;
                    }
                    m.scalarType = static_cast<ShaderScalarType>(scalar);
                    m.kind       = static_cast<ShaderTypeKind>(kind);
                    blk.members.push_back(m);
                }
                refl.blocks.push_back(std::move(blk));
            }
            if (!r.U32(count)) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                PushConstantRange pc{};
                uint32_t          stageFlags = 0;
                if (!r.U32(stageFlags) || !r.U32(pc.offset) || !r.U32(pc.size)) {
                    return false;
                }
                pc.stageFlags = ShaderStageFlags(stageFlags);
                refl.pushConstants.push_back(pc);
            }
            return r.U32(refl.threadGroupSize[0]) && r.U32(refl.threadGroupSize[1]) && r.U32(refl.threadGroupSize[2]);
        }

    } // namespace

    std::vector<uint8_t> EncodeIndex(const ShaderCacheIndex &index)
    {
        Writer w;
        w.U32(index.formatVersion);
        w.U64(index.toolchainFp);
        w.U64(index.layoutFp);
        w.U32(static_cast<uint32_t>(index.paths.size()));
        for (const auto &p : index.paths) {
            w.Str(p.first);
            w.U64(p.second.schemaFp);
            WriteSchema(w, p.second.schema);
            w.U32(static_cast<uint32_t>(p.second.artifacts.size()));
            for (const auto &a : p.second.artifacts) {
                w.U64(a.first);
                w.U64(a.second.compileHash);
                w.U64(a.second.sourceHash);
                w.U32(static_cast<uint32_t>(a.second.sourceDeps.size()));
                for (const auto &d : a.second.sourceDeps) {
                    w.Str(d);
                }
            }
        }
        return w.buf;
    }

    bool DecodeIndex(const std::vector<uint8_t> &bytes, ShaderCacheIndex &out)
    {
        Reader           r{bytes.data(), bytes.size()};
        ShaderCacheIndex index;
        if (!r.U32(index.formatVersion)) {
            return false;
        }
        if (index.formatVersion != ShaderCacheIndex::kFormatVersion) {
            return false;
        }
        if (!r.U64(index.toolchainFp) || !r.U64(index.layoutFp)) {
            return false;
        }

        uint32_t pathCount = 0;
        if (!r.U32(pathCount)) {
            return false;
        }
        index.paths.reserve(pathCount);
        for (uint32_t i = 0; i < pathCount; ++i) {
            std::string          path;
            ShaderCachePathEntry entry;
            if (!r.Str(path) || !r.U64(entry.schemaFp) || !ReadSchema(r, entry.schema)) {
                return false;
            }

            uint32_t artCount = 0;
            if (!r.U32(artCount)) {
                return false;
            }
            for (uint32_t j = 0; j < artCount; ++j) {
                uint64_t            key = 0;
                ShaderCacheArtifact art;
                if (!r.U64(key) || !r.U64(art.compileHash) || !r.U64(art.sourceHash)) {
                    return false;
                }
                uint32_t depCount = 0;
                if (!r.U32(depCount)) {
                    return false;
                }
                for (uint32_t k = 0; k < depCount; ++k) {
                    std::string dep;
                    if (!r.Str(dep)) {
                        return false;
                    }
                    art.sourceDeps.push_back(std::move(dep));
                }
                entry.artifacts.emplace_back(key, std::move(art));
            }
            index.paths.emplace_back(std::move(path), std::move(entry));
        }
        out = std::move(index);
        return true;
    }

    std::vector<uint8_t> EncodeBlob(const ShaderCacheBlob &blob)
    {
        Writer w;
        w.U32(0x53484232);
        w.U32(ShaderCacheIndex::kFormatVersion);
        w.U32(blob.target);
        w.U64(blob.sourceHash);
        w.U64(blob.layoutFp);
        w.U64(blob.schemaFp);
        w.U32(static_cast<uint32_t>(blob.data.size()));
        w.Bytes(blob.data.data(), blob.data.size() * sizeof(uint32_t));
        WriteReflection(w, blob.reflection);
        return w.buf;
    }

    bool DecodeBlob(const std::vector<uint8_t> &bytes, ShaderCacheBlob &out)
    {
        Reader          r{bytes.data(), bytes.size()};
        ShaderCacheBlob blob;
        uint32_t        magic = 0, version = 0;
        if (!r.U32(magic) || magic != 0x53484232 || !r.U32(version) || version != ShaderCacheIndex::kFormatVersion)
            return false;
        if (!r.U32(blob.target) || !r.U64(blob.sourceHash) || !r.U64(blob.layoutFp) || !r.U64(blob.schemaFp)) {
            return false;
        }
        uint32_t dataCount = 0;
        if (!r.U32(dataCount)) {
            return false;
        }
        if (dataCount > (r.n - r.pos) / sizeof(uint32_t))
            return false;
        blob.data.resize(dataCount);
        if (dataCount > 0 && !r.Bytes(blob.data.data(), dataCount * sizeof(uint32_t))) {
            return false;
        }
        if (!ReadReflection(r, blob.reflection) || r.pos != r.n) {
            return false;
        }
        out = std::move(blob);
        return true;
    }

} // namespace sky::aurora
