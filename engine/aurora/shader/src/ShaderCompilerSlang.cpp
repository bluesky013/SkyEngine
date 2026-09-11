//
// Aurora shader compiler: Slang backend implementation.
//

#include <aurora/shader/ShaderCompilerSlang.h>
#include <core/logger/Logger.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <array>
#include <cstring>

static const char *TAG = "ShaderCompilerSlang";

namespace sky::aurora {

    namespace {
        // deliberately leaked: COM global session must outlive slang.dll unload
        slang::IGlobalSession *gGlobalSession = nullptr;

        SlangStage ToSlangStage(ShaderStageFlagBit stage)
        {
            switch (stage) {
            case ShaderStageFlagBit::VS:  return SLANG_STAGE_VERTEX;
            case ShaderStageFlagBit::FS:  return SLANG_STAGE_FRAGMENT;
            case ShaderStageFlagBit::CS:  return SLANG_STAGE_COMPUTE;
            case ShaderStageFlagBit::TAS: return SLANG_STAGE_AMPLIFICATION;
            case ShaderStageFlagBit::MS:  return SLANG_STAGE_MESH;
            default:                      return SLANG_STAGE_NONE;
            }
        }

        const char *SlangTargetProfile(ShaderTarget target)
        {
            switch (target) {
            case ShaderTarget::SPIRV: return "spirv_1_5";
            case ShaderTarget::MSL:   return "metal";
            case ShaderTarget::DXIL:  return "sm_6_5";
            default:                  return nullptr;
            }
        }

        SlangCompileTarget ToSlangTarget(ShaderTarget target)
        {
            switch (target) {
            case ShaderTarget::SPIRV: return SLANG_SPIRV;
            case ShaderTarget::MSL:   return SLANG_METAL;
            case ShaderTarget::DXIL:  return SLANG_DXIL;
            default:                  return SLANG_TARGET_UNKNOWN;
            }
        }

        ShaderResourceType FromSlangCategory(slang::ParameterCategory category, slang::TypeLayoutReflection *typeLayout)
        {
            if (category == slang::ParameterCategory::DescriptorTableSlot && typeLayout != nullptr) {
                const auto kind = typeLayout->getType()->getKind();
                switch (kind) {
                case slang::TypeReflection::Kind::SamplerState: return ShaderResourceType::SAMPLER;
                default:                                        return ShaderResourceType::SAMPLED_IMAGE;
                }
            }
            switch (category) {
            case slang::ParameterCategory::SamplerState:
                return ShaderResourceType::SAMPLER;
            case slang::ParameterCategory::ConstantBuffer:
            case slang::ParameterCategory::PushConstantBuffer:
                return ShaderResourceType::UNIFORM_BUFFER;
            case slang::ParameterCategory::ShaderResource:
                return ShaderResourceType::SAMPLED_IMAGE;
            case slang::ParameterCategory::UnorderedAccess:
                return ShaderResourceType::STORAGE_IMAGE;
            default:
                return ShaderResourceType::UNIFORM_BUFFER;
            }
        }

        void ReflectBlockMembers(slang::VariableLayoutReflection *var, uint32_t set, uint32_t binding,
                                 ShaderCompileResult &result)
        {
            // ParameterBlock / ConstantBuffer: unwrap to the element type, then read fields
            auto *typeLayout = var->getTypeLayout();
            auto *elemLayout = typeLayout->getElementTypeLayout();
            if (elemLayout == nullptr) {
                return;
            }

            ShaderBlockLayout block{};
            block.name    = var->getName() != nullptr ? var->getName() : "";
            block.set     = set;
            block.binding = binding;
            block.size    = static_cast<uint32_t>(
                elemLayout->getSize(slang::ParameterCategory::Uniform));

            const uint32_t fieldCount = elemLayout->getFieldCount();
            for (uint32_t f = 0; f < fieldCount; ++f) {
                auto *field = elemLayout->getFieldByIndex(f);
                if (field == nullptr) {
                    continue;
                }
                ShaderBlockMember member{};
                member.name   = field->getName() != nullptr ? field->getName() : "";
                member.offset = static_cast<uint32_t>(
                    field->getOffset(slang::ParameterCategory::Uniform));
                auto *fieldType = field->getTypeLayout();
                member.size = fieldType != nullptr
                    ? static_cast<uint32_t>(fieldType->getSize(slang::ParameterCategory::Uniform))
                    : 0;
                block.members.push_back(std::move(member));
            }
            result.reflection.blocks.push_back(std::move(block));
        }

        void CollectSlangResources(slang::ProgramLayout *layout, ShaderCompileResult &result)
        {
            const uint32_t count = layout->getParameterCount();
            for (uint32_t i = 0; i < count; ++i) {
                auto *var = layout->getParameterByIndex(i);
                if (var == nullptr) {
                    continue;
                }

                const auto category = var->getCategory();

                // ParameterBlock occupies a whole register space (descriptor set);
                // on SPIRV its getBindingIndex() is the descriptor set index it owns
                if (category == slang::ParameterCategory::SubElementRegisterSpace) {
                    const auto blockSet = var->getBindingIndex();
                    const uint32_t set = blockSet == SLANG_UNKNOWN_SIZE ? 0 : static_cast<uint32_t>(blockSet);

                    ShaderResource res{};
                    res.name    = var->getName() != nullptr ? var->getName() : "";
                    res.set     = set;
                    res.binding = 0;
                    res.type    = ShaderResourceType::UNIFORM_BUFFER;
                    result.reflection.resources.push_back(std::move(res));

                    ReflectBlockMembers(var, set, 0, result);
                    continue;
                }

                if (category != slang::ParameterCategory::ConstantBuffer &&
                    category != slang::ParameterCategory::ShaderResource &&
                    category != slang::ParameterCategory::UnorderedAccess &&
                    category != slang::ParameterCategory::SamplerState &&
                    category != slang::ParameterCategory::DescriptorTableSlot &&
                    category != slang::ParameterCategory::PushConstantBuffer) {
                    continue;
                }

                const uint32_t set     = static_cast<uint32_t>(var->getBindingSpace());
                const uint32_t binding = static_cast<uint32_t>(var->getBindingIndex());
                if (set == SLANG_UNKNOWN_SIZE || binding == SLANG_UNKNOWN_SIZE) {
                    continue;
                }

                ShaderResource res{};
                res.name    = var->getName() != nullptr ? var->getName() : "";
                res.set     = set;
                res.binding = binding;
                res.type    = FromSlangCategory(category, var->getTypeLayout());
                result.reflection.resources.push_back(std::move(res));

                if (res.type == ShaderResourceType::UNIFORM_BUFFER) {
                    ReflectBlockMembers(var, set, binding, result);
                }
            }
        }
    } // namespace

    bool ShaderCompilerSlang::InitGlobalSession()
    {
        if (gGlobalSession != nullptr) {
            return true;
        }
        slang::IGlobalSession *session = nullptr;
        if (!SLANG_SUCCEEDED(createGlobalSession(&session))) {
            LOG_E(TAG, "createGlobalSession failed");
            return false;
        }
        gGlobalSession = session;
        return true;
    }

    bool ShaderCompilerSlang::Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result)
    {
        if (!InitGlobalSession()) {
            return false;
        }

        const auto slangTarget = ToSlangTarget(desc.target);
        const char *profileName = SlangTargetProfile(desc.target);
        if (slangTarget == SLANG_TARGET_UNKNOWN || profileName == nullptr) {
            result.errorInfo = "unsupported slang target";
            return false;
        }

        slang::SessionDesc sessionDesc = {};
        slang::TargetDesc targetDesc = {};
        targetDesc.format  = slangTarget;
        targetDesc.profile = gGlobalSession->findProfile(profileName);

        // no #line directives in emitted code (keeps MSL/GLSL output clean);
        // output-format options live on TargetDesc
        slang::CompilerOptionEntry lineOpt{};
        lineOpt.name            = slang::CompilerOptionName::LineDirectiveMode;
        lineOpt.value.kind      = slang::CompilerOptionValueKind::Int;
        lineOpt.value.intValue0 = SLANG_LINE_DIRECTIVE_MODE_NONE;
        targetDesc.compilerOptionEntries    = &lineOpt;
        targetDesc.compilerOptionEntryCount = 1;

        sessionDesc.targets     = &targetDesc;
        sessionDesc.targetCount = 1;

        Slang::ComPtr<slang::ISession> session;
        if (!SLANG_SUCCEEDED(gGlobalSession->createSession(sessionDesc, session.writeRef()))) {
            result.errorInfo = "createSession failed";
            return false;
        }

        Slang::ComPtr<slang::IBlob> diagnostics;
        Slang::ComPtr<slang::IModule> module;
        module.attach(session->loadModuleFromSourceString(
            "spike", "spike.slang", desc.source.c_str(), diagnostics.writeRef()));
        if (diagnostics != nullptr) {
            result.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
        }
        if (module == nullptr) {
            return false;
        }

        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        if (!SLANG_SUCCEEDED(module->findAndCheckEntryPoint(desc.entry.c_str(), ToSlangStage(desc.stage),
                                                            entryPoint.writeRef(), diagnostics.writeRef()))) {
            if (diagnostics != nullptr) {
                result.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
            }
            return false;
        }

        std::array<slang::IComponentType *, 2> components = {module.get(), entryPoint.get()};
        Slang::ComPtr<slang::IComponentType> composed;
        if (!SLANG_SUCCEEDED(session->createCompositeComponentType(components.data(), components.size(),
                                                                   composed.writeRef(), diagnostics.writeRef()))) {
            if (diagnostics != nullptr) {
                result.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
            }
            return false;
        }

        Slang::ComPtr<slang::IComponentType> linked;
        if (!SLANG_SUCCEEDED(composed->link(linked.writeRef(), diagnostics.writeRef()))) {
            if (diagnostics != nullptr) {
                result.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
            }
            return false;
        }

        Slang::ComPtr<slang::IBlob> code;
        if (!SLANG_SUCCEEDED(linked->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef()))) {
            if (diagnostics != nullptr) {
                result.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
            }
            return false;
        }

        const auto *bytes = static_cast<const uint8_t *>(code->getBufferPointer());
        const size_t size = code->getBufferSize();

        if (desc.target == ShaderTarget::SPIRV) {
            const size_t wordCount = size / sizeof(uint32_t);
            result.data.assign(reinterpret_cast<const uint32_t *>(bytes),
                               reinterpret_cast<const uint32_t *>(bytes) + wordCount);
        } else {
            // MSL text / DXIL blob packed into words
            const size_t wordCount = (size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
            result.data.resize(wordCount, 0);
            std::memcpy(result.data.data(), bytes, size);
        }

        // slang-side reflection (per-platform own reflection principle)
        if (auto *layout = linked->getLayout(0, diagnostics.writeRef())) {
            CollectSlangResources(layout, result);
        }

        // spike: intentionally leak session/module chain; COM teardown order in slang
        // is fragile and not worth stabilizing until the spike proves the approach
        code.detach();
        linked.detach();
        composed.detach();
        entryPoint.detach();
        module.detach();
        session.detach();
        return true;
    }

} // namespace sky::aurora
