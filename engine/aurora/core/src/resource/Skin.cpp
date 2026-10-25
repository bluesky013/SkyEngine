#include <aurora/resource/Skin.h>

#include <array>
#include <cmath>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace sky::aurora {

    namespace {
        bool ValidMatrix(const Matrix4 &matrix)
        {
            for (const float value : matrix.v) {
                if (!std::isfinite(value)) {
                    return false;
                }
            }
            const float determinant = matrix.Determinant();
            return std::isfinite(determinant) && determinant != 0.f && matrix[0][3] == 0.f && matrix[1][3] == 0.f && matrix[2][3] == 0.f &&
                   matrix[3][3] == 1.f;
        }

        bool ValidName(const std::string &name)
        {
            return !name.empty() && name.size() <= 4096 && name.find('\0') == std::string::npos;
        }

        bool ReferenceMatches(const Matrix4 &model, const Matrix4 &inverseBind)
        {
            const Matrix4 product = model * inverseBind;
            for (uint32_t column = 0; column < 4; ++column) {
                for (uint32_t row = 0; row < 4; ++row) {
                    const float expected = column == row ? 1.f : 0.f;
                    if (!std::isfinite(product[column][row]) || std::abs(product[column][row] - expected) > 1e-4f) {
                        return false;
                    }
                }
            }
            return true;
        }

        std::string_view Leaf(const std::string &path)
        {
            const auto separator = path.find_last_of('/');
            return separator == std::string::npos ? std::string_view(path) : std::string_view(path).substr(separator + 1);
        }
    } // namespace

    Skin::Skin(SkinData value) : data(std::move(value))
    {
    }

    SkinStatus Skin::Validate(const SkinData &data)
    {
        if (data.binds.empty() || data.binds.size() > 256 || data.binds.size() != data.input.slots.size() || data.input.modelSpaceSignature == 0) {
            return {SkinError::InvalidData, "Skin requires 1..256 slots, matching binds and a model-space signature"};
        }
        if (data.mode != SkinBindingMode::Named && data.mode != SkinBindingMode::Indexed) {
            return {SkinError::InvalidData, "unknown Skin binding mode"};
        }
        if (data.mode == SkinBindingMode::Indexed && data.targetLayoutSignature == 0) {
            return {SkinError::InvalidData, "indexed Skin requires a target layout signature"};
        }
        std::unordered_set<std::string> slots;
        for (size_t i = 0; i < data.binds.size(); ++i) {
            if (!ValidName(data.input.slots[i]) || !slots.emplace(data.input.slots[i]).second || !ValidMatrix(data.binds[i].inverseBind)) {
                return {SkinError::InvalidData, "Skin input slots must be unique and inverse binds finite, affine and invertible"};
            }
            if ((data.mode == SkinBindingMode::Named && !ValidName(data.binds[i].targetBone)) ||
                (data.mode == SkinBindingMode::Indexed && data.binds[i].targetIndex >= 256)) {
                return {SkinError::InvalidData, "invalid Skin target bone"};
            }
        }
        return {};
    }

    CounterPtr<Skin> Skin::Create(SkinData data, SkinStatus *status)
    {
        const auto validation = Validate(data);
        if (status != nullptr) {
            *status = validation;
        }
        return validation ? CounterPtr<Skin>(new Skin(std::move(data))) : CounterPtr<Skin>{};
    }

    SkinStatus ResolveSkinBinding(
        const SkinInputLayout &meshInput, const Skin &skin, const SkinTargetLayout &target, const SkinBindingKey &key, ResolvedSkinBindingPtr &out)
    {
        const auto &data = skin.GetData();
        if (meshInput != data.input) {
            return {SkinError::LayoutMismatch, "Mesh and Skin input slot/model-space layouts differ"};
        }
        if (target.paths.empty() || target.paths.size() > 256 || target.paths.size() != target.referenceModels.size() || target.signature == 0 ||
            key.skeletonLayout != target.signature || !key.mesh || !key.skin || !key.skeleton || key.meshRevision == 0 || key.skinRevision == 0 ||
            key.skeletonRevision == 0 || !key.meshGeneration || !key.skinGeneration || !key.skeletonGeneration) {
            return {SkinError::InvalidData, "binding requires complete resource revisions and a valid target layout"};
        }
        if (data.mode == SkinBindingMode::Indexed && data.targetLayoutSignature != target.signature) {
            return {SkinError::LayoutMismatch, "indexed Skin target layout changed"};
        }
        std::unordered_set<std::string> paths;
        for (size_t i = 0; i < target.paths.size(); ++i) {
            if (!ValidName(target.paths[i]) || !paths.emplace(target.paths[i]).second || !ValidMatrix(target.referenceModels[i])) {
                return {SkinError::InvalidData, "target paths must be unique and reference matrices finite, affine and invertible"};
            }
        }
        auto resolved             = std::make_shared<ResolvedSkinBinding>();
        resolved->key             = key;
        resolved->targetBoneCount = static_cast<uint32_t>(target.paths.size());
        resolved->slotToBone.reserve(data.binds.size());
        resolved->inverseBindMatrices.reserve(data.binds.size());
        for (const auto &bind : data.binds) {
            uint32_t index = bind.targetIndex;
            if (data.mode == SkinBindingMode::Named) {
                index                = UINT32_MAX;
                uint32_t   matches   = 0;
                const bool canonical = bind.targetBone.find('/') != std::string::npos;
                for (uint32_t i = 0; i < target.paths.size(); ++i) {
                    if ((canonical ? std::string_view(target.paths[i]) : Leaf(target.paths[i])) == bind.targetBone) {
                        index = i;
                        ++matches;
                    }
                }
                if (matches > 1) {
                    return {SkinError::AmbiguousBone, "Skin target bone is ambiguous: " + bind.targetBone};
                }
            }
            if (index >= target.paths.size()) {
                return {SkinError::MissingBone, "Skin target bone is absent or out of range"};
            }
            if (!ReferenceMatches(target.referenceModels[index], bind.inverseBind)) {
                return {SkinError::ReferenceMismatch, "target reference pose does not match Skin inverse bind"};
            }
            resolved->slotToBone.push_back(static_cast<uint16_t>(index));
            resolved->inverseBindMatrices.push_back(bind.inverseBind);
        }
        out = std::move(resolved);
        return {};
    }

    bool SkinBindingCache::KeyLess::operator()(const SkinBindingKey &lhs, const SkinBindingKey &rhs) const
    {
        return std::tie(lhs.mesh, lhs.skin, lhs.skeleton, lhs.meshRevision, lhs.skinRevision, lhs.skeletonRevision, lhs.skeletonLayout,
                        lhs.meshGeneration, lhs.skinGeneration,
                        lhs.skeletonGeneration) < std::tie(rhs.mesh, rhs.skin, rhs.skeleton, rhs.meshRevision, rhs.skinRevision, rhs.skeletonRevision,
                                                           rhs.skeletonLayout, rhs.meshGeneration, rhs.skinGeneration, rhs.skeletonGeneration);
    }

    SkinStatus SkinBindingCache::Resolve(
        const SkinInputLayout &meshInput, const Skin &skin, const SkinTargetLayout &target, const SkinBindingKey &key, ResolvedSkinBindingPtr &out)
    {
        if (meshInput != skin.GetData().input || key.skeletonLayout != target.signature) {
            return {SkinError::LayoutMismatch, "binding input does not match its resource revision key"};
        }
        const auto entry = entries.find(key);
        if (entry != entries.end()) {
            if (const auto binding = entry->second.lock(); binding) {
                out = binding;
                return {};
            }
        }
        ResolvedSkinBindingPtr binding;
        if (const auto status = ResolveSkinBinding(meshInput, skin, target, key, binding); !status) {
            return status;
        }
        PruneExpired();
        entries[key] = binding;
        out          = std::move(binding);
        return {};
    }

    void SkinBindingCache::PruneExpired()
    {
        std::erase_if(entries, [](const auto &entry) { return entry.second.expired(); });
    }

    SkinStatus BuildSkinningPalette(const ResolvedSkinBinding &binding, std::span<const Matrix4> models, std::vector<Matrix4> &out)
    {
        if (models.size() != binding.targetBoneCount || binding.slotToBone.empty() || binding.slotToBone.size() > 256 ||
            binding.slotToBone.size() != binding.inverseBindMatrices.size()) {
            return {SkinError::InvalidData, "palette pose and binding sizes differ"};
        }
        std::array<Matrix4, 256> candidate;
        for (size_t i = 0; i < binding.slotToBone.size(); ++i) {
            const auto bone = binding.slotToBone[i];
            if (bone >= models.size() || !ValidMatrix(models[bone])) {
                return {SkinError::InvalidData, "palette contains a missing, nonfinite or singular joint"};
            }
            const Matrix4 palette = models[bone] * binding.inverseBindMatrices[i];
            if (!ValidMatrix(palette)) {
                return {SkinError::InvalidData, "palette transform is nonfinite or singular"};
            }
            candidate[i] = palette;
        }
        out.assign(candidate.begin(), candidate.begin() + binding.slotToBone.size());
        return {};
    }

    SkinStatus BuildSkinnedBounds(std::span<const SkinSlotBounds> slots, std::span<const Matrix4> palette, AABB &out)
    {
        if (slots.empty() || slots.size() != palette.size() || slots.size() > 256) {
            return {SkinError::InvalidData, "Skin bounds and palette sizes differ"};
        }
        AABB candidate;
        bool initialized = false;
        for (size_t i = 0; i < slots.size(); ++i) {
            if (!ValidMatrix(palette[i])) {
                return {SkinError::InvalidData, "Skin bounds require finite invertible transforms"};
            }
            if (!slots[i].occupied) {
                continue;
            }
            const auto box = AABB::Transform(slots[i].bounds, palette[i]);
            for (uint32_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(box.min[axis]) || !std::isfinite(box.max[axis]) || box.min[axis] > box.max[axis] ||
                    !std::isfinite(slots[i].bounds.min[axis]) || !std::isfinite(slots[i].bounds.max[axis]) ||
                    slots[i].bounds.min[axis] > slots[i].bounds.max[axis]) {
                    return {SkinError::InvalidData, "Skin slot bounds are invalid or overflowed"};
                }
                candidate.min[axis] = initialized ? std::min(candidate.min[axis], box.min[axis]) : box.min[axis];
                candidate.max[axis] = initialized ? std::max(candidate.max[axis], box.max[axis]) : box.max[axis];
            }
            initialized = true;
        }
        if (!initialized) {
            return {SkinError::InvalidData, "Skin has no occupied bounds"};
        }
        out = candidate;
        return {};
    }
} // namespace sky::aurora
