//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <utility>
#include <vector>
#include <memory>
#include <map>
#include <variant>
#include <rhi/Device.h>
#include <render/resource/ResourceGroup.h>
#include <core/util/Uuid.h>

namespace sky {

    struct ShaderBufferMember {
        std::string name;
        uint32_t offset;
        uint32_t size;
    };

    enum class ShaderResourceType {
        UNIFORM_BUFFER,
        TEXTURE,
        SAMPLER,
        STORAGE_IMAGE,
        STORAGE_BUFFER,
        INPUT_ATTACHMENT,
        PUSH_CONSTANT
    };

    struct ShaderResource {
        std::string name;
        uint32_t space;
        uint32_t binding;
        ShaderResourceType resourceType;
    };

    struct ShaderReflection {
        std::vector<ShaderResource> resources;
    };

    class ShaderPreprocessor {
    public:
        ShaderPreprocessor() = default;
        ~ShaderPreprocessor() = default;

        struct Def {};

        using ValueType = std::variant<std::string, uint32_t, Def>;
        void SetValue(const std::string &key, const ValueType &val);
        void CalculateHash();
        uint32_t GetHash() const { return hash; }

        std::string BuildSource() const;

    private:
        std::map<std::string, ValueType> values;
        uint32_t hash = 0;
    };

    class ShaderCollection {
    public:
        explicit ShaderCollection(std::string val) : source(std::move(val)) {}
        ~ShaderCollection() = default;

        const std::string &RequestSource() const { return source; }
        std::string RequestSource(const ShaderPreprocessor &);

        void SetUuid(const Uuid &id) { uuid = id; }
        const Uuid &GetUuid() const { return uuid; }
    private:
        std::string source;
        Uuid uuid;
    };
    using ShaderCollectionPtr = std::shared_ptr<ShaderCollection>;

    class Program {
    public:
        Program() = default;
        ~Program() = default;

        void SetShader(const ShaderCollectionPtr &shader);

        RDResourceLayoutPtr RequestLayout(uint32_t index) const;
        const rhi::PipelineLayoutPtr &GetPipelineLayout() const { return pipelineLayout; }
    private:
        ShaderCollectionPtr shader;
        ShaderReflection reflection;

        rhi::PipelineLayoutPtr pipelineLayout;
    };
    using RDProgramPtr = std::shared_ptr<Program>;

} // namespace sky
