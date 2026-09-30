//
// GlobalVariantLayout implementation: load + parse + validate the data-driven
// pipeline variant layout.
//

#include <aurora/pipeline/GlobalVariantLayout.h>

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderHash.h>
#include <aurora/shader/gen/ShaderVariantGen.h>

namespace sky::aurora {

    bool GlobalVariantLayout::Init(std::string *error)
    {
        if (schema.totalBits > reservedBits) {
            if (error != nullptr) {
                *error = "pipeline variant keys exceed reserved bits";
            }
            return false;
        }

        const uint32_t total = static_cast<uint32_t>(reservedBits) + kVertexSemanticBits + schema.totalBits;
        if (total > ShaderVariantKey::kMaxBits) {
            if (error != nullptr) {
                *error = "pipeline + vertex + shader variant bits exceed 128";
            }
            return false;
        }

        uint64_t fp = kFnv1aBasis;
        fp          = HashMixU64(fp, reservedBits);
        for (const auto &source : schema.sources) {
            fp = HashMixU64(fp, source.name.GetHandle());
            fp = HashMixU64(fp, source.bitOffset);
            fp = HashMixU64(fp, source.bitWidth);
        }
        for (const auto &entry : schema.entries) {
            fp = HashMixU64(fp, entry.key.GetHandle());
            fp = HashMixU64(fp, entry.source.GetHandle());
            fp = HashMixU64(fp, entry.bitOffset);
            fp = HashMixU64(fp, entry.bitWidth);
            fp = HashMixU64(fp, entry.defaultValue);
        }
        fingerprint = fp;
        return true;
    }

    bool GlobalVariantLayout::Load(ShaderFileSystem &fs, const std::string &path, std::string *error)
    {
        std::string source;
        if (!fs.ReadFile(path, source)) {
            if (error != nullptr) {
                *error = "failed to read " + path;
            }
            return false;
        }

        std::string parseError;
        if (!ShaderVariantGen::Parse(source, schema, reservedBits, parseError)) {
            if (error != nullptr) {
                *error = parseError;
            }
            return false;
        }
        return Init(error);
    }

    const ShaderVariantSchema::Entry *GlobalVariantLayout::Find(Name key) const
    {
        return schema.FindEntry(key);
    }

    std::string GlobalVariantLayout::ToString(const ShaderVariantKey &key) const
    {
        return key.ToString(schema);
    }

    std::string GlobalVariantLayout::ToString(const ShaderVariantKey &key, const ShaderVariantSchema &perShaderSchema) const
    {
        std::string       s   = key.ToString(schema);
        const std::string per = key.ToString(perShaderSchema, reservedBits);
        if (!s.empty() && !per.empty()) {
            s += ", ";
        }
        s += per;
        return s;
    }

} // namespace sky::aurora
