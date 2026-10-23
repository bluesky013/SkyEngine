#include <aurora/shader/ShaderCacheStore.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace sky::aurora {
    namespace {
        constexpr uint32_t kBlobMagic    = 0x53484136;
        constexpr uint32_t kMaximumCount = 1u << 20;
        struct Writer {
            std::vector<uint8_t> bytes;
            void                 U32(uint32_t value)
            {
                for (uint32_t i = 0; i < 4; ++i) {
                    bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
                }
            }
            void U64(uint64_t value)
            {
                for (uint32_t i = 0; i < 8; ++i) {
                    bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
                }
            }
            void Str(std::string_view value)
            {
                U32(static_cast<uint32_t>(value.size()));
                bytes.insert(bytes.end(), value.begin(), value.end());
            }
            template <class T>
            void Enum(T value)
            {
                U32(static_cast<uint32_t>(value));
            }
        };
        struct Reader {
            const std::vector<uint8_t> &bytes;
            size_t                      position = 0;
            bool                        U32(uint32_t &value)
            {
                if (bytes.size() - position < 4) {
                    return false;
                }
                value = 0;
                for (uint32_t i = 0; i < 4; ++i) {
                    value |= uint32_t(bytes[position++]) << (i * 8);
                }
                return true;
            }
            bool U64(uint64_t &value)
            {
                if (bytes.size() - position < 8) {
                    return false;
                }
                value = 0;
                for (uint32_t i = 0; i < 8; ++i) {
                    value |= uint64_t(bytes[position++]) << (i * 8);
                }
                return true;
            }
            bool Count(uint32_t &count, size_t minimum = 4)
            {
                return U32(count) && count <= kMaximumCount && count <= (bytes.size() - position) / minimum;
            }
            bool Str(std::string &value)
            {
                uint32_t size = 0;
                if (!U32(size) || size > (1u << 24) || size > bytes.size() - position) {
                    return false;
                }
                value.assign(reinterpret_cast<const char *>(bytes.data() + position), size);
                position += size;
                return value.find('\0') == std::string::npos;
            }
            template <class T>
            bool Enum(T &value, T maximum)
            {
                uint32_t number = 0;
                if (!U32(number) || number > static_cast<uint32_t>(maximum)) {
                    return false;
                }
                value = static_cast<T>(number);
                return true;
            }
            bool Bool(bool &value)
            {
                uint32_t number = 0;
                if (!U32(number) || number > 1) {
                    return false;
                }
                value = number != 0;
                return true;
            }
        };
        template <class T, class F>
        void WriteList(Writer &writer, const std::vector<T> &list, F write)
        {
            writer.U32(static_cast<uint32_t>(list.size()));
            for (const auto &item : list) {
                write(writer, item);
            }
        }
        template <class T, class F>
        bool ReadList(Reader &reader, std::vector<T> &list, F read)
        {
            uint32_t count = 0;
            if (!reader.Count(count)) {
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                T item{};
                if (!read(reader, item)) {
                    return false;
                }
                list.push_back(std::move(item));
            }
            return true;
        }
        void WriteString(Writer &w, const std::string &s)
        {
            w.Str(s);
        }
        bool ReadString(Reader &r, std::string &s)
        {
            return r.Str(s);
        }
        void WriteVariant(Writer &w, const ShaderVariantValue &v)
        {
            w.Str(v.name);
            w.U32(v.value);
        }
        bool ReadVariant(Reader &r, ShaderVariantValue &v)
        {
            return r.Str(v.name) && r.U32(v.value);
        }
        void WriteEnvironment(Writer &w, const ShaderTargetEnvironment &v)
        {
            w.Enum(v.target);
            w.Enum(v.platform);
            w.Enum(v.codeFormat);
            w.Str(v.profile);
            w.U32(v.minimumVersion);
            w.U64(v.requiredCapabilities);
            w.Enum(v.matrixLayout);
            WriteList(w, v.options, WriteString);
        }
        bool ReadEnvironment(Reader &r, ShaderTargetEnvironment &v)
        {
            if (!r.Enum(v.target, ShaderTarget::DXIL) || !r.Enum(v.platform, ShaderPlatform::LINUX) ||
                !r.Enum(v.codeFormat, ShaderCodeFormat::METALLIB) || !r.Str(v.profile) || !r.U32(v.minimumVersion) ||
                !r.U64(v.requiredCapabilities) || !r.Enum(v.matrixLayout, ShaderMatrixLayout::ROW_MAJOR) || !ReadList(r, v.options, ReadString)) {
                return false;
            }
            auto        normalized = v;
            std::string error;
            return NormalizeShaderEnvironment(normalized, error) && normalized == v;
        }
        void WriteProducer(Writer &w, const ShaderProducerIdentity &v)
        {
            w.Str(v.compiler);
            w.Str(v.version);
            w.U32(v.adapterVersion);
            w.U32(v.reflectionVersion);
            w.U64(v.configuration);
        }
        bool ReadProducer(Reader &r, ShaderProducerIdentity &v)
        {
            return r.Str(v.compiler) && r.Str(v.version) && r.U32(v.adapterVersion) && r.U32(v.reflectionVersion) && r.U64(v.configuration);
        }
        void WriteIdentity(Writer &w, const ShaderStageIdentity &v)
        {
            w.Str(v.sourcePath);
            w.Str(v.sourceEntry);
            w.Str(v.nativeEntry);
            w.Enum(v.stage);
            w.U64(v.sourceHash);
            w.U64(v.variantHash);
            w.U64(v.layoutFp);
            w.U64(v.schemaFp);
            w.U64(v.interfaceFp);
            WriteList(w, v.variants, WriteVariant);
            WriteEnvironment(w, v.environment);
            WriteProducer(w, v.producer);
        }
        bool ReadStage(Reader &r, ShaderStageFlagBit &stage)
        {
            uint32_t value = 0;
            if (!r.U32(value)) {
                return false;
            }
            switch (static_cast<ShaderStageFlagBit>(value)) {
            case ShaderStageFlagBit::VS:
            case ShaderStageFlagBit::FS:
            case ShaderStageFlagBit::CS:
            case ShaderStageFlagBit::TAS:
            case ShaderStageFlagBit::MS: stage = static_cast<ShaderStageFlagBit>(value); return true;
            default: return false;
            }
        }
        bool ReadIdentity(Reader &r, ShaderStageIdentity &v)
        {
            return r.Str(v.sourcePath) && r.Str(v.sourceEntry) && r.Str(v.nativeEntry) && ReadStage(r, v.stage) && r.U64(v.sourceHash) &&
                   r.U64(v.variantHash) && r.U64(v.layoutFp) && r.U64(v.schemaFp) && r.U64(v.interfaceFp) && ReadList(r, v.variants, ReadVariant) &&
                   ReadEnvironment(r, v.environment) && ReadProducer(r, v.producer);
        }
        void WriteKey(Writer &w, const ShaderCacheKey &v)
        {
            w.U64(v.sourceHash);
            w.U64(v.variantHash);
            w.U64(v.layoutFp);
            w.U64(v.schemaFp);
            w.U64(v.toolchainFp);
            w.U64(v.entryHash);
            w.U32(v.target);
            w.U32(v.stage);
            w.Str(v.sourcePath);
            w.Str(v.sourceEntry);
            WriteList(w, v.variants, WriteVariant);
            WriteEnvironment(w, v.environment);
            WriteProducer(w, v.producer);
        }
        bool ReadKey(Reader &r, ShaderCacheKey &v)
        {
            return r.U64(v.sourceHash) && r.U64(v.variantHash) && r.U64(v.layoutFp) && r.U64(v.schemaFp) && r.U64(v.toolchainFp) &&
                   r.U64(v.entryHash) && r.U32(v.target) && r.U32(v.stage) && r.Str(v.sourcePath) && r.Str(v.sourceEntry) &&
                   ReadList(r, v.variants, ReadVariant) && ReadEnvironment(r, v.environment) && ReadProducer(r, v.producer);
        }
        void WriteSchema(Writer &w, const ShaderVariantSchema &v)
        {
            WriteList(w, v.sources, [](Writer &w, const ShaderVariantSchema::Source &s) {
                w.Str(s.name.GetStr());
                w.U32(s.bitOffset);
                w.U32(s.bitWidth);
            });
            WriteList(w, v.entries, [](Writer &w, const ShaderVariantSchema::Entry &e) {
                w.Str(e.key.GetStr());
                w.Str(e.source.GetStr());
                w.U32(e.bitOffset);
                w.U32(e.bitWidth);
                w.U32(e.defaultValue);
                w.U32(e.isSpec);
                w.U32(e.specId);
            });
            w.U32(v.totalBits);
        }
        bool ReadSchema(Reader &r, ShaderVariantSchema &v)
        {
            if (!ReadList(r, v.sources,
                          [](Reader &r, ShaderVariantSchema::Source &s) {
                              std::string name;
                              uint32_t    offset = 0, width = 0;
                              if (!r.Str(name) || !r.U32(offset) || !r.U32(width) || offset > UINT16_MAX || width > UINT8_MAX) {
                                  return false;
                              }
                              s = {Name(name.c_str()), static_cast<uint16_t>(offset), static_cast<uint8_t>(width)};
                              return true;
                          }) ||
                !ReadList(r, v.entries,
                          [](Reader &r, ShaderVariantSchema::Entry &e) {
                              std::string key, source;
                              uint32_t    offset = 0, width = 0;
                              if (!r.Str(key) || !r.Str(source) || !r.U32(offset) || !r.U32(width) || !r.U32(e.defaultValue) || !r.Bool(e.isSpec) ||
                                  !r.U32(e.specId) || offset > UINT16_MAX || width > UINT8_MAX) {
                                  return false;
                              }
                              e.key       = Name(key.c_str());
                              e.source    = Name(source.c_str());
                              e.bitOffset = static_cast<uint16_t>(offset);
                              e.bitWidth  = static_cast<uint8_t>(width);
                              return true;
                          }) ||
                !r.U32(v.totalBits)) {
                return false;
            }
            return v.Validate();
        }
        void WriteResource(Writer &w, const ShaderResource &v)
        {
            w.Str(v.name);
            w.Enum(v.type);
            w.U32(v.set);
            w.U32(v.binding);
            w.U32(v.count);
            w.Enum(v.texture.dimension);
            w.Enum(v.texture.scalarType);
            w.U32(v.texture.components);
            w.U32(v.texture.arrayed);
            w.U32(v.texture.multisampled);
            w.Enum(v.sampler);
            w.U32(v.descriptorArray);
            w.Enum(v.access);
            w.Enum(v.bufferKind);
            w.U32(v.elementStride);
            w.U32(v.stages.value);
            w.Str(v.declarationOwner);
        }
        bool ReadResource(Reader &r, ShaderResource &v)
        {
            uint32_t stages = 0;
            if (!r.Str(v.name) || !r.Enum(v.type, ShaderResourceType::STORAGE_BUFFER_DYNAMIC) || !r.U32(v.set) || !r.U32(v.binding) ||
                !r.U32(v.count) || !r.Enum(v.texture.dimension, ShaderTextureDimension::BUFFER) ||
                !r.Enum(v.texture.scalarType, ShaderScalarType::BOOL) || !r.U32(v.texture.components) || !r.Bool(v.texture.arrayed) ||
                !r.Bool(v.texture.multisampled) || !r.Enum(v.sampler, ShaderSamplerType::COMPARISON) || !r.Bool(v.descriptorArray) ||
                !r.Enum(v.access, ShaderResourceAccess::READ_WRITE) || !r.Enum(v.bufferKind, ShaderBufferKind::STRUCTURED) ||
                !r.U32(v.elementStride) || !r.U32(stages) || !r.Str(v.declarationOwner)) {
                return false;
            }
            v.stages = ShaderStageFlags(stages);
            return true;
        }
        void WriteBlock(Writer &w, const ShaderBlockLayout &v)
        {
            w.Str(v.name);
            w.Str(v.structName);
            w.U32(v.set);
            w.U32(v.binding);
            w.U32(v.size);
            w.U32(v.alignment);
            WriteList(w, v.members, [](Writer &w, const ShaderBlockMember &m) {
                w.Str(m.name);
                w.U32(m.offset);
                w.U32(m.size);
                w.Enum(m.scalarType);
                w.Enum(m.kind);
                w.U32(m.rows);
                w.U32(m.cols);
                w.U32(m.arrayCount);
                w.U32(m.arrayStride);
                w.U32(m.alignment);
                w.U32(m.scalarBytes);
                w.U32(m.matrixStride);
                w.Enum(m.matrixLayout);
            });
        }
        bool ReadBlock(Reader &r, ShaderBlockLayout &v)
        {
            return r.Str(v.name) && r.Str(v.structName) && r.U32(v.set) && r.U32(v.binding) && r.U32(v.size) && r.U32(v.alignment) &&
                   ReadList(r, v.members, [](Reader &r, ShaderBlockMember &m) {
                       return r.Str(m.name) && r.U32(m.offset) && r.U32(m.size) && r.Enum(m.scalarType, ShaderScalarType::BOOL) &&
                              r.Enum(m.kind, ShaderTypeKind::STRUCT) && r.U32(m.rows) && r.U32(m.cols) && r.U32(m.arrayCount) &&
                              r.U32(m.arrayStride) && r.U32(m.alignment) && r.U32(m.scalarBytes) && r.U32(m.matrixStride) &&
                              r.Enum(m.matrixLayout, ShaderMatrixLayout::ROW_MAJOR);
                   });
        }
        void WriteReflection(Writer &w, const ShaderReflection &v)
        {
            w.U32(v.protocolVersion);
            w.Enum(v.kind);
            w.Enum(v.bindingModel);
            w.Enum(v.usage);
            w.U32(v.stages.value);
            w.U32(v.complete);
            WriteList(w, v.resources, WriteResource);
            WriteList(w, v.declarations, WriteResource);
            WriteList(w, v.blocks, WriteBlock);
            WriteList(w, v.declaredBlocks, WriteBlock);
            WriteList(w, v.nativeBindings, [](Writer &w, const ShaderNativeBinding &b) {
                w.U32(b.set);
                w.U32(b.binding);
                w.Enum(b.stage);
                w.U32(b.index);
                w.Enum(b.category);
                w.U32(b.space);
                w.U32(b.count);
            });
            WriteList(w, v.pushConstants, [](Writer &w, const PushConstantRange &p) {
                w.U32(p.stageFlags.value);
                w.U32(p.offset);
                w.U32(p.size);
            });
            for (const auto size : v.threadGroupSize) {
                w.U32(size);
            }
        }
        bool ReadReflection(Reader &r, ShaderReflection &v)
        {
            uint32_t stages = 0;
            if (!r.U32(v.protocolVersion) || !r.Enum(v.kind, ShaderReflectionKind::PROGRAM) ||
                !r.Enum(v.bindingModel, ShaderBindingModel::METAL_DIRECT_ARGUMENTS) || !r.Enum(v.usage, ShaderUsageAnalysis::CONSERVATIVE) ||
                !r.U32(stages) || !r.Bool(v.complete) || !ReadList(r, v.resources, ReadResource) || !ReadList(r, v.declarations, ReadResource) ||
                !ReadList(r, v.blocks, ReadBlock) || !ReadList(r, v.declaredBlocks, ReadBlock) ||
                !ReadList(r, v.nativeBindings,
                          [](Reader &r, ShaderNativeBinding &b) {
                              return r.U32(b.set) && r.U32(b.binding) && ReadStage(r, b.stage) && r.U32(b.index) &&
                                     r.Enum(b.category, ShaderNativeCategory::METAL_SAMPLER) && r.U32(b.space) && r.U32(b.count);
                          }) ||
                !ReadList(r, v.pushConstants, [](Reader &r, PushConstantRange &p) {
                    uint32_t stages = 0;
                    if (!r.U32(stages) || !r.U32(p.offset) || !r.U32(p.size)) {
                        return false;
                    }
                    p.stageFlags = ShaderStageFlags(stages);
                    return true;
                })) {
                return false;
            }
            v.stages = ShaderStageFlags(stages);
            for (auto &size : v.threadGroupSize) {
                if (!r.U32(size)) {
                    return false;
                }
            }
            std::string error;
            return ValidateShaderReflection(v, error);
        }
    } // namespace
    std::vector<uint8_t> EncodeIndex(const ShaderCacheIndex &index)
    {
        Writer w;
        w.U32(index.formatVersion);
        WriteList(w, index.paths, [](Writer &w, const auto &p) {
            w.Str(p.first);
            w.U64(p.second.schemaFp);
            WriteSchema(w, p.second.schema);
            WriteList(w, p.second.artifacts, [](Writer &w, const auto &a) {
                w.U64(a.first);
                w.U64(a.second.compileHash);
                w.U64(a.second.artifactId);
                WriteKey(w, a.second.key);
                WriteIdentity(w, a.second.identity);
                WriteList(w, a.second.sourceDeps, WriteString);
            });
        });
        return std::move(w.bytes);
    }
    bool DecodeIndex(const std::vector<uint8_t> &bytes, ShaderCacheIndex &out)
    {
        Reader           r{bytes};
        ShaderCacheIndex index;
        if (!r.U32(index.formatVersion) || index.formatVersion != ShaderCacheIndex::kFormatVersion ||
            !ReadList(r, index.paths,
                      [](Reader &r, auto &p) {
                          if (!r.Str(p.first) || !r.U64(p.second.schemaFp) || !ReadSchema(r, p.second.schema) ||
                              p.second.schemaFp != HashSchema(p.second.schema)) {
                              return false;
                          }
                          return ReadList(r, p.second.artifacts, [](Reader &r, auto &a) {
                              std::string error;
                              return r.U64(a.first) && r.U64(a.second.compileHash) && r.U64(a.second.artifactId) && ReadKey(r, a.second.key) &&
                                     ReadIdentity(r, a.second.identity) && ReadList(r, a.second.sourceDeps, ReadString) &&
                                     a.first == ArtifactKey(PublishedRequest(a.second.key)) && ValidateCacheRecord(a.second, error);
                          });
                      }) ||
            r.position != bytes.size()) {
            return false;
        }
        std::unordered_set<std::string> paths;
        for (const auto &path : index.paths) {
            if (!paths.insert(path.first).second) {
                return false;
            }
            for (size_t i = 0; i < path.second.artifacts.size(); ++i) {
                const auto &record = path.second.artifacts[i].second;
                if (record.key.sourcePath != path.first || record.key.schemaFp != path.second.schemaFp) {
                    return false;
                }
                for (size_t j = 0; j < i; ++j) {
                    if (PublishedRequest(path.second.artifacts[j].second.key) == PublishedRequest(record.key)) {
                        return false;
                    }
                }
            }
        }
        out = std::move(index);
        return true;
    }
    std::vector<uint8_t> EncodeBlob(const ShaderCacheBlob &blob)
    {
        Writer w;
        w.U32(kBlobMagic);
        w.U32(ShaderCacheIndex::kFormatVersion);
        w.U32(blob.runtimeAbiVersion);
        w.U64(blob.artifactId);
        WriteIdentity(w, blob.identity);
        WriteReflection(w, blob.reflection);
        w.U64(blob.data.size());
        w.bytes.insert(w.bytes.end(), blob.data.begin(), blob.data.end());
        return std::move(w.bytes);
    }
    bool DecodeBlob(const std::vector<uint8_t> &bytes, ShaderCacheBlob &out)
    {
        Reader          r{bytes};
        ShaderCacheBlob blob;
        uint32_t        magic = 0, version = 0;
        uint64_t        size = 0;
        if (!r.U32(magic) || magic != kBlobMagic || !r.U32(version) || version != ShaderCacheIndex::kFormatVersion ||
            !r.U32(blob.runtimeAbiVersion) || !r.U64(blob.artifactId) || !ReadIdentity(r, blob.identity) || !ReadReflection(r, blob.reflection) ||
            !r.U64(size) || size > 64u * 1024u * 1024u || size != bytes.size() - r.position) {
            return false;
        }
        blob.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(r.position), bytes.end());
        std::string error;
        if (!ValidateShaderArtifact(blob, error)) {
            return false;
        }
        out = std::move(blob);
        return true;
    }
} // namespace sky::aurora
