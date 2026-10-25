#pragma once

#include <core/math/Matrix4.h>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>
#include <core/util/Uuid.h>

#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sky::aurora {

    enum class SkinError : uint8_t {
        None,
        InvalidData,
        LayoutMismatch,
        MissingBone,
        AmbiguousBone,
        ReferenceMismatch,
        UnsupportedVersion,
    };

    struct SkinStatus {
        SkinError   error = SkinError::None;
        std::string message;
        explicit    operator bool() const
        {
            return error == SkinError::None;
        }
    };

    enum class SkinBindingMode : uint8_t { Named, Indexed };

    struct SkinInputLayout {
        uint64_t                 modelSpaceSignature = 0;
        std::vector<std::string> slots;
        bool                     operator==(const SkinInputLayout &) const = default;
    };

    struct SkinBind {
        Matrix4     inverseBind = Matrix4::Identity();
        std::string targetBone;
        uint16_t    targetIndex = 0;
    };

    struct SkinData {
        SkinInputLayout       input;
        SkinBindingMode       mode                  = SkinBindingMode::Named;
        uint64_t              targetLayoutSignature = 0;
        std::vector<SkinBind> binds;
    };

    struct SkinSlotBounds {
        bool occupied = false;
        AABB bounds;
    };

    struct SkinTargetLayout {
        uint64_t                 signature = 0;
        std::vector<std::string> paths;
        std::vector<Matrix4>     referenceModels;
    };

    struct SkinBindingKey {
        Uuid     mesh;
        Uuid     skin;
        Uuid     skeleton;
        uint64_t meshRevision     = 0;
        uint64_t skinRevision     = 0;
        uint64_t skeletonRevision = 0;
        uint64_t skeletonLayout   = 0;
        Uuid     meshGeneration;
        Uuid     skinGeneration;
        Uuid     skeletonGeneration;
        bool     operator==(const SkinBindingKey &) const = default;
    };

    struct ResolvedSkinBinding {
        SkinBindingKey        key;
        uint32_t              targetBoneCount = 0;
        std::vector<uint16_t> slotToBone;
        std::vector<Matrix4>  inverseBindMatrices;
    };

    using ResolvedSkinBindingPtr = std::shared_ptr<const ResolvedSkinBinding>;

    class Skin : public RefObject {
    public:
        ~Skin() override = default;

        Skin(const Skin &)            = delete;
        Skin &operator=(const Skin &) = delete;

        static SkinStatus       Validate(const SkinData &data);
        static CounterPtr<Skin> Create(SkinData data, SkinStatus *status = nullptr);
        const SkinData         &GetData() const
        {
            return data;
        }

    private:
        explicit Skin(SkinData value);
        SkinData data;
    };

    SkinStatus ResolveSkinBinding(
        const SkinInputLayout &meshInput, const Skin &skin, const SkinTargetLayout &target, const SkinBindingKey &key, ResolvedSkinBindingPtr &out);

    // Main-thread cache. Keys identify immutable resource revisions, never mutable instance poses.
    class SkinBindingCache {
    public:
        SkinStatus Resolve(const SkinInputLayout  &meshInput,
                           const Skin             &skin,
                           const SkinTargetLayout &target,
                           const SkinBindingKey   &key,
                           ResolvedSkinBindingPtr &out);
        void       PruneExpired();
        void       Clear()
        {
            entries.clear();
        }

    private:
        struct KeyLess {
            bool operator()(const SkinBindingKey &lhs, const SkinBindingKey &rhs) const;
        };
        std::map<SkinBindingKey, std::weak_ptr<const ResolvedSkinBinding>, KeyLess> entries;
    };

    // Output is instance/frame-owned; the shared Skin never stores evaluated matrices.
    // Models must use the skeleton revision and order recorded in binding.key.
    SkinStatus BuildSkinningPalette(const ResolvedSkinBinding &binding, std::span<const Matrix4> models, std::vector<Matrix4> &out);
    SkinStatus BuildSkinnedBounds(std::span<const SkinSlotBounds> slots, std::span<const Matrix4> palette, AABB &out);

} // namespace sky::aurora
