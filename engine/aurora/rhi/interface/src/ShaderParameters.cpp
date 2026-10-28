//
// Created on 2026/10/23.
//

#include <aurora/rhi/ShaderParameters.h>

#include <algorithm>
#include <aurora/rhi/Shader.h>
#include <aurora/rhi/ShaderArtifact.h>
#include <map>
#include <set>

namespace sky::aurora {
    namespace {
        bool Invalid(std::string &error, const char *message)
        {
            error = message;
            return false;
        }
        uint32_t DataSize(ShaderDataType type, uint32_t count)
        {
            switch (type) {
            case ShaderDataType::FLOAT:
            case ShaderDataType::INT:
            case ShaderDataType::UINT: return 4;
            case ShaderDataType::FLOAT2: return 8;
            case ShaderDataType::FLOAT4: return 16;
            case ShaderDataType::MAT4: return 64;
            case ShaderDataType::FLOAT4_ARRAY: return count && count <= 4096 ? count * 16 : 0;
            default: return 0;
            }
        }
        void Mix(uint64_t &hash, uint64_t value)
        {
            for (uint32_t i = 0; i < 8; ++i) {
                hash ^= (value >> (8 * i)) & 255u;
                hash *= 1099511628211ull;
            }
        }
        void Mix(uint64_t &hash, const std::string &value)
        {
            Mix(hash, value.size());
            for (const auto byte : value) {
                hash ^= static_cast<uint8_t>(byte);
                hash *= 1099511628211ull;
            }
        }
    } // namespace

    ShaderDataType ShaderMemberDataType(const ShaderBlockMember &member)
    {
        if (member.scalarBytes != 4)
            return ShaderDataType::UNKNOWN;
        if (member.kind == ShaderTypeKind::SCALAR && member.rows == 1 && member.cols == 1 && !member.arrayCount) {
            return member.scalarType == ShaderScalarType::FLOAT  ? ShaderDataType::FLOAT
                   : member.scalarType == ShaderScalarType::INT  ? ShaderDataType::INT
                   : member.scalarType == ShaderScalarType::UINT ? ShaderDataType::UINT
                                                                 : ShaderDataType::UNKNOWN;
        }
        if (member.scalarType != ShaderScalarType::FLOAT)
            return ShaderDataType::UNKNOWN;
        if (member.kind == ShaderTypeKind::VECTOR && member.rows == 1 && !member.arrayCount)
            return member.cols == 2 ? ShaderDataType::FLOAT2 : member.cols == 4 ? ShaderDataType::FLOAT4 : ShaderDataType::UNKNOWN;
        if (member.kind == ShaderTypeKind::MATRIX && member.rows == 4 && member.cols == 4 && !member.arrayCount)
            return ShaderDataType::MAT4;
        if (member.kind == ShaderTypeKind::ARRAY && member.rows == 1 && member.cols == 4 && member.arrayCount && member.arrayCount <= 4096)
            return ShaderDataType::FLOAT4_ARRAY;
        return ShaderDataType::UNKNOWN;
    }

    bool ValidateShaderData(const ShaderDataValue &value, std::string &error)
    {
        const auto size = DataSize(value.type, value.count);
        if (!size || value.bytes.size() != size || (value.type != ShaderDataType::FLOAT4_ARRAY && value.count) ||
            (value.matrixLayout != ShaderMatrixLayout::COLUMN_MAJOR && value.matrixLayout != ShaderMatrixLayout::ROW_MAJOR))
            return Invalid(error, "Unsupported or malformed typed shader data");
        return true;
    }

    uint64_t ShaderCpuSchemaFingerprint(const ShaderCpuSchema &schema)
    {
        uint64_t hash = 1469598103934665603ull;
        Mix(hash, UniformUploadPlan::kProtocolVersion);
        Mix(hash, schema.size);
        Mix(hash, schema.alignment);
        Mix(hash, schema.fields.size());
        auto fields = schema.fields;
        std::ranges::sort(fields, {}, &ShaderBlockMember::name);
        for (const auto &field : fields) {
            Mix(hash, field.name);
            Mix(hash, field.offset);
            Mix(hash, field.size);
            Mix(hash, static_cast<uint32_t>(field.scalarType));
            Mix(hash, static_cast<uint32_t>(field.kind));
            Mix(hash, field.rows);
            Mix(hash, field.cols);
            Mix(hash, field.arrayCount);
            Mix(hash, field.arrayStride);
            Mix(hash, field.alignment);
            Mix(hash, field.scalarBytes);
            Mix(hash, field.matrixStride);
            Mix(hash, static_cast<uint32_t>(field.matrixLayout));
        }
        return hash;
    }

    bool BuildUniformUploadPlan(
        const ShaderCpuSchema &cpu, const ShaderBlockLayout &target, uint64_t artifactId, UniformUploadPlan &out, std::string &error)
    {
        if (!cpu.size || cpu.size > 65536 || !target.size || target.size > 65536 || cpu.fields.size() != target.members.size() || !cpu.alignment ||
            (cpu.alignment & (cpu.alignment - 1)) || !target.alignment || (target.alignment & (target.alignment - 1)))
            return Invalid(error, "Uniform plan requires complete bounded CPU and target schemas");
        UniformUploadPlan plan;
        plan.sourceSize        = cpu.size;
        plan.targetSize        = target.size;
        const auto rangesValid = [](const std::vector<ShaderBlockMember> &fields, uint32_t size) {
            std::vector<std::pair<uint32_t, uint32_t>> ranges;
            for (const auto &field : fields) {
                if (!field.size || field.offset > size || field.size > size - field.offset || !field.alignment ||
                    (field.alignment & (field.alignment - 1)))
                    return false;
                ranges.emplace_back(field.offset, field.offset + field.size);
            }
            std::ranges::sort(ranges);
            for (size_t i = 1; i < ranges.size(); ++i)
                if (ranges[i].first < ranges[i - 1].second)
                    return false;
            return true;
        };
        if (!rangesValid(cpu.fields, cpu.size) || !rangesValid(target.members, target.size))
            return Invalid(error, "Uniform field storage ranges overlap or are invalid");
        std::set<std::string> sourceNames, targetNames;
        std::vector<bool>     sourceUsed(cpu.size), targetUsed(target.size);
        for (const auto &field : target.members) {
            const auto source = std::ranges::find(cpu.fields, field.name, &ShaderBlockMember::name);
            const auto type   = ShaderMemberDataType(field);
            if (!targetNames.insert(field.name).second || source == cpu.fields.end() || !sourceNames.insert(source->name).second ||
                type == ShaderDataType::UNKNOWN || ShaderMemberDataType(*source) != type || source->arrayCount != field.arrayCount ||
                source->offset > cpu.size || source->size > cpu.size - source->offset || field.offset > target.size ||
                field.size > target.size - field.offset)
                return Invalid(error, "Uniform field identity, type or declared range is incompatible");
            const auto copy = [&](uint32_t src, uint32_t dst, uint32_t size) {
                if (src > source->size || size > source->size - src || dst > field.size || size > field.size - dst)
                    return false;
                src += source->offset;
                dst += field.offset;
                for (uint32_t i = 0; i < size; ++i)
                    if (sourceUsed[src + i] || targetUsed[dst + i])
                        return false;
                for (uint32_t i = 0; i < size; ++i) {
                    sourceUsed[src + i] = true;
                    targetUsed[dst + i] = true;
                }
                if (!plan.segments.empty() && plan.segments.back().source + plan.segments.back().size == src &&
                    plan.segments.back().target + plan.segments.back().size == dst)
                    plan.segments.back().size += size;
                else
                    plan.segments.push_back({src, dst, size});
                return true;
            };
            if (type == ShaderDataType::MAT4) {
                if (source->matrixStride < 16 || field.matrixStride < 16 || source->matrixStride > source->size / 4 ||
                    field.matrixStride > field.size / 4 || static_cast<uint32_t>(source->matrixLayout) > 1 ||
                    static_cast<uint32_t>(field.matrixLayout) > 1)
                    return Invalid(error, "Invalid matrix storage stride or major convention");
                for (uint32_t column = 0; column < 4; ++column)
                    for (uint32_t row = 0; row < 4; ++row) {
                        const auto src = source->matrixLayout == ShaderMatrixLayout::COLUMN_MAJOR ? column * source->matrixStride + row * 4
                                                                                                  : row * source->matrixStride + column * 4;
                        const auto dst = field.matrixLayout == ShaderMatrixLayout::COLUMN_MAJOR ? column * field.matrixStride + row * 4
                                                                                                : row * field.matrixStride + column * 4;
                        if (!copy(src, dst, 4))
                            return Invalid(error, "Matrix storage overlaps or exceeds its declared range");
                    }
            } else if (type == ShaderDataType::FLOAT4_ARRAY) {
                if (source->arrayStride < 16 || field.arrayStride < 16 || source->arrayCount > source->size / source->arrayStride ||
                    field.arrayCount > field.size / field.arrayStride)
                    return Invalid(error, "Invalid uniform array storage stride");
                for (uint32_t i = 0; i < field.arrayCount; ++i)
                    if (!copy(i * source->arrayStride, i * field.arrayStride, 16))
                        return Invalid(error, "Uniform array storage overlaps");
            } else if (!copy(0, 0, DataSize(type, 0)))
                return Invalid(error, "Uniform field storage overlaps or exceeds its declared range");
        }
        plan.wholeCopy = plan.segments.size() == 1 && plan.segments[0].source == 0 && plan.segments[0].target == 0 &&
                         plan.segments[0].size == cpu.size && cpu.size == target.size;
        plan.key = ShaderCpuSchemaFingerprint(cpu);
        Mix(plan.key, artifactId);
        ShaderReflection targetLayout;
        targetLayout.blocks.push_back(target);
        Mix(plan.key, ShaderReflectionFingerprint(targetLayout));
        out = std::move(plan);
        return true;
    }

    bool EncodeShaderUniform(const UniformUploadPlan &plan, std::span<const std::byte> source, std::vector<std::byte> &out, std::string &error)
    {
        if (!plan.key || source.size() != plan.sourceSize || !plan.targetSize || plan.targetSize > 65536 || plan.segments.empty())
            return Invalid(error, "Uniform source does not match its upload plan");
        std::vector<std::byte> bytes(plan.targetSize);
        if (plan.wholeCopy) {
            if (plan.sourceSize != plan.targetSize || plan.segments.size() != 1 || plan.segments[0].source || plan.segments[0].target ||
                plan.segments[0].size != plan.targetSize)
                return Invalid(error, "Invalid whole-block uniform copy plan");
            std::memcpy(bytes.data(), source.data(), bytes.size());
            out = std::move(bytes);
            return true;
        }
        for (const auto &segment : plan.segments) {
            if (!segment.size || segment.source > source.size() || segment.size > source.size() - segment.source || segment.target > bytes.size() ||
                segment.size > bytes.size() - segment.target)
                return Invalid(error, "Uniform upload plan contains an invalid copy range");
            std::memcpy(bytes.data() + segment.target, source.data() + segment.source, segment.size);
        }
        out = std::move(bytes);
        return true;
    }

    bool BuildShaderDataSchema(std::span<const std::pair<std::string, ShaderDataValue>> values,
                               ShaderCpuSchema                                         &outSchema,
                               std::vector<std::byte>                                  &outBytes,
                               std::string                                             &error)
    {
        ShaderCpuSchema        schema;
        std::vector<std::byte> source;
        std::set<std::string>  names;
        for (const auto &[name, value] : values) {
            if (!ValidateShaderData(value, error))
                return false;
            if (name.empty() || !names.insert(name).second || source.size() > 65536 - value.bytes.size())
                return Invalid(error, "Duplicate uniform name or CPU data exceeds the block limit");
            ShaderBlockMember field;
            field.name         = name;
            field.offset       = static_cast<uint32_t>(source.size());
            field.size         = static_cast<uint32_t>(value.bytes.size());
            field.scalarType   = value.type == ShaderDataType::INT    ? ShaderScalarType::INT
                                 : value.type == ShaderDataType::UINT ? ShaderScalarType::UINT
                                                                      : ShaderScalarType::FLOAT;
            field.kind         = value.type == ShaderDataType::MAT4                                             ? ShaderTypeKind::MATRIX
                                 : value.type == ShaderDataType::FLOAT4_ARRAY                                   ? ShaderTypeKind::ARRAY
                                 : value.type == ShaderDataType::FLOAT4 || value.type == ShaderDataType::FLOAT2 ? ShaderTypeKind::VECTOR
                                                                                                                : ShaderTypeKind::SCALAR;
            field.cols         = value.type == ShaderDataType::FLOAT2 ? 2
                                 : value.type == ShaderDataType::MAT4 || value.type == ShaderDataType::FLOAT4 || value.type == ShaderDataType::FLOAT4_ARRAY
                                     ? 4
                                     : 1;
            field.rows         = value.type == ShaderDataType::MAT4 ? 4 : 1;
            field.matrixLayout = value.matrixLayout;
            field.arrayCount   = value.count;
            field.matrixStride = value.type == ShaderDataType::MAT4 ? 16 : 0;
            field.arrayStride  = value.count ? 16 : 0;
            schema.fields.push_back(std::move(field));
            source.insert(source.end(), value.bytes.begin(), value.bytes.end());
        }
        schema.size = static_cast<uint32_t>(source.size());
        outSchema   = std::move(schema);
        outBytes    = std::move(source);
        return true;
    }
    bool PackShaderData(const ShaderBlockLayout                                 &target,
                        std::span<const std::pair<std::string, ShaderDataValue>> values,
                        std::vector<std::byte>                                  &out,
                        std::string                                             &error)
    {
        ShaderCpuSchema        schema;
        std::vector<std::byte> source;
        UniformUploadPlan      plan;
        return BuildShaderDataSchema(values, schema, source, error) && BuildUniformUploadPlan(schema, target, 0, plan, error) &&
               EncodeShaderUniform(plan, source, out, error);
    }

    ShaderParameterLookup ShaderParameterTable::Resolve(Name name, uint32_t set, ShaderParameterHandle &out) const
    {
        std::string target(name.GetStr());
        for (const auto &alias : aliases)
            if (alias.alias == target) {
                target = alias.target;
                break;
            }
        uint32_t index = UINT32_MAX;
        for (uint32_t i = 0; i < entries.size(); ++i) {
            if (entries[i].set != set || entries[i].name != target)
                continue;
            if (index != UINT32_MAX)
                return ShaderParameterLookup::AMBIGUOUS;
            index = i;
        }
        if (index == UINT32_MAX)
            return ShaderParameterLookup::UNKNOWN;
        out = {generation, index, set};
        return entries[index].active ? ShaderParameterLookup::READY : ShaderParameterLookup::INACTIVE;
    }
    ShaderParameterLookup ShaderParameterTable::Resolve(ShaderParameterHandle handle, const ShaderParameterEntry *&out) const
    {
        if (!generation || handle.generation != generation || handle.index >= entries.size() || entries[handle.index].set != handle.set)
            return ShaderParameterLookup::STALE;
        out = &entries[handle.index];
        return out->active ? ShaderParameterLookup::READY : ShaderParameterLookup::INACTIVE;
    }
    bool BuildShaderParameterTable(const ShaderReflection               &reflection,
                                   uint64_t                              generation,
                                   std::span<const ShaderParameterAlias> aliases,
                                   ShaderParameterTable                 &out,
                                   std::string                          &error)
    {
        if (!generation || !ValidateShaderReflection(reflection, error))
            return false;
        ShaderParameterTable table;
        table.generation                                     = generation;
        const auto                                &resources = reflection.declarations;
        std::set<std::pair<uint32_t, std::string>> names;
        for (const auto &resource : resources) {
            if (!names.emplace(resource.set, resource.name).second)
                return Invalid(error, "Ambiguous logical parameter name");
            const bool active = std::ranges::any_of(reflection.resources,
                                                    [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; });
            table.entries.push_back(
                {.name = resource.name, .set = resource.set, .binding = resource.binding, .resourceType = resource.type, .active = active});
            if (resource.type != ShaderResourceType::UNIFORM_BUFFER && resource.type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC)
                continue;
            const auto block = std::ranges::find_if(reflection.declaredBlocks,
                                                    [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; });
            if (block == reflection.declaredBlocks.end())
                return Invalid(error, "Declared uniform is missing its field schema");
            for (const auto &field : block->members) {
                const auto name = resource.name + "." + field.name;
                if (!names.emplace(resource.set, name).second)
                    return Invalid(error, "Ambiguous logical uniform field name");
                table.entries.push_back({.name         = name,
                                         .set          = resource.set,
                                         .binding      = resource.binding,
                                         .kind         = ShaderParameterKind::FIELD,
                                         .resourceType = resource.type,
                                         .dataType     = ShaderMemberDataType(field),
                                         .arrayCount   = field.arrayCount,
                                         .active       = active,
                                         .field        = field.name});
            }
        }
        std::set<std::string> aliasNames;
        for (const auto &alias : aliases) {
            if (alias.alias.empty() || !aliasNames.insert(alias.alias).second ||
                std::ranges::any_of(table.entries, [&](const auto &entry) { return entry.name == alias.alias; }))
                return Invalid(error, "Duplicate or conflicting parameter alias");
            const auto count = std::ranges::count(table.entries, alias.target, &ShaderParameterEntry::name);
            if (count != 1)
                return Invalid(error, "Parameter alias target is unknown or ambiguous");
            table.aliases.push_back(alias);
            Mix(table.generation, alias.alias);
            Mix(table.generation, alias.target);
        }
        out = std::move(table);
        return true;
    }

    namespace {
        ShaderResourceType ParameterBaseType(ShaderResourceType type)
        {
            return type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC   ? ShaderResourceType::UNIFORM_BUFFER
                   : type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC ? ShaderResourceType::STORAGE_BUFFER
                                                                        : type;
        }
        const ShaderResource *ParameterResource(const ShaderReflection &reflection, uint32_t set, uint32_t binding, bool active)
        {
            const auto &resources = active ? reflection.resources : reflection.declarations;
            const auto  found =
                std::ranges::find_if(resources, [&](const auto &resource) { return resource.set == set && resource.binding == binding; });
            return found == resources.end() ? nullptr : &*found;
        }
        const ShaderBlockLayout *ParameterBlock(const ShaderReflection &reflection, uint32_t set, uint32_t binding, bool active)
        {
            const auto &blocks = active ? reflection.blocks : reflection.declaredBlocks;
            const auto  found  = std::ranges::find_if(blocks, [&](const auto &block) { return block.set == set && block.binding == binding; });
            return found == blocks.end() ? nullptr : &*found;
        }
    } // namespace

    ShaderParameterTransaction::ShaderParameterTransaction(const Shader &source, uint32_t targetSet, const ShaderParameterTransactionState &initial)
        : shader(source), set(targetSet), staged(initial)
    {
        if (!shader.GetParameterTable().generation)
            Fail("Parameter transaction requires an initialized shader parameter table");
    }

    bool ShaderParameterTransaction::Open() const noexcept
    {
        return !ended && error.empty();
    }
    bool ShaderParameterTransaction::Fail(std::string message)
    {
        if (error.empty())
            error = std::move(message);
        return false;
    }
    bool ShaderParameterTransaction::Resolve(Name name, ShaderParameterHandle &handle)
    {
        if (!Open())
            return Fail("Parameter transaction is closed or failed");
        const auto status = shader.GetParameterTable().Resolve(name, set, handle);
        if (status != ShaderParameterLookup::READY && status != ShaderParameterLookup::INACTIVE)
            return Fail("Unknown or ambiguous parameter name");
        return true;
    }
    const ShaderParameterEntry *ShaderParameterTransaction::Resource(Name name)
    {
        ShaderParameterHandle handle;
        if (!Resolve(name, handle))
            return nullptr;
        const ShaderParameterEntry *entry = nullptr;
        shader.GetParameterTable().Resolve(handle, entry);
        if (!entry || entry->kind != ShaderParameterKind::RESOURCE) {
            Fail("Parameter name does not identify a resource");
            return nullptr;
        }
        return entry;
    }
    bool ShaderParameterTransaction::Touch(uint32_t binding, const std::string &field, bool whole)
    {
        if (!Open() || wholeBlocks.contains(binding) || (whole && touchedBindings.contains(binding)) || !touched.emplace(binding, field).second)
            return Fail("Duplicate parameter target or overlapping block and field update");
        if (whole)
            wholeBlocks.insert(binding);
        touchedBindings.insert(binding);
        return true;
    }
    ShaderParameterWriteResult ShaderParameterTransaction::WriteData(Name name, const ShaderDataValue &value)
    {
        ShaderParameterHandle handle;
        return Resolve(name, handle) ? WriteData(handle, value) : ShaderParameterWriteResult{};
    }
    ShaderParameterWriteResult ShaderParameterTransaction::WriteData(ShaderParameterHandle handle, const ShaderDataValue &value)
    {
        const ShaderParameterEntry *entry  = nullptr;
        const auto                  lookup = shader.GetParameterTable().Resolve(handle, entry);
        if (!Open() || handle.set != set || (lookup != ShaderParameterLookup::READY && lookup != ShaderParameterLookup::INACTIVE) || !entry) {
            Fail("Stale or out-of-scope parameter handle");
            return {};
        }
        if (entry->kind != ShaderParameterKind::FIELD || entry->dataType != value.type || entry->arrayCount != value.count ||
            !ValidateShaderData(value, error)) {
            Fail("Shader data semantic type does not match the named field");
            return {};
        }
        if (!Touch(entry->binding, entry->field, false))
            return {};
        if (lookup == ShaderParameterLookup::INACTIVE)
            return {ShaderParameterWriteStatus::INACTIVE};
        staged.resources.erase({entry->binding, 0});
        staged.fields[entry->binding][entry->field] = value;
        return {ShaderParameterWriteStatus::SUCCESS};
    }
    ShaderParameterWriteResult ShaderParameterTransaction::WriteBlock(Name name, const ShaderCpuSchema &schema, std::span<const std::byte> bytes)
    {
        const auto *entry = Resource(name);
        if (!entry || ParameterBaseType(entry->resourceType) != ShaderResourceType::UNIFORM_BUFFER) {
            Fail("WriteBlock requires a named uniform block");
            return {};
        }
        const auto *block = ParameterBlock(shader.GetReflection(), set, entry->binding, entry->active);
        if (!block || bytes.size() != schema.size) {
            Fail("CPU block schema or byte range is incompatible");
            return {};
        }
        if (entry->active) {
            if (!shader.GetUniformPlan(set, entry->binding, schema, error)) {
                Fail("CPU block schema is incompatible");
                return {};
            }
        } else {
            UniformUploadPlan checked;
            if (!BuildUniformUploadPlan(schema, *block, shader.GetParameterTable().generation, checked, error)) {
                Fail("CPU block schema is incompatible");
                return {};
            }
        }
        if (!Touch(entry->binding, {}, true))
            return {};
        if (!entry->active)
            return {ShaderParameterWriteStatus::INACTIVE};
        std::map<std::string, ShaderDataValue> fields;
        for (const auto &field : schema.fields) {
            ShaderDataValue value{.type = ShaderMemberDataType(field), .count = field.arrayCount, .matrixLayout = field.matrixLayout};
            value.bytes.resize(DataSize(value.type, value.count));
            if (value.type == ShaderDataType::MAT4 || value.type == ShaderDataType::FLOAT4_ARRAY) {
                const auto count  = value.type == ShaderDataType::MAT4 ? 4u : value.count;
                const auto stride = value.type == ShaderDataType::MAT4 ? field.matrixStride : field.arrayStride;
                for (uint32_t i = 0; i < count; ++i)
                    std::memcpy(value.bytes.data() + i * 16, bytes.data() + field.offset + i * stride, 16);
            } else
                std::memcpy(value.bytes.data(), bytes.data() + field.offset, value.bytes.size());
            fields.emplace(field.name, std::move(value));
        }
        staged.resources.erase({entry->binding, 0});
        staged.fields[entry->binding] = std::move(fields);
        return {ShaderParameterWriteStatus::SUCCESS};
    }
    ShaderParameterWriteResult ShaderParameterTransaction::WriteResource(Name name, ShaderResourceType type, uint32_t token, uint32_t arrayElement)
    {
        const auto *entry    = Resource(name);
        const auto *resource = entry ? ParameterResource(shader.GetReflection(), set, entry->binding, entry->active) : nullptr;
        if (!resource || ParameterBaseType(resource->type) != ParameterBaseType(type) || arrayElement >= resource->count || token == UINT32_MAX) {
            Fail("Invalid named resource semantic type, element or token");
            return {};
        }
        const bool uniform = ParameterBaseType(type) == ShaderResourceType::UNIFORM_BUFFER;
        if (!Touch(entry->binding, uniform ? std::string{} : "resource:" + std::to_string(arrayElement), uniform))
            return {};
        if (!entry->active)
            return {ShaderParameterWriteStatus::INACTIVE};
        if (uniform)
            staged.fields.erase(entry->binding);
        staged.resources[{entry->binding, arrayElement}] = {entry->binding, arrayElement, token, resource->type};
        return {ShaderParameterWriteStatus::SUCCESS};
    }
    bool ValidateShaderParameterState(const Shader &shader, uint32_t set, const ShaderParameterTransactionState &state, std::string &error)
    {
        if (!shader.GetParameterTable().generation)
            return Invalid(error, "Parameter transaction requires an initialized shader parameter table");
        const auto &reflection     = shader.GetReflection();
        uint64_t    resourceCount  = 0;
        size_t      fieldProviders = 0, resourceProviders = 0;
        for (const auto &resource : reflection.resources) {
            if (resource.set != set)
                continue;
            if (!resource.count || resource.count > 65536 || resourceCount + resource.count > 65536)
                return Invalid(error, "Parameter descriptor count exceeds the transaction limit");
            resourceCount += resource.count;
            const bool uniform = ParameterBaseType(resource.type) == ShaderResourceType::UNIFORM_BUFFER;
            const auto fields  = state.fields.find(resource.binding);
            if (uniform && !state.resources.contains({resource.binding, 0})) {
                const auto *block = ParameterBlock(reflection, set, resource.binding, true);
                if (!block || resource.count != 1 || fields == state.fields.end() || fields->second.empty() ||
                    fields->second.size() != block->members.size())
                    return Invalid(error, "Required uniform block has incomplete values");
                for (const auto &member : block->members) {
                    const auto value = fields->second.find(member.name);
                    if (value == fields->second.end() || value->second.type != ShaderMemberDataType(member) ||
                        value->second.count != member.arrayCount)
                        return Invalid(error, "Required uniform field has an incompatible type or array count");
                    if (!ValidateShaderData(value->second, error))
                        return false;
                }
                ++fieldProviders;
                continue;
            }
            if (uniform && fields != state.fields.end() && !fields->second.empty())
                return Invalid(error, "Uniform resource and typed field providers overlap");
            for (uint32_t element = 0; element < resource.count; ++element) {
                const auto value = state.resources.find({resource.binding, element});
                if (value == state.resources.end() || value->second.binding != resource.binding || value->second.element != element ||
                    ParameterBaseType(value->second.type) != ParameterBaseType(resource.type) || value->second.token == UINT32_MAX)
                    return Invalid(error, "Required parameter resource is missing or has an incompatible type");
                ++resourceProviders;
            }
        }
        if (fieldProviders != state.fields.size())
            return Invalid(error, "Parameter state contains an unused field provider");
        if (resourceProviders != state.resources.size())
            return Invalid(error, "Parameter state contains an unused resource provider");
        return true;
    }
    bool ShaderParameterTransaction::Complete()
    {
        if (!Open())
            return Fail("Parameter transaction is closed or failed");
        return ValidateShaderParameterState(shader, set, staged, error);
    }
    bool ShaderParameterTransaction::Finalize(std::shared_ptr<const ShaderParameterPacket> &out)
    {
        if (!Complete())
            return false;
        auto packet        = std::make_shared<ShaderParameterPacket>();
        packet->generation = shader.GetParameterTable().generation;
        packet->set        = set;
        for (const auto &resource : shader.GetReflection().resources) {
            if (resource.set != set)
                continue;
            if (ParameterBaseType(resource.type) == ShaderResourceType::UNIFORM_BUFFER && !staged.resources.contains({resource.binding, 0})) {
                const auto                                          *block  = ParameterBlock(shader.GetReflection(), set, resource.binding, true);
                const auto                                          &fields = staged.fields.at(resource.binding);
                std::vector<std::pair<std::string, ShaderDataValue>> values(fields.begin(), fields.end());
                ShaderParameterUniformWrite                          uniform{
                                             .binding = resource.binding, .type = resource.type, .alignment = std::max(256u, block->alignment)};
                if (!shader.PackUniformData(set, resource.binding, values, uniform.bytes, error))
                    return Fail("Uniform parameter encoding failed");
                packet->uniforms.push_back(std::move(uniform));
            } else {
                for (uint32_t element = 0; element < resource.count; ++element) {
                    auto value = staged.resources.at({resource.binding, element});
                    value.type = resource.type;
                    packet->resources.push_back(value);
                }
            }
        }
        out   = std::move(packet);
        ended = true;
        return true;
    }
    void ShaderParameterTransaction::Cancel()
    {
        ended  = true;
        staged = {};
        Fail("Parameter transaction was canceled");
    }

} // namespace sky::aurora
