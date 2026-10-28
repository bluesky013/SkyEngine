//
// Aurora shader compiler: Slang backend implementation.
//

#include <aurora/shader/ShaderCompilerSlang.h>

#include "SpirvArtifact.h"

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderHash.h>
#include <aurora/shader/SlangFileSystemAdapter.h>
#include <core/logger/Logger.h>

#include <slang-com-ptr.h>
#include <slang.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string_view>
#include <vector>

#if defined(SKY_PLATFORM_WINDOWS)
    #include <windows.h>
#endif

static const char *TAG = "ShaderCompilerSlang";

namespace sky::aurora {

    namespace {
        // deliberately leaked: COM global session must outlive slang.dll unload
        slang::IGlobalSession *gGlobalSession = nullptr;
        std::mutex             gGlobalSessionMutex;
        // Slang global-session operations, including downstream selection, require external synchronization.
        std::mutex gCompilerMutex;

#if defined(SKY_PLATFORM_WINDOWS)
        struct PinnedCompilerLibrary {
            HANDLE   file   = INVALID_HANDLE_VALUE;
            HMODULE  module = nullptr;
            uint64_t hash   = 0;

            ~PinnedCompilerLibrary()
            {
                if (module)
                    ::FreeLibrary(module);
                if (file != INVALID_HANDLE_VALUE)
                    ::CloseHandle(file);
            }

            bool Open(const std::filesystem::path &path)
            {
                file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                LARGE_INTEGER size{};
                if (file == INVALID_HANDLE_VALUE || !::GetFileSizeEx(file, &size) || size.QuadPart <= 0)
                    return false;
                uint64_t                read = 0;
                std::array<char, 16384> bytes{};
                hash = HashMixU64(kFnv1aBasis, static_cast<uint64_t>(size.QuadPart));
                for (;;) {
                    DWORD count = 0;
                    if (!::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr))
                        return false;
                    if (!count)
                        break;
                    read += count;
                    hash = HashMixBytes(hash, {bytes.data(), count});
                }
                if (read != static_cast<uint64_t>(size.QuadPart))
                    return false;
                module = ::LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
                if (!module)
                    return false;
                std::vector<wchar_t> actual(32768);
                const DWORD          length = ::GetModuleFileNameW(module, actual.data(), static_cast<DWORD>(actual.size()));
                std::error_code      error;
                return length && length < actual.size() &&
                       std::filesystem::equivalent(path, std::filesystem::path(std::wstring_view(actual.data(), length)), error) && !error;
            }
        };

        struct PinnedDxcLibraries {
            PinnedCompilerLibrary validator;
            PinnedCompilerLibrary compiler;
            uint64_t              fingerprint = 0;
        };

        PinnedDxcLibraries *gDxcLibraries = nullptr;

        bool PinDxcLibraries()
        {
            std::lock_guard lock(gGlobalSessionMutex);
            if (gDxcLibraries)
                return true;
            HMODULE owner = nullptr;
            if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      reinterpret_cast<LPCWSTR>(&gGlobalSession), &owner))
                return false;
            std::vector<wchar_t> path(32768);
            const DWORD          length = ::GetModuleFileNameW(owner, path.data(), static_cast<DWORD>(path.size()));
            if (!length || length >= path.size())
                return false;
            const auto directory    = std::filesystem::path(std::wstring_view(path.data(), length)).parent_path();
            const auto compilerPath = directory / L"dxcompiler.dll";
            auto       libraries    = std::make_unique<PinnedDxcLibraries>();
            if (!libraries->validator.Open(directory / L"dxil.dll") || !libraries->compiler.Open(compilerPath) ||
                !::GetProcAddress(libraries->compiler.module, "DxcCreateInstance"))
                return false;
            if (gGlobalSession->getSharedLibraryLoader() != nullptr)
                return false;
            const auto        encoded = directory.u8string();
            const std::string absoluteDirectory(encoded.begin(), encoded.end());
            // Slang's default loader appends the platform suffix to its unadorned library names.
            gGlobalSession->setDownstreamCompilerPath(SLANG_PASS_THROUGH_DXC, absoluteDirectory.c_str());
            if (SLANG_FAILED(gGlobalSession->checkCompileTargetSupport(SLANG_DXIL))) {
                gGlobalSession->setDownstreamCompilerPath(SLANG_PASS_THROUGH_DXC, "");
                return false;
            }
            libraries->fingerprint = HashMixU64(HashMixU64(HashString("DXC+DXIL"), libraries->compiler.hash), libraries->validator.hash);
            // The global Slang session retains these modules; read handles pin the exact hashed files for the same lifetime.
            gDxcLibraries = libraries.release();
            return true;
        }
#endif

        SlangStage ToSlangStage(ShaderStageFlagBit stage)
        {
            switch (stage) {
            case ShaderStageFlagBit::VS: return SLANG_STAGE_VERTEX;
            case ShaderStageFlagBit::FS: return SLANG_STAGE_FRAGMENT;
            case ShaderStageFlagBit::CS: return SLANG_STAGE_COMPUTE;
            case ShaderStageFlagBit::TAS: return SLANG_STAGE_AMPLIFICATION;
            case ShaderStageFlagBit::MS: return SLANG_STAGE_MESH;
            default: return SLANG_STAGE_NONE;
            }
        }

        const char *SlangTargetProfile(ShaderTarget target)
        {
            switch (target) {
            case ShaderTarget::SPIRV: return "spirv_1_5";
            case ShaderTarget::MSL: return "metallib_2_3";
            case ShaderTarget::DXIL: return "sm_6_5";
            default: return nullptr;
            }
        }

        SlangCompileTarget ToSlangTarget(ShaderTarget target)
        {
            switch (target) {
            case ShaderTarget::SPIRV: return SLANG_SPIRV;
            case ShaderTarget::MSL: return SLANG_METAL;
            case ShaderTarget::DXIL: return SLANG_DXIL;
            default: return SLANG_TARGET_UNKNOWN;
            }
        }

        ShaderScalarType MapScalarType(slang::TypeReflection::ScalarType type)
        {
            switch (type) {
            case slang::TypeReflection::ScalarType::Float32: return ShaderScalarType::FLOAT;
            case slang::TypeReflection::ScalarType::Int32: return ShaderScalarType::INT;
            case slang::TypeReflection::ScalarType::UInt32: return ShaderScalarType::UINT;
            case slang::TypeReflection::ScalarType::Bool: return ShaderScalarType::BOOL;
            default: return ShaderScalarType::UNKNOWN;
            }
        }

        bool Fail(std::string &error, const std::string &message)
        {
            error = message;
            return false;
        }

        bool ReadAttribute(slang::VariableReflection *variable, const char *name, slang::Attribute *&attribute, std::string &error)
        {
            attribute = nullptr;
            if (!variable)
                return Fail(error, "Missing declaration variable");
            for (uint32_t i = 0; i < variable->getUserAttributeCount(); ++i) {
                auto *candidate = variable->getUserAttributeByIndex(i);
                if (candidate && candidate->getName() && std::strcmp(candidate->getName(), name) == 0) {
                    if (attribute)
                        return Fail(error, "Duplicate Aurora declaration identity attribute");
                    attribute = candidate;
                }
            }
            return true;
        }

        bool ReadIdentityString(slang::Attribute *attribute, uint32_t argument, std::string &value, std::string &error)
        {
            size_t      size = 0;
            const char *text = attribute->getArgumentValueString(argument, &size);
            if (!text || !size || size > 1024 || std::memchr(text, 0, size))
                return Fail(error, "Invalid Aurora declaration identity string");
            value.assign(text, size);
            return true;
        }

        bool ReflectResourceType(slang::TypeLayoutReflection *layout, ShaderResource &resource, std::string &error)
        {
            auto *type = layout ? layout->getType() : nullptr;
            while (type && type->getKind() == slang::TypeReflection::Kind::Array) {
                resource.descriptorArray = true;
                const auto count         = type->getElementCount();
                resource.count = resource.count && count && count <= UINT32_MAX / resource.count ? resource.count * static_cast<uint32_t>(count) : 0;
                layout         = layout->getElementTypeLayout();
                type           = type->getElementType();
            }
            if (!type || !layout)
                return Fail(error, "Missing resource type layout");
            if (type->getKind() == slang::TypeReflection::Kind::SamplerState) {
                resource.type          = ShaderResourceType::SAMPLER;
                const std::string name = type->getName() ? type->getName() : "";
                resource.sampler       = name == "SamplerState"             ? ShaderSamplerType::REGULAR
                                         : name == "SamplerComparisonState" ? ShaderSamplerType::COMPARISON
                                                                            : ShaderSamplerType::UNKNOWN;
                return resource.sampler != ShaderSamplerType::UNKNOWN || Fail(error, "Unsupported sampler type");
            }
            if (type->getKind() == slang::TypeReflection::Kind::ConstantBuffer) {
                resource.type = ShaderResourceType::UNIFORM_BUFFER;
                auto *element = layout->getElementTypeLayout();
                if (!element)
                    return Fail(error, "Missing uniform element layout");
                for (uint32_t i = 0; i < element->getCategoryCount(); ++i) {
                    const auto category = element->getCategoryByIndex(i);
                    if (category != slang::ParameterCategory::Uniform && element->getSize(category))
                        return Fail(error, "Resource-containing uniform blocks are unsupported");
                }
                return true;
            }
            if (type->getKind() != slang::TypeReflection::Kind::Resource)
                return Fail(error, "Unsupported resource container; use a direct ConstantBuffer or resource declaration");
            const auto access = type->getResourceAccess();
            if (access != SLANG_RESOURCE_ACCESS_READ && access != SLANG_RESOURCE_ACCESS_READ_WRITE)
                return Fail(error, "Unsupported resource access or counter buffer");
            resource.access  = access == SLANG_RESOURCE_ACCESS_READ ? ShaderResourceAccess::READ_ONLY : ShaderResourceAccess::READ_WRITE;
            const auto shape = type->getResourceShape();
            const auto base  = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;
            resource.type    = access == SLANG_RESOURCE_ACCESS_READ ? ShaderResourceType::SAMPLED_IMAGE : ShaderResourceType::STORAGE_IMAGE;
            switch (base) {
            case SLANG_TEXTURE_1D: resource.texture.dimension = ShaderTextureDimension::D1; break;
            case SLANG_TEXTURE_2D: resource.texture.dimension = ShaderTextureDimension::D2; break;
            case SLANG_TEXTURE_3D: resource.texture.dimension = ShaderTextureDimension::D3; break;
            case SLANG_TEXTURE_CUBE: resource.texture.dimension = ShaderTextureDimension::CUBE; break;
            case SLANG_STRUCTURED_BUFFER: {
                resource.type       = ShaderResourceType::STORAGE_BUFFER;
                resource.bufferKind = ShaderBufferKind::STRUCTURED;
                auto      *element  = layout->getElementTypeLayout();
                const auto stride   = element ? element->getStride(slang::ParameterCategory::Uniform) : 0;
                if (!stride || stride > UINT32_MAX || stride % 4)
                    return Fail(error, "Unknown structured buffer element stride");
                resource.elementStride = static_cast<uint32_t>(stride);
                return true;
            }
            case SLANG_BYTE_ADDRESS_BUFFER:
                resource.type       = ShaderResourceType::STORAGE_BUFFER;
                resource.bufferKind = ShaderBufferKind::RAW;
                return true;
            default: return Fail(error, "Unsupported typed buffer or resource shape");
            }
            resource.texture.arrayed      = (shape & SLANG_TEXTURE_ARRAY_FLAG) != 0;
            resource.texture.multisampled = (shape & SLANG_TEXTURE_MULTISAMPLE_FLAG) != 0;
            if (shape & (SLANG_TEXTURE_SHADOW_FLAG | SLANG_TEXTURE_COMBINED_FLAG | SLANG_TEXTURE_FEEDBACK_FLAG))
                return Fail(error, "Unsupported combined or feedback resource");
            auto *value = type->getResourceResultType();
            if (!value || (value->getKind() != slang::TypeReflection::Kind::Vector && value->getKind() != slang::TypeReflection::Kind::Scalar))
                return Fail(error, "Unsupported image value type");
            resource.texture.components =
                value->getKind() == slang::TypeReflection::Kind::Vector ? static_cast<uint32_t>(value->getElementCount()) : 1;
            resource.texture.scalarType = MapScalarType(value->getScalarType());
            return resource.texture.scalarType != ShaderScalarType::UNKNOWN || Fail(error, "Unsupported image scalar type");
        }

        bool ReflectBlock(slang::VariableLayoutReflection *var,
                          const ShaderResource            &resource,
                          bool                             requireAttributes,
                          ShaderBlockLayout               &block,
                          std::string                     &error)
        {
            auto *element = var->getTypeLayout()->getElementTypeLayout();
            if (!element)
                return Fail(error, "Missing uniform block layout");
            const auto size      = element->getSize(slang::ParameterCategory::Uniform);
            const auto alignment = element->getAlignment();
            if (!size || size > 65536 || alignment <= 0)
                return Fail(error, "Invalid uniform block extent");
            block.name       = resource.name;
            block.set        = resource.set;
            block.binding    = resource.binding;
            block.size       = static_cast<uint32_t>(size);
            block.alignment  = static_cast<uint32_t>(alignment);
            block.structName = element->getName() ? element->getName() : "";
            std::set<std::string> names;
            for (uint32_t f = 0; f < element->getFieldCount(); ++f) {
                auto *field  = element->getFieldByIndex(f);
                auto *layout = field ? field->getTypeLayout() : nullptr;
                auto *type   = layout ? layout->getType() : nullptr;
                if (!field || !type)
                    return Fail(error, "Missing uniform field layout");
                ShaderBlockMember member;
                slang::Attribute *identity = nullptr;
                if (!ReadAttribute(field->getVariable(), "AuroraField", identity, error))
                    return false;
                if (identity) {
                    if (identity->getArgumentCount() != 1 || !ReadIdentityString(identity, 0, member.name, error))
                        return Fail(error, "Invalid AuroraField identity");
                } else if (requireAttributes) {
                    return Fail(error, "Uniform field requires AuroraField identity");
                } else {
                    member.name = field->getName() ? field->getName() : "";
                }
                const auto offset    = field->getOffset(slang::ParameterCategory::Uniform);
                const auto fieldSize = layout->getSize(slang::ParameterCategory::Uniform);
                if (member.name.empty() || !names.insert(member.name).second || offset > size || !fieldSize || fieldSize > size - offset ||
                    layout->getAlignment() <= 0)
                    return Fail(error, "Invalid or duplicate uniform field identity or extent");
                member.offset     = static_cast<uint32_t>(offset);
                member.size       = static_cast<uint32_t>(fieldSize);
                member.alignment  = static_cast<uint32_t>(layout->getAlignment());
                member.scalarType = MapScalarType(type->getScalarType());
                member.rows       = type->getRowCount();
                member.cols       = type->getColumnCount();
                switch (type->getKind()) {
                case slang::TypeReflection::Kind::Scalar: member.kind = ShaderTypeKind::SCALAR; break;
                case slang::TypeReflection::Kind::Vector: member.kind = ShaderTypeKind::VECTOR; break;
                case slang::TypeReflection::Kind::Matrix: {
                    member.kind       = ShaderTypeKind::MATRIX;
                    auto      *vector = layout->getElementTypeLayout();
                    const auto stride = vector ? vector->getStride(slang::ParameterCategory::Uniform) : 0;
                    const auto mode   = layout->getMatrixLayoutMode();
                    if (member.rows != 4 || member.cols != 4 || member.scalarType != ShaderScalarType::FLOAT || stride < 16 || stride > UINT32_MAX ||
                        stride > UINT32_MAX / 4 || member.size != stride * 4 ||
                        (mode != SLANG_MATRIX_LAYOUT_ROW_MAJOR && mode != SLANG_MATRIX_LAYOUT_COLUMN_MAJOR))
                        return Fail(error, "Unsupported matrix storage layout; only reflected float4x4 is supported");
                    member.matrixStride = static_cast<uint32_t>(stride);
                    member.matrixLayout = mode == SLANG_MATRIX_LAYOUT_ROW_MAJOR ? ShaderMatrixLayout::ROW_MAJOR : ShaderMatrixLayout::COLUMN_MAJOR;
                    break;
                }
                case slang::TypeReflection::Kind::Array: {
                    member.kind       = ShaderTypeKind::ARRAY;
                    const auto count  = type->getElementCount();
                    const auto stride = layout->getElementStride(SLANG_PARAMETER_CATEGORY_UNIFORM);
                    auto      *value  = type->getElementType();
                    if (!count || count > 4096 || !stride || stride > UINT32_MAX || !value ||
                        value->getKind() != slang::TypeReflection::Kind::Vector || value->getElementCount() != 4 ||
                        value->getScalarType() != slang::TypeReflection::ScalarType::Float32 || stride < 16 || count > member.size / stride)
                        return Fail(error, "Only bounded float4 uniform arrays are supported");
                    member.arrayCount  = static_cast<uint32_t>(count);
                    member.arrayStride = static_cast<uint32_t>(stride);
                    member.scalarType  = ShaderScalarType::FLOAT;
                    member.rows        = 1;
                    member.cols        = 4;
                    break;
                }
                default: return Fail(error, "Unsupported nested or opaque uniform field");
                }
                if (member.scalarType == ShaderScalarType::UNKNOWN)
                    return Fail(error, "Unsupported uniform scalar storage type");
                // Canonical CPU-facing alignment. Slang reports HLSL cbuffer packing
                // (float4 alignment 4) for DXIL, which must not leak into the shared layout.
                switch (member.kind) {
                case ShaderTypeKind::SCALAR: member.alignment = 4; break;
                case ShaderTypeKind::VECTOR: member.alignment = member.cols == 2 ? 8 : 16; break;
                case ShaderTypeKind::MATRIX:
                case ShaderTypeKind::ARRAY: member.alignment = 16; break;
                default: break;
                }
                block.members.push_back(std::move(member));
            }
            for (const auto &member : block.members)
                block.alignment = std::max(block.alignment, member.alignment);
            return true;
        }

        bool HasCategory(slang::VariableLayoutReflection *var, slang::ParameterCategory category)
        {
            for (uint32_t i = 0; i < var->getCategoryCount(); ++i)
                if (var->getCategoryByIndex(i) == category)
                    return true;
            return false;
        }

        bool NativeAddress(slang::VariableLayoutReflection *var,
                           slang::VariableLayoutReflection *scope,
                           ShaderTarget                     target,
                           const ShaderResource            &resource,
                           ShaderNativeBinding             &native,
                           slang::ParameterCategory        &category,
                           std::string                     &error)
        {
            if (target == ShaderTarget::MSL) {
                category        = resource.type == ShaderResourceType::SAMPLER ? slang::ParameterCategory::SamplerState
                                  : resource.type == ShaderResourceType::SAMPLED_IMAGE || resource.type == ShaderResourceType::STORAGE_IMAGE
                                      ? slang::ParameterCategory::MetalTexture
                                      : slang::ParameterCategory::MetalBuffer;
                native.category = resource.type == ShaderResourceType::SAMPLER         ? ShaderNativeCategory::METAL_SAMPLER
                                  : category == slang::ParameterCategory::MetalTexture ? ShaderNativeCategory::METAL_TEXTURE
                                                                                       : ShaderNativeCategory::METAL_BUFFER;
            } else if (target == ShaderTarget::SPIRV) {
                category        = slang::ParameterCategory::DescriptorTableSlot;
                native.category = ShaderNativeCategory::DESCRIPTOR;
            } else {
                category        = resource.type == ShaderResourceType::SAMPLER          ? slang::ParameterCategory::SamplerState
                                  : resource.type == ShaderResourceType::UNIFORM_BUFFER ? slang::ParameterCategory::ConstantBuffer
                                  : resource.access == ShaderResourceAccess::READ_ONLY  ? slang::ParameterCategory::ShaderResource
                                                                                        : slang::ParameterCategory::UnorderedAccess;
                native.category = resource.type == ShaderResourceType::SAMPLER          ? ShaderNativeCategory::SAMPLER
                                  : resource.type == ShaderResourceType::UNIFORM_BUFFER ? ShaderNativeCategory::CBV
                                  : resource.access == ShaderResourceAccess::READ_ONLY  ? ShaderNativeCategory::SRV
                                                                                        : ShaderNativeCategory::UAV;
            }
            // MetalBuffer/MetalTexture are target-native aliases; mixed Slang categories need no logical-category filtering.
            const auto index      = var->getOffset(category);
            const auto space      = var->getBindingSpace(category);
            const auto scopeIndex = scope && HasCategory(scope, category) ? scope->getOffset(category) : 0;
            const auto scopeSpace = scope && HasCategory(scope, category) ? scope->getBindingSpace(category) : 0;
            if (index > UINT32_MAX || space > UINT32_MAX || scopeIndex > UINT32_MAX - index || scopeSpace > UINT32_MAX - space)
                return Fail(error, "Unknown or overflowing target resource address");
            native.index = static_cast<uint32_t>(index + scopeIndex);
            native.space = static_cast<uint32_t>(space + scopeSpace);
            native.count = resource.count;
            return true;
        }

        bool ReflectVertexInput(slang::VariableLayoutReflection *variable, uint32_t parentLocation, ShaderReflection &reflection)
        {
            auto *layout = variable ? variable->getTypeLayout() : nullptr;
            if (!layout || !layout->getSize(slang::ParameterCategory::VaryingInput))
                return true;
            const auto offset = variable->getOffset(slang::ParameterCategory::VaryingInput);
            if (offset > UINT32_MAX - parentLocation)
                return false;
            const auto location = parentLocation + static_cast<uint32_t>(offset);
            if (layout->getKind() == slang::TypeReflection::Kind::Struct) {
                for (uint32_t i = 0; i < layout->getFieldCount(); ++i)
                    if (!ReflectVertexInput(layout->getFieldByIndex(i), location, reflection))
                        return false;
                return true;
            }
            const auto *semantic = variable->getSemanticName();
            if (!semantic)
                return false;
            std::string name(semantic);
            std::ranges::transform(name, name.begin(), [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
            if (name.starts_with("SV_"))
                return true;
            auto      *type       = layout->getType();
            const auto kind       = type->getKind();
            const auto components = kind == slang::TypeReflection::Kind::Scalar ? 1u : type->getColumnCount();
            const auto scalar     = MapScalarType(type->getScalarType());
            if ((kind != slang::TypeReflection::Kind::Scalar && kind != slang::TypeReflection::Kind::Vector) || !components || components > 4 ||
                scalar == ShaderScalarType::UNKNOWN || scalar == ShaderScalarType::BOOL || variable->getSemanticIndex() > UINT32_MAX ||
                reflection.vertexInputs.size() >= 16)
                return false;
            reflection.vertexInputs.push_back(
                {std::move(name), static_cast<uint32_t>(variable->getSemanticIndex()), location, scalar, static_cast<uint32_t>(components)});
            return true;
        }

        bool CollectSlangResources(slang::ProgramLayout *layout,
                                   ShaderReflection     &reflection,
                                   ShaderTarget          target,
                                   ShaderStageFlagBit    stage,
                                   slang::IMetadata     *metadata,
                                   std::string          &error)
        {
            reflection.bindingModel = target == ShaderTarget::SPIRV ? ShaderBindingModel::VULKAN_DESCRIPTORS
                                      : target == ShaderTarget::MSL ? ShaderBindingModel::METAL_DIRECT_ARGUMENTS
                                                                    : ShaderBindingModel::D3D12_REGISTERS;
            reflection.kind         = metadata ? ShaderReflectionKind::STAGE : ShaderReflectionKind::DECLARATION;
            if (metadata)
                reflection.stages = stage;
            std::set<std::pair<uint32_t, uint32_t>>             keys;
            std::set<std::pair<ShaderNativeCategory, uint32_t>> metalSlots;
            auto collect = [&](slang::VariableLayoutReflection *var, slang::VariableLayoutReflection *scope, bool entryScope) {
                auto *type = var ? var->getTypeLayout()->getType() : nullptr;
                if (!type)
                    return Fail(error, "Missing shader parameter layout");
                if ((entryScope && var->getCategory() == slang::ParameterCategory::None) ||
                    var->getCategory() == slang::ParameterCategory::SpecializationConstant ||
                    var->getCategory() == slang::ParameterCategory::VaryingInput || var->getCategory() == slang::ParameterCategory::VaryingOutput)
                    return true;
                ShaderResource resource;
                if (!ReflectResourceType(var->getTypeLayout(), resource, error))
                    return false;
                resource.name = var->getName() ? var->getName() : "";
                if (metadata)
                    resource.stages = stage;
                slang::Attribute *identity = nullptr;
                if (!ReadAttribute(var->getVariable(), "AuroraResource", identity, error))
                    return false;
                if (!identity) {
                    error = std::string("Resource requires AuroraResource logical identity: ") + (var->getName() ? var->getName() : "?");
                    return false;
                }
                int set = -1, binding = -1;
                if (identity->getArgumentCount() != 3 || SLANG_FAILED(identity->getArgumentValueInt(0, &set)) ||
                    SLANG_FAILED(identity->getArgumentValueInt(1, &binding)) || set < 0 || binding < 0 ||
                    !ReadIdentityString(identity, 2, resource.declarationOwner, error))
                    return Fail(error, "Invalid AuroraResource logical identity");
                resource.set     = static_cast<uint32_t>(set);
                resource.binding = static_cast<uint32_t>(binding);
                if (resource.name.empty() || !keys.emplace(resource.set, resource.binding).second)
                    return Fail(error, "Duplicate or missing logical resource declaration");
                reflection.declarations.push_back(resource);
                ShaderBlockLayout block;
                if (resource.type == ShaderResourceType::UNIFORM_BUFFER && !resource.descriptorArray) {
                    if (!ReflectBlock(var, resource, true, block, error))
                        return false;
                    reflection.declaredBlocks.push_back(block);
                }
                if (!metadata)
                    return true;
                if (!resource.count || (target == ShaderTarget::MSL && resource.descriptorArray))
                    return Fail(error, "Unsupported descriptor array or unknown resource count");
                ShaderNativeBinding native;
                native.set     = resource.set;
                native.binding = resource.binding;
                native.stage   = stage;
                slang::ParameterCategory category;
                if (!NativeAddress(var, scope, target, resource, native, category, error))
                    return false;
                bool used = false;
                if (SLANG_FAILED(metadata->isParameterLocationUsed(static_cast<SlangParameterCategory>(category), native.space, native.index, used)))
                    return Fail(error, "Target resource usage query failed");
                if (!used)
                    return true;
                // Slang usage keys retain authored spaces; Metal direct arguments have no native space.
                if (target == ShaderTarget::MSL) {
                    if (!metalSlots.emplace(native.category, native.index).second)
                        return Fail(error, "Unsupported Metal direct bindings: distinct register spaces collide at one native argument slot");
                    native.space = 0;
                }
                reflection.resources.push_back(std::move(resource));
                reflection.nativeBindings.push_back(native);
                if (reflection.resources.back().type == ShaderResourceType::UNIFORM_BUFFER) {
                    if (block.size == 0)
                        return Fail(error, "Unsupported uniform descriptor array");
                    reflection.blocks.push_back(std::move(block));
                }
                return true;
            };
            auto *globalScope = layout->getGlobalParamsVarLayout();
            for (uint32_t i = 0; i < layout->getParameterCount(); ++i)
                if (!collect(layout->getParameterByIndex(i), globalScope, false))
                    return false;
            if (metadata) {
                auto *entry = layout->getEntryPointByIndex(0);
                if (!entry)
                    return Fail(error, "Missing entry point target layout");
                for (uint32_t i = 0; i < entry->getParameterCount(); ++i)
                    if (!collect(entry->getParameterByIndex(i), entry->getVarLayout(), true))
                        return false;
            }
            reflection.complete = true;
            return true;
        }

    } // namespace

    bool FinalizeSpirvArtifact(std::vector<uint8_t> &code, ShaderStageFlagBit stage, std::string &entry, std::string &error)
    {
        error.clear();
        if (code.size() < 20 || code.size() % 4)
            return Fail(error, "Malformed SPIR-V payload");
        const auto word = [&](size_t index) {
            const auto *bytes = code.data() + index * 4;
            return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
        };
        if (word(0) != 0x07230203u || word(4) != 0)
            return Fail(error, "Malformed SPIR-V header");
        uint32_t model = 0;
        switch (stage) {
        case ShaderStageFlagBit::VS: model = 0; break;
        case ShaderStageFlagBit::FS: model = 4; break;
        case ShaderStageFlagBit::CS: model = 5; break;
        case ShaderStageFlagBit::TAS: model = 5364; break;
        case ShaderStageFlagBit::MS: model = 5365; break;
        default: return Fail(error, "Unsupported SPIR-V entry stage");
        }
        std::vector<uint8_t> finalized;
        finalized.reserve(code.size());
        finalized.insert(finalized.end(), code.begin(), code.begin() + 20);
        std::string nativeEntry;
        for (size_t i = 5; i < code.size() / 4;) {
            const auto instruction = word(i);
            const auto count       = instruction >> 16;
            const auto opcode      = instruction & 0xffffu;
            if (!count || count > code.size() / 4 - i)
                return Fail(error, "Malformed SPIR-V instruction");
            const auto terminated = [&](size_t first) { return std::memchr(code.data() + (i + first) * 4, 0, (count - first) * 4) != nullptr; };
            if (opcode == 3) {
                if (count < 3 || (count > 4 && !terminated(4)))
                    return Fail(error, "Malformed SPIR-V OpSource");
            } else if (opcode == 2) {
                if (count < 2 || !terminated(1))
                    return Fail(error, "Malformed SPIR-V OpSourceContinued");
            } else {
                if (opcode == 15) {
                    if (count < 4)
                        return Fail(error, "Malformed SPIR-V entry point");
                    const char *name = reinterpret_cast<const char *>(code.data() + (i + 3) * 4);
                    const char *end  = static_cast<const char *>(std::memchr(name, 0, (count - 3) * 4));
                    if (!end || end == name)
                        return Fail(error, "Malformed SPIR-V entry point");
                    if (word(i + 1) == model) {
                        if (!nativeEntry.empty())
                            return Fail(error, "Ambiguous SPIR-V entry point");
                        nativeEntry.assign(name, end);
                    }
                }
                finalized.insert(finalized.end(), code.begin() + i * 4, code.begin() + (i + count) * 4);
            }
            i += count;
        }
        if (nativeEntry.empty())
            return Fail(error, "SPIR-V has no matching native entry point");
        code  = std::move(finalized);
        entry = std::move(nativeEntry);
        return true;
    }

    bool ShaderCompilerSlang::InitGlobalSession()
    {
        std::lock_guard lock(gGlobalSessionMutex);
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

    bool ShaderCompilerSlang::InspectVariants(const ShaderCompileDesc &desc, std::vector<ShaderVariantBinding> &bindings, std::string &error)
    {
        ShaderCompileResult inspected;
        if (!CompileImpl(desc, inspected, nullptr, true)) {
            error = inspected.errorInfo;
            return false;
        }
        bindings = std::move(inspected.variantBindings);
        return true;
    }

    bool ShaderCompilerSlang::Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result)
    {
        result = {};
        std::vector<ShaderVariantBinding> bindings;
        if (desc.schema && !desc.schema->entries.empty() && !InspectVariants(desc, bindings, result.errorInfo))
            return false;
        ShaderCompileResult candidate;
        if (!CompileImpl(desc, candidate, &bindings, false)) {
            result.errorInfo = std::move(candidate.errorInfo);
            return false;
        }
        const auto materialBufferCount = std::ranges::count_if(candidate.reflection.declarations, [](const auto &resource) {
            return resource.set == 2 &&
                   (resource.type == ShaderResourceType::UNIFORM_BUFFER || resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC);
        });
        if (materialBufferCount > 1) {
            result.errorInfo = "Material Group 2 permits at most one logical cbuffer";
            return false;
        }
        bool probe = false;
        if (desc.schema && desc.variant)
            for (const auto &value : desc.variant->entries) {
                const auto *entry   = desc.schema->FindEntry(value.key);
                const auto  mapping = std::ranges::find(bindings, value.key.GetStr(), &ShaderVariantBinding::key);
                probe               = probe || (entry && value.value != entry->defaultValue &&
                                  (mapping == bindings.end() || mapping->lowering == ShaderVariantLowering::COMPILE));
            }
        if (probe) {
            auto baselineDesc        = desc;
            baselineDesc.variant     = nullptr;
            baselineDesc.variantHash = 0;
            ShaderCompileResult baseline;
            if (!CompileImpl(baselineDesc, baseline, &bindings, false)) {
                result.errorInfo = "Variant baseline declarations are invalid: " + baseline.errorInfo;
                return false;
            }
            const auto materialInterface = [](const ShaderReflection &reflection) {
                ShaderReflection material;
                for (const auto &resource : reflection.declarations)
                    if (resource.set == 2)
                        material.declarations.push_back(resource);
                for (const auto &block : reflection.declaredBlocks)
                    if (block.set == 2)
                        material.declaredBlocks.push_back(block);
                return ShaderInterfaceFingerprint(material);
            };
            if (materialInterface(candidate.reflection) != materialInterface(baseline.reflection)) {
                result.errorInfo = "Material Group 2 declarations or ABI change with the selected variant; keep declarations stable";
                return false;
            }
        }
        result = std::move(candidate);
        return true;
    }

    bool ShaderCompilerSlang::CompileImpl(const ShaderCompileDesc                 &desc,
                                          ShaderCompileResult                     &result,
                                          const std::vector<ShaderVariantBinding> *bindings,
                                          bool                                     inspect)
    {
        result = {};
        ShaderCompileResult         candidate;
        Slang::ComPtr<slang::IBlob> lastDiagnostics;
        const bool                  success = [&]() {
            if (!InitGlobalSession()) {
                return false;
            }
            const auto producer = GetProducerIdentity(desc.target);
            if (producer.compiler.empty() || producer.version.empty())
                return Fail(candidate.errorInfo, "Shader target compiler identity is unavailable; DXIL requires the deployed DXC and DXIL libraries");
            std::lock_guard compilerLock(gCompilerMutex);

            auto environment = desc.environment == ShaderTargetEnvironment{} ? DefaultShaderEnvironment(desc.target) : desc.environment;
            if (environment.target != desc.target || !NormalizeShaderEnvironment(environment, candidate.errorInfo))
                return Fail(candidate.errorInfo, "Compile target and requested environment disagree");
            if (environment.minimumVersion != DefaultShaderEnvironment(desc.target, environment.platform).minimumVersion ||
                environment.requiredCapabilities || environment.codeFormat == ShaderCodeFormat::METALLIB)
                return Fail(candidate.errorInfo, "Unsupported target capability or precompiled code request");
            const auto slangTarget = ToSlangTarget(desc.target);
            if (slangTarget == SLANG_TARGET_UNKNOWN || ToSlangStage(desc.stage) == SLANG_STAGE_NONE || desc.entry.empty() || desc.source.empty())
                return Fail(candidate.errorInfo, "Unsupported shader target, stage, or missing source entry");
            slang::SessionDesc sessionDesc{};
            slang::TargetDesc  targetDesc{};
            targetDesc.format  = slangTarget;
            targetDesc.profile = gGlobalSession->findProfile(environment.profile.c_str());
            if (targetDesc.profile == SLANG_PROFILE_UNKNOWN)
                return Fail(candidate.errorInfo, "Unknown target profile");
            sessionDesc.defaultMatrixLayoutMode =
                environment.matrixLayout == ShaderMatrixLayout::ROW_MAJOR ? SLANG_MATRIX_LAYOUT_ROW_MAJOR : SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

            std::vector<ShaderVariantValue> variants;
            if (desc.schema) {
                auto          effectiveSchema = *desc.schema;
                ShaderVariant effectiveValues = desc.variant ? *desc.variant : ShaderVariant{};
                for (auto &entry : effectiveSchema.entries) {
                    if (inspect && !entry.symbol.Empty()) {
                        std::erase_if(effectiveValues.entries, [&](const auto &v) { return v.key == entry.key; });
                        effectiveValues.entries.push_back({entry.key, entry.defaultValue, entry.type});
                    }
                }
                if (!CanonicalShaderVariants(&effectiveValues, effectiveSchema, desc.target, variants, &candidate.errorInfo,
                                             inspect ? nullptr : bindings))
                    return false;
            } else if (desc.variant) {
                std::set<std::string> names;
                for (const auto &entry : desc.variant->entries) {
                    std::string name(entry.key.GetStr());
                    if (name.empty() || !names.insert(name).second)
                        return Fail(candidate.errorInfo, "Duplicate or empty shader macro name");
                    variants.push_back({std::move(name), entry.value, entry.type});
                }
                std::sort(variants.begin(), variants.end(), [](const auto &left, const auto &right) { return left.name < right.name; });
            }
            const auto variantHash = HashShaderVariants(variants);
            if (!inspect && desc.variantHash && desc.variantHash != variantHash)
                return Fail(candidate.errorInfo, "Compile variant fingerprint does not match effective macros");
            std::vector<std::string>                  macroValues;
            std::vector<slang::PreprocessorMacroDesc> macros;
            macros.push_back({"AURORA_TARGET_SPIRV", desc.target == ShaderTarget::SPIRV ? "1" : "0"});
            macros.push_back({"AURORA_TARGET_MSL", desc.target == ShaderTarget::MSL ? "1" : "0"});
            macros.push_back({"AURORA_TARGET_DXIL", desc.target == ShaderTarget::DXIL ? "1" : "0"});
            macroValues.reserve(variants.size());
            for (const auto &entry : variants) {
                macroValues.push_back(entry.type == ShaderVariantType::INT32 ? std::to_string(std::bit_cast<int32_t>(entry.value))
                                                                                              : std::to_string(entry.value));
                macros.push_back({entry.name.c_str(), macroValues.back().c_str()});
            }
            sessionDesc.preprocessorMacros     = macros.empty() ? nullptr : macros.data();
            sessionDesc.preprocessorMacroCount = static_cast<SlangInt>(macros.size());

            std::vector<slang::CompilerOptionEntry> options;
            slang::CompilerOptionEntry              lineOption{};
            lineOption.name            = slang::CompilerOptionName::LineDirectiveMode;
            lineOption.value.kind      = slang::CompilerOptionValueKind::Int;
            lineOption.value.intValue0 = SLANG_LINE_DIRECTIVE_MODE_NONE;
            options.push_back(lineOption);
            bool optimizationSet = false;
            for (const auto &option : environment.options) {
                if (option.size() != 14 || option.compare(0, 13, "optimization=") || option[13] < '0' || option[13] > '3' || optimizationSet)
                    return Fail(candidate.errorInfo, "Unsupported or conflicting target compiler option");
                slang::CompilerOptionEntry optimization{};
                optimization.name            = slang::CompilerOptionName::Optimization;
                optimization.value.kind      = slang::CompilerOptionValueKind::Int;
                optimization.value.intValue0 = option[13] - '0';
                options.push_back(optimization);
                optimizationSet = true;
            }
            targetDesc.compilerOptionEntries    = options.data();
            targetDesc.compilerOptionEntryCount = static_cast<uint32_t>(options.size());
            sessionDesc.targets                 = &targetDesc;
            sessionDesc.targetCount             = 1;
            // Angle includes need an explicit root; ShaderFileSystem resolves logical paths.
            const auto sourceSnapshot =
                desc.fileSystem != nullptr && desc.fileSystem->IsBundlePath(desc.sourcePath) ? desc.fileSystem->Snapshot(desc.sourcePath) : nullptr;
            std::vector<std::string>        includePaths;
            std::vector<const char *>       includeRoots;
            Slang::ComPtr<ISlangFileSystem> slangFileSystem;
            if (desc.fileSystem != nullptr) {
                auto *files  = sourceSnapshot ? sourceSnapshot.get() : desc.fileSystem;
                includePaths = files->IncludeRoots();
                for (const auto &path : includePaths) {
                    includeRoots.push_back(path.c_str());
                }
                slangFileSystem.attach(CreateSlangFileSystem(files));
                sessionDesc.searchPaths     = includeRoots.data();
                sessionDesc.searchPathCount = static_cast<SlangInt>(includeRoots.size());
            }
            sessionDesc.fileSystem = slangFileSystem.get();
            Slang::ComPtr<slang::ISession> session;
            if (!SLANG_SUCCEEDED(gGlobalSession->createSession(sessionDesc, session.writeRef()))) {
                candidate.errorInfo = "createSession failed";
                return false;
            }

            auto             &diagnostics = lastDiagnostics;
            const std::string sourcePath  = desc.sourcePath.empty() ? std::string("spike.slang") : desc.sourcePath;
            // The session's module cache owns the returned module; a ComPtr would
            // release a cache reference and corrupt session teardown.
            slang::IModule *module = session->loadModuleFromSourceString("shader", sourcePath.c_str(), desc.source.c_str(), diagnostics.writeRef());
            if (diagnostics != nullptr) {
                candidate.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
            }
            if (module == nullptr) {
                return false;
            }

            if (inspect) {
                auto *declarations = module->getModuleReflection();
                auto *moduleLayout = module->getLayout(0, diagnostics.writeRef());
                if (!declarations || !moduleLayout)
                    return Fail(candidate.errorInfo, "Variant declaration reflection is unavailable");
                std::set<uint32_t>    nativeIds;
                std::set<std::string> symbols;
                if (desc.schema)
                    for (const auto &entry : desc.schema->entries) {
                        ShaderVariantBinding binding{std::string(entry.key.GetStr()),
                                                     std::string(entry.symbol.GetStr()),
                                                     entry.type,
                                                     ShaderVariantLowering::COMPILE,
                                                     0,
                                                     entry.defaultValue};
                        const auto           invalid = [&](const char *reason) {
                            return Fail(candidate.errorInfo, "Variant " + binding.key + " / " + desc.entry + " / " +
                                                                                            std::to_string(static_cast<uint32_t>(desc.target)) + ": " + reason);
                        };
                        if (binding.symbol.empty()) {
                            candidate.variantBindings.push_back(binding);
                            continue;
                        }
                        if (!symbols.insert(binding.symbol).second || binding.symbol == binding.key)
                            return invalid("source symbol must be unique and distinct from its preprocessor key");
                        slang::VariableReflection *variable = nullptr;
                        for (auto *child : declarations->getChildren()) {
                            auto *declared = child->asVariable();
                            if (declared && declared->getName() && binding.symbol == declared->getName()) {
                                if (variable)
                                    return invalid("ambiguous source symbol");
                                variable = declared;
                            }
                        }
                        if (!variable)
                            return invalid("source symbol is not a stable top-level declaration");
                        const auto expectedType = entry.type == ShaderVariantType::BOOL    ? slang::TypeReflection::ScalarType::Bool
                                                                   : entry.type == ShaderVariantType::INT32 ? slang::TypeReflection::ScalarType::Int32
                                                                                                            : slang::TypeReflection::ScalarType::UInt32;
                        if (!variable->getType() || variable->getType()->getKind() != slang::TypeReflection::Kind::Scalar ||
                            variable->getType()->getScalarType() != expectedType)
                            return invalid("source type differs from bool/int32/uint32 logical type");
                        Slang::ComPtr<slang::IBlob> defaultValue;
                        if (SLANG_FAILED(variable->getDefaultValueBlob(defaultValue.writeRef())) || !defaultValue ||
                            defaultValue->getBufferSize() != sizeof(uint32_t))
                            return invalid("source default is missing or cannot be represented");
                        uint32_t bits = 0;
                        std::memcpy(&bits, defaultValue->getBufferPointer(), sizeof(bits));
                        if (bits != entry.defaultValue)
                            return invalid("source default disagrees with logical default");
                        for (unsigned i = 0; i < moduleLayout->getParameterCount(); ++i) {
                            auto *parameter = moduleLayout->getParameterByIndex(i);
                            if (!parameter->getName() || binding.symbol != parameter->getName())
                                continue;
                            if (parameter->getCategory() != slang::ParameterCategory::SpecializationConstant || desc.target == ShaderTarget::DXIL)
                                return invalid("target declaration must be a native constant or an explicit static compile-time value");
                            const auto id = parameter->getOffset(slang::ParameterCategory::SpecializationConstant);
                            if (id > UINT32_MAX || !nativeIds.insert(static_cast<uint32_t>(id)).second)
                                return invalid("native ID is invalid or duplicated within the stage");
                            binding.lowering = ShaderVariantLowering::NATIVE;
                            binding.nativeId = static_cast<uint32_t>(id);
                        }
                        if (!variable->findModifier(slang::Modifier::Const) ||
                            (binding.lowering == ShaderVariantLowering::COMPILE && !variable->findModifier(slang::Modifier::Static)))
                            return invalid("compile-time fallback must be an explicit static const declaration");
                        candidate.variantBindings.push_back(std::move(binding));
                    }
                std::sort(candidate.variantBindings.begin(), candidate.variantBindings.end(),
                                           [](const auto &a, const auto &b) { return a.key < b.key; });
                return true;
            }
            if (bindings) {
                candidate.variantBindings = *bindings;
                for (const auto &binding : *bindings) {
                    if (binding.symbol.empty())
                        continue;
                    uint32_t expected = binding.defaultValue;
                    if (binding.lowering == ShaderVariantLowering::COMPILE && desc.variant)
                        for (const auto &value : desc.variant->entries)
                            if (value.key.GetStr() == binding.key)
                                expected = value.value;
                    bool verified = false;
                    for (auto *child : module->getModuleReflection()->getChildren()) {
                        auto *variable = child->asVariable();
                        if (!variable || !variable->getName() || binding.symbol != variable->getName())
                            continue;
                        Slang::ComPtr<slang::IBlob> value;
                        uint32_t                    bits = 0;
                        if (SLANG_FAILED(variable->getDefaultValueBlob(value.writeRef())) || !value || value->getBufferSize() != sizeof(bits))
                            return Fail(candidate.errorInfo, "Selected variant cannot be reflected: " + binding.key);
                        std::memcpy(&bits, value->getBufferPointer(), sizeof(bits));
                        verified = bits == expected;
                    }
                    if (!verified)
                        return Fail(candidate.errorInfo, "Selected variant is not represented by its source declaration: " + binding.key);
                }
            }

            Slang::ComPtr<slang::IEntryPoint> entryPoint;
            if (!SLANG_SUCCEEDED(
                    module->findAndCheckEntryPoint(desc.entry.c_str(), ToSlangStage(desc.stage), entryPoint.writeRef(), diagnostics.writeRef()))) {
                if (diagnostics != nullptr) {
                    candidate.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
                }
                return false;
            }

            std::array<slang::IComponentType *, 2> components = {module, entryPoint.get()};
            Slang::ComPtr<slang::IComponentType>   composed;
            if (!SLANG_SUCCEEDED(
                    session->createCompositeComponentType(components.data(), components.size(), composed.writeRef(), diagnostics.writeRef()))) {
                if (diagnostics != nullptr) {
                    candidate.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
                }
                return false;
            }

            Slang::ComPtr<slang::IComponentType> linked;
            if (!SLANG_SUCCEEDED(composed->link(linked.writeRef(), diagnostics.writeRef()))) {
                if (diagnostics != nullptr) {
                    candidate.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
                }
                return false;
            }

            Slang::ComPtr<slang::IBlob> code;
            if (!SLANG_SUCCEEDED(linked->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef()))) {
                if (diagnostics != nullptr) {
                    candidate.errorInfo = static_cast<const char *>(diagnostics->getBufferPointer());
                }
                return false;
            }

            const auto  *bytes = static_cast<const uint8_t *>(code->getBufferPointer());
            const size_t size  = code->getBufferSize();

            if (!bytes || !size || size > 64u * 1024u * 1024u)
                return Fail(candidate.errorInfo, "Missing or oversized shader payload");
            candidate.data.assign(bytes, bytes + size);

            auto *layout = linked->getLayout(0, diagnostics.writeRef());
            if (!layout || layout->getEntryPointCount() != 1)
                return Fail(candidate.errorInfo, "Missing or ambiguous target entry reflection");
            Slang::ComPtr<slang::IMetadata> metadata;
            if (SLANG_FAILED(linked->getEntryPointMetadata(0, 0, metadata.writeRef())) || !metadata)
                return Fail(candidate.errorInfo, "Target usage metadata is unavailable");
            if (!CollectSlangResources(layout, candidate.reflection, desc.target, desc.stage, metadata.get(), candidate.errorInfo))
                return false;
            auto *entryLayout = layout->getEntryPointByIndex(0);
            if (!entryLayout)
                return Fail(candidate.errorInfo, "Missing or ambiguous target entry reflection");
            if (desc.stage == ShaderStageFlagBit::VS) {
                for (uint32_t i = 0; i < entryLayout->getParameterCount(); ++i)
                    if (!ReflectVertexInput(entryLayout->getParameterByIndex(i), 0, candidate.reflection))
                        return Fail(candidate.errorInfo, "Vertex inputs require bounded scalar/vector semantics");
            }
            if (desc.stage == ShaderStageFlagBit::CS) {
                SlangUInt sizes[3]{};
                entryLayout->getComputeThreadGroupSize(3, sizes);
                for (uint32_t i = 0; i < 3; ++i) {
                    if (!sizes[i] || sizes[i] > UINT32_MAX)
                        return Fail(candidate.errorInfo, "Unknown or overflowing compute thread group size");
                    candidate.reflection.threadGroupSize[i] = static_cast<uint32_t>(sizes[i]);
                }
            }
            if (desc.schema && !CanonicalShaderVariants(desc.variant, *desc.schema, desc.target, candidate.selection, &candidate.errorInfo))
                return false;
            candidate.identity.sourcePath  = desc.sourcePath.empty() ? "inline.slang" : desc.sourcePath;
            candidate.identity.sourceEntry = desc.entry;
            candidate.identity.stage       = desc.stage;
            if (desc.target == ShaderTarget::SPIRV) {
                if (!FinalizeSpirvArtifact(candidate.data, desc.stage, candidate.identity.nativeEntry, candidate.errorInfo))
                    return false;
            } else {
                const char *nativeEntry = entryLayout->getNameOverride();
                if (!nativeEntry || !*nativeEntry)
                    return Fail(candidate.errorInfo, "Target reflection has no native entry symbol");
                candidate.identity.nativeEntry = nativeEntry;
            }
            candidate.identity.sourceHash  = desc.sourceHash ? desc.sourceHash : HashString(desc.source);
            candidate.identity.variantHash = variantHash;
            candidate.identity.layoutFp    = desc.layoutFp;
            candidate.identity.schemaFp    = desc.schemaFp;
            candidate.identity.environment = std::move(environment);
            candidate.identity.producer    = producer;
            candidate.identity.variants    = std::move(variants);
            if (!SealShaderArtifact(candidate, candidate.errorInfo))
                return false;

            return true;
        }();
        if (!success) {
            if (candidate.errorInfo.empty() && lastDiagnostics != nullptr)
                candidate.errorInfo = static_cast<const char *>(lastDiagnostics->getBufferPointer());
            result.errorInfo = candidate.errorInfo.empty() ? "Slang compilation failed" : std::move(candidate.errorInfo);
            return false;
        }
        result = std::move(candidate);
        return true;
    }

    ShaderProducerIdentity ShaderCompilerSlang::GetProducerIdentity() const
    {
        return GetProducerIdentity(ShaderTarget::SPIRV);
    }

    ShaderProducerIdentity ShaderCompilerSlang::GetProducerIdentity(ShaderTarget target) const
    {
        if (!InitGlobalSession())
            return {};
        std::lock_guard compilerLock(gCompilerMutex);
        if (ToSlangTarget(target) == SLANG_TARGET_UNKNOWN)
            return {};
        if (target == ShaderTarget::DXIL) {
#if defined(SKY_PLATFORM_WINDOWS)
            if (!PinDxcLibraries())
                return {};
#else
            return {};
#endif
        }
        ShaderProducerIdentity identity;
        identity.compiler       = "Slang";
        const char *version     = gGlobalSession->getBuildTagString();
        identity.version        = version ? version : "";
        identity.adapterVersion = 3;
        identity.configuration  = HashString(
            "AuroraResource-v1;AuroraField-v1;single-target;direct-arguments;exact-metadata;native-mat4-stride;line-none;typed-variant-symbol-v1");
        if (target == ShaderTarget::SPIRV)
            identity.configuration = HashMixU64(identity.configuration, HashString("spirv-source-omitted-v1"));
#if defined(SKY_PLATFORM_WINDOWS)
        if (target == ShaderTarget::DXIL)
            identity.configuration = HashMixU64(identity.configuration, gDxcLibraries->fingerprint);
#endif
        return identity;
    }

    bool ShaderCompilerSlang::ReflectBlocks(const std::string &source, ShaderTarget target, ShaderReflection &reflection, std::string &error)
    {
        reflection = {};
        error.clear();
        if (!InitGlobalSession())
            return Fail(error, "createGlobalSession failed");
        std::lock_guard compilerLock(gCompilerMutex);
        const auto      slangTarget = ToSlangTarget(target);
        const char     *profile     = SlangTargetProfile(target);
        if (slangTarget == SLANG_TARGET_UNKNOWN || !profile)
            return Fail(error, "Unsupported Slang target");
        slang::TargetDesc targetDesc{};
        targetDesc.format  = slangTarget;
        targetDesc.profile = gGlobalSession->findProfile(profile);
        slang::SessionDesc sessionDesc{};
        sessionDesc.targets                 = &targetDesc;
        sessionDesc.targetCount             = 1;
        sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
        const std::array<slang::PreprocessorMacroDesc, 3> macros{{
            {"AURORA_TARGET_SPIRV", target == ShaderTarget::SPIRV ? "1" : "0"},
            {"AURORA_TARGET_MSL", target == ShaderTarget::MSL ? "1" : "0"},
            {"AURORA_TARGET_DXIL", target == ShaderTarget::DXIL ? "1" : "0"},
        }};
        sessionDesc.preprocessorMacros     = macros.data();
        sessionDesc.preprocessorMacroCount = macros.size();
        Slang::ComPtr<slang::ISession> session;
        if (SLANG_FAILED(gGlobalSession->createSession(sessionDesc, session.writeRef())))
            return Fail(error, "createSession failed");
        Slang::ComPtr<slang::IBlob> diagnostics;
        // Borrowed from the session module cache; see Compile.
        slang::IModule *module = session->loadModuleFromSourceString("declarations", "declarations.slang", source.c_str(), diagnostics.writeRef());
        if (!module) {
            error = diagnostics ? static_cast<const char *>(diagnostics->getBufferPointer()) : "Declaration module load failed";
            return false;
        }
        Slang::ComPtr<slang::IComponentType> linked;
        if (SLANG_FAILED(module->link(linked.writeRef(), diagnostics.writeRef()))) {
            error = diagnostics ? static_cast<const char *>(diagnostics->getBufferPointer()) : "Declaration link failed";
            return false;
        }
        auto            *layout = linked->getLayout(0, diagnostics.writeRef());
        ShaderReflection candidate;
        if (!layout || !CollectSlangResources(layout, candidate, target, ShaderStageFlagBit::VS, nullptr, error))
            return error.empty() ? Fail(error, "Declaration layout is unavailable") : false;
        reflection = std::move(candidate);
        return true;
    }

} // namespace sky::aurora
