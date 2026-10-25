#pragma once

#include <aurora/shader/IShaderCompiler.h>
#include <aurora/shader/ShaderCacheStore.h>

#include <core/file/FileSystem.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cstdlib>
#include <filesystem>

namespace sky::aurora::test {
    struct ShaderCacheDirectory {
        std::filesystem::path path;
        FileSystemPtr         fs;
        explicit ShaderCacheDirectory(const std::string &name)
        {
            path = std::filesystem::temp_directory_path() / ("sky_shader_artifact_" + name + "_" + std::to_string(::rand()));
            std::filesystem::create_directories(path);
            fs = new NativeFileSystem(FilePath(path));
        }
        ~ShaderCacheDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
    inline ShaderProducerIdentity TestProducer(const std::string &version = "1")
    {
        return {"test-compiler", version, 1, ShaderReflection::kProtocolVersion, 0};
    }
    inline ShaderCacheKey TestCacheKey(const ShaderVariantSchema &schema   = {},
                                       const std::string         &path     = "s.slang",
                                       ShaderTarget               target   = ShaderTarget::SPIRV,
                                       ShaderPlatform             platform = ShaderPlatform::MACOS)
    {
        ShaderCacheKey key;
        key.sourcePath  = path;
        key.sourceEntry = "mainFS";
        key.entryHash   = HashEntryName(key.sourceEntry);
        key.stage       = static_cast<uint32_t>(ShaderStageFlagBit::FS);
        key.target      = static_cast<uint32_t>(target);
        key.environment = DefaultShaderEnvironment(target, platform);
        key.sourceHash  = HashString("test-source");
        key.schemaFp    = HashSchema(schema);
        key.variantHash = HashShaderVariants({});
        key.producer    = TestProducer();
        key.toolchainFp = ShaderProducerFingerprint(key.producer);
        return key;
    }
    inline ShaderCompileResult TestArtifact(const ShaderCacheKey &key)
    {
        ShaderCompileResult result;
        auto               &id         = result.identity;
        id.sourcePath                  = key.sourcePath;
        id.sourceEntry                 = key.sourceEntry;
        id.nativeEntry                 = key.sourceEntry;
        id.stage                       = static_cast<ShaderStageFlagBit>(key.stage);
        id.sourceHash                  = key.sourceHash;
        id.variantHash                 = key.variantHash;
        id.layoutFp                    = key.layoutFp;
        id.schemaFp                    = key.schemaFp;
        id.environment                 = key.environment;
        id.producer                    = key.producer;
        id.variants                    = key.variants;
        result.reflection.kind         = ShaderReflectionKind::STAGE;
        result.reflection.complete     = true;
        result.reflection.stages       = ShaderStageFlags(id.stage);
        result.reflection.bindingModel = key.environment.target == ShaderTarget::SPIRV ? ShaderBindingModel::VULKAN_DESCRIPTORS
                                         : key.environment.target == ShaderTarget::MSL ? ShaderBindingModel::METAL_DIRECT_ARGUMENTS
                                                                                       : ShaderBindingModel::D3D12_REGISTERS;
        result.data                    = key.environment.target == ShaderTarget::SPIRV
                                             ? std::vector<uint8_t>{3, 2, 35, 7, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}
                                             : std::vector<uint8_t>{'t', 'e', 's', 't'};
        SealShaderArtifact(result, result.errorInfo);
        return result;
    }
    inline std::vector<ShaderVariantBinding> TestVariantBindings(const ShaderCompileDesc &desc)
    {
        std::vector<ShaderVariantBinding> result;
        if (desc.schema)
            for (const auto &entry : desc.schema->entries)
                result.push_back({std::string(entry.key.GetStr()), std::string(entry.symbol.GetStr()), entry.type,
                                  !entry.symbol.Empty() && desc.target != ShaderTarget::DXIL ? ShaderVariantLowering::NATIVE : ShaderVariantLowering::COMPILE,
                                  entry.symbol.Empty() ? 0u : 3u, entry.defaultValue});
        return result;
    }
    inline bool TestCompile(const ShaderCompileDesc &desc, const ShaderProducerIdentity &producer, ShaderCompileResult &out)
    {
        ShaderCacheKey key =
            TestCacheKey(desc.schema != nullptr ? *desc.schema : ShaderVariantSchema{}, desc.sourcePath, desc.target, desc.environment.platform);
        key.sourceEntry = desc.entry;
        key.entryHash   = HashEntryName(desc.entry);
        key.stage       = static_cast<uint32_t>(desc.stage);
        key.sourceHash  = desc.sourceHash;
        key.variantHash = desc.variantHash;
        key.layoutFp    = desc.layoutFp;
        key.schemaFp    = desc.schemaFp;
        key.environment = desc.environment;
        key.producer    = producer;
        key.toolchainFp = ShaderProducerFingerprint(producer);
        const auto bindings = TestVariantBindings(desc);
        if (desc.schema != nullptr && !CanonicalShaderVariants(desc.variant, *desc.schema, desc.target, key.variants, &out.errorInfo, &bindings)) {
            return false;
        }
        out = TestArtifact(key);
        out.variantBindings = bindings;
        if (desc.schema && !CanonicalShaderVariants(desc.variant, *desc.schema, desc.target, out.selection, &out.errorInfo))
            return false;
        return SealShaderArtifact(out, out.errorInfo);
    }
    inline std::string TestProductManifest(const ShaderTargetEnvironment &environment)
    {
        const char *platform = "unknown";
        switch (environment.platform) {
        case ShaderPlatform::WINDOWS: platform = "windows"; break;
        case ShaderPlatform::MACOS: platform = "macos"; break;
        case ShaderPlatform::IOS: platform = "ios"; break;
        case ShaderPlatform::ANDROID_OS: platform = "android"; break;
        case ShaderPlatform::LINUX: platform = "linux"; break;
        default: break;
        }
        rapidjson::Document document(rapidjson::kObjectType);
        auto               &allocator = document.GetAllocator();
        document.AddMember("formatVersion", 2, allocator);
        document.AddMember("platform", rapidjson::Value(platform, allocator), allocator);
        const auto      *target = ShaderTargetDirName(static_cast<uint32_t>(environment.target));
        rapidjson::Value targets(rapidjson::kArrayType);
        targets.PushBack(rapidjson::Value(target, allocator), allocator);
        document.AddMember("targets", targets, allocator);
        rapidjson::Value value(rapidjson::kObjectType);
        value.AddMember("target", static_cast<uint32_t>(environment.target), allocator);
        value.AddMember("platform", static_cast<uint32_t>(environment.platform), allocator);
        value.AddMember("codeFormat", static_cast<uint32_t>(environment.codeFormat), allocator);
        value.AddMember("profile", rapidjson::Value(environment.profile.c_str(), allocator), allocator);
        value.AddMember("minimumVersion", environment.minimumVersion, allocator);
        value.AddMember("requiredCapabilities", environment.requiredCapabilities, allocator);
        value.AddMember("matrixLayout", static_cast<uint32_t>(environment.matrixLayout), allocator);
        rapidjson::Value options(rapidjson::kArrayType);
        for (const auto &option : environment.options) {
            options.PushBack(rapidjson::Value(option.c_str(), allocator), allocator);
        }
        value.AddMember("options", options, allocator);
        rapidjson::Value environments(rapidjson::kObjectType), name(target, allocator);
        environments.AddMember(name, value, allocator);
        document.AddMember("environments", environments, allocator);
        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        return {buffer.GetString(), buffer.GetSize()};
    }

    class TestShaderCompiler : public IShaderCompiler {
    public:
        uint32_t               count    = 0;
        ShaderProducerIdentity producer = TestProducer();
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return producer;
        }
        bool InspectVariants(const ShaderCompileDesc &desc, std::vector<ShaderVariantBinding> &bindings, std::string &) override
        {
            bindings = TestVariantBindings(desc);
            return true;
        }
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            ++count;
            return TestCompile(desc, producer, out);
        }
    };
    struct CompilerRegistration {
        explicit CompilerRegistration(IShaderCompiler *compiler)
        {
            ShaderCompilerFactory::Get().Register(compiler);
        }
        ~CompilerRegistration()
        {
            ShaderCompilerFactory::Get().Register(nullptr);
        }
    };
} // namespace sky::aurora::test
