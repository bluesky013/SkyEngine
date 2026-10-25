#pragma once

#include <core/math/Matrix4.h>
#include <core/util/Uuid.h>
#include <string>
#include <vector>

namespace sky::aurora {
    enum class SkeletalPoseState : uint8_t { Pending, Ready, Error };
    struct SkeletalPoseIdentity {
        const void *world = nullptr;
        Uuid        worldEpoch;
        Uuid        actor;
        Uuid        generation;
    };
    struct SkeletalRigVersion {
        Uuid     asset;
        Uuid     generation;
        uint64_t revision                                     = 0;
        uint64_t layout                                       = 0;
        bool     operator==(const SkeletalRigVersion &) const = default;
    };
    struct SkeletalPoseResult {
        SkeletalPoseState    state = SkeletalPoseState::Pending;
        std::string          diagnostic;
        SkeletalPoseIdentity instance;
        SkeletalRigVersion   skeleton;
        uint64_t             revision        = 0;
        bool                 retainedVersion = false;
        std::vector<Matrix4> models;
    };
    class ISkeletalPoseSource {
    public:
        virtual ~ISkeletalPoseSource() = default;
        // Main-thread discovery borrows world identity; returned matrices own their storage.
        virtual SkeletalPoseResult Describe(const void *world, const Uuid &actor) const                                  = 0;
        virtual SkeletalPoseResult Query(const SkeletalPoseIdentity &instance, const SkeletalRigVersion &skeleton) const = 0;
    };
} // namespace sky::aurora
