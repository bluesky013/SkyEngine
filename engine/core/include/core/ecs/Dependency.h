#pragma once

#include <compare>
#include <core/ecs/TypeId.h>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sky {

    using SystemId = uint64_t;

    struct AccessOwner {
        uint64_t instance                               = 0;
        uint64_t generation                             = 0;
        auto     operator<=>(const AccessOwner &) const = default;
    };

    enum class AccessKind : uint8_t { COMPONENT, RESOURCE };
    enum class AccessMode : uint8_t { READ, WRITE };

    // Identity describes actual storage; frame sequence must not disguise mutable aliases.
    struct AccessKey {
        std::string domain;
        AccessOwner owner;
        AccessKind  kind                                 = AccessKind::COMPONENT;
        uint64_t    identity                             = 0;
        auto        operator<=>(const AccessKey &) const = default;
    };

    struct AccessDesc {
        AccessKey   key;
        AccessMode  mode = AccessMode::READ;
        std::string tag;
        bool        operator==(const AccessDesc &) const = default;
    };

    template <typename T>
    AccessDesc ComponentAccess(std::string domain, AccessOwner owner, AccessMode mode)
    {
        return {{std::move(domain), owner, AccessKind::COMPONENT, TypeId<T>()}, mode, std::string(TypeTag<T>())};
    }

    inline AccessDesc ResourceAccess(std::string domain, AccessOwner owner, std::string name, AccessMode mode)
    {
        const auto identity = Fnv1a64(name);
        return {{std::move(domain), owner, AccessKind::RESOURCE, identity}, mode, std::move(name)};
    }

    struct SystemDesc {
        SystemId                id = 0;
        std::string             name;
        uint32_t                phase = 0;
        int32_t                 order = 0;
        std::vector<AccessDesc> accesses;
        std::vector<SystemId>   before;
        std::vector<SystemId>   after;
        bool                    operator==(const SystemDesc &) const = default;
    };

    enum class DependencyOrigin : uint8_t { PHASE, BEFORE, AFTER, RAW, WAR, WAW };

    struct DependencyEdge {
        uint32_t                 from   = 0;
        uint32_t                 to     = 0;
        DependencyOrigin         origin = DependencyOrigin::PHASE;
        std::optional<AccessKey> access;
        auto                     operator<=>(const DependencyEdge &) const = default;
    };

    // Systems are in stable semantic order; edges refer to indices in this owned array.
    struct DependencyPlan {
        std::vector<SystemDesc>     systems;
        std::vector<DependencyEdge> edges;
        bool                        operator==(const DependencyPlan &) const = default;
    };

    enum class DependencyErrorCode : uint8_t {
        INVALID_SYSTEM,
        DUPLICATE_SYSTEM,
        INVALID_ACCESS,
        TAG_COLLISION,
        UNKNOWN_SYSTEM,
        PHASE_CONFLICT,
        CYCLE
    };

    struct DependencyIssueEdge {
        SystemId         from   = 0;
        SystemId         to     = 0;
        DependencyOrigin origin = DependencyOrigin::PHASE;
    };

    struct DependencyDiagnostic {
        DependencyErrorCode              code = DependencyErrorCode::INVALID_SYSTEM;
        std::string                      message;
        std::vector<DependencyIssueEdge> edges;
        std::optional<AccessKey>         access;
    };

    using DependencyCompileResult = std::variant<DependencyPlan, DependencyDiagnostic>;

    // Explicit order is resolved before hazards; Read/Write alone does not imply producer intent.
    DependencyCompileResult CompileDependencyPlan(std::span<const SystemDesc> systems);

} // namespace sky
