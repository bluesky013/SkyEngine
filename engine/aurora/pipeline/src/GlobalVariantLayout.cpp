//
// GlobalVariantLayout implementation: load + parse + validate the data-driven
// pipeline variant layout.
//

#include <aurora/pipeline/GlobalVariantLayout.h>

#include <aurora/shader/ShaderFileSystem.h>
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
        return true;
    }

    bool GlobalVariantLayout::Load(ShaderFileSystem &fs, const std::string &path,
                                   std::string *error)
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

    std::string GlobalVariantLayout::ToString(const ShaderVariantKey &key,
                                              const ShaderVariantSchema &perShaderSchema) const
    {
        std::string s = key.ToString(schema);
        const std::string per = key.ToString(perShaderSchema, reservedBits);
        if (!s.empty() && !per.empty()) {
            s += ", ";
        }
        s += per;
        return s;
    }

} // namespace sky::aurora
