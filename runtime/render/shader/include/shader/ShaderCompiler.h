//
// Created by Zach Lee on 2023/2/18.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <map>
#include <variant>
#include <unordered_map>
#include <core/name/Name.h>
#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>
#include <core/template/ReferenceObject.h>
#include <core/crypto/md5/MD5.h>
#include <core/archive/IArchive.h>

#include <shader/ShaderVariant.h>
#include <rhi/Core.h>

namespace sky {

    struct ShaderIncludeContext {
        std::vector<FilePath> &searchPaths;
        std::set<std::string> visited;
    };

    struct ShaderResource {
        std::string name;
        rhi::DescriptorType type;
        rhi::ShaderStageFlags visibility;
        uint32_t set;
        uint32_t binding;
        uint32_t count;
        uint32_t size;
    };

    struct ShaderVariable {
        std::string name;
        uint32_t set;
        uint32_t binding;
        uint32_t offset;
        uint32_t size;
    };

    struct ShaderStructType {
        std::string name;
        std::vector<ShaderVariable> variables;
    };

    struct VertexStageAttribute {
        std::string semantic;
        uint32_t location;
        uint32_t vecSize;
        rhi::BaseType type;
    };

    struct ShaderReflection {
        std::vector<ShaderResource> resources;
        std::vector<ShaderStructType> types;
        std::vector<VertexStageAttribute> attributes;
    };

    struct ShaderBuildResult {
        std::vector<uint32_t> data;
        ShaderReflection reflection;
        std::string errorInfo;
    };

    enum class ShaderCompileTarget : uint32_t {
        SPIRV,
        MSL,
        DXIL,
        NUM
    };

    class ShaderOption : public RefObject {
    public:
        ShaderOption() = default;
        ~ShaderOption() override = default;

        void SetValue(const std::string &key, const uint8_t &val);
        void CalculateHash();
        uint32_t GetHash() const { return hash; }

        std::map<std::string, uint8_t> values;
    private:
        uint32_t hash = 0;
    };
    using ShaderOptionPtr = CounterPtr<ShaderOption>;

    struct ShaderCompileOption {
        ShaderCompileTarget target;
        ShaderOptionPtr option;
    };

    struct ShaderSourceDesc {
        std::string source;
        std::string entry;
        rhi::ShaderStageFlagBit stage;
    };

    class ShaderCompilerBase {
    public:
        ShaderCompilerBase() = default;
        virtual ~ShaderCompilerBase() = default;

        virtual bool CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result) = 0;
        virtual std::string Disassemble(const std::vector<uint32_t>& binary, ShaderCompileTarget target) const { return {}; }
    };

    class ShaderCompiler : public Singleton<ShaderCompiler> {
    public:
        ShaderCompiler();
        ~ShaderCompiler() override;

        void AddSearchPath(const FilePath &path) { searchPaths.emplace_back(path); }

        FilePath GetShaderPath(const std::string &name) const;
        std::string LoadShader(const std::string &name);

        static void SaveToMemory(IOutputArchive& archive, const ShaderBuildResult& result);

        static MD5 CalculateShaderMD5(const std::string &source);

        // replace shader name, '/', '\\' to '_'
        static std::string ReplaceShadeName(const Name& name);

        // replace shader name with entry and option
        static std::string GetBinaryShaderName(const Name& name, const Name& entry, const ShaderOptionPtr &option);

        // get shader stage by name
        static rhi::ShaderStageFlagBit GetShaderStage(const std::string& stage);

        // get shader target name
        static Name GetTargetName(const ShaderCompileTarget &target);
    private:
        std::pair<bool, std::string> ProcessShaderSource(const std::string &path);
        std::pair<bool, std::string> ProcessHeaderFile(const std::string &path, ShaderIncludeContext &context, uint32_t depth);

        std::vector<FilePath> searchPaths;
    };

    using ShaderCompileFunc = bool (*)(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result);\
    using GetBinaryCompilerFunc = ShaderCompilerBase* (*)(ShaderCompileTarget);
} // namespace sky
