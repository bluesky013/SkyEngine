//
// Skin: mesh-side skinning data, self-contained. Holds the inverse bind
// matrices (bind pose) and the evaluated bone matrix palette uploaded to the
// GPU, plus an optional vertex-bone-slot -> bone-index mapping.
//
// Animation is an independent module (engine/animation, target `Animation`):
// aurora does NOT define or depend on a Skeleton type. The bridge/adaptor layer
// maps an animation skeleton/pose into this Skin's `boneMatrices`.
//

#pragma once

#include <core/math/Matrix4.h>
#include <core/template/ReferenceObject.h>

#include <utility>
#include <vector>

namespace sky::aurora {

    class Skin : public RefObject {
    public:
        Skin() = default;
        ~Skin() override = default;

        Skin(const Skin &) = delete;
        Skin &operator=(const Skin &) = delete;

        void SetInverseBindMatrices(std::vector<Matrix4> matrices) { inverseBindMatrices = std::move(matrices); }
        const std::vector<Matrix4> &GetInverseBindMatrices() const { return inverseBindMatrices; }

        void SetBoneMatrices(std::vector<Matrix4> matrices) { boneMatrices = std::move(matrices); }
        const std::vector<Matrix4> &GetBoneMatrices() const { return boneMatrices; }

        void SetBoneMapping(std::vector<uint32_t> mapping) { boneMapping = std::move(mapping); }
        const std::vector<uint32_t> &GetBoneMapping() const { return boneMapping; }

    private:
        std::vector<Matrix4>  inverseBindMatrices;
        std::vector<Matrix4>  boneMatrices;
        std::vector<uint32_t> boneMapping;
    };

} // namespace sky::aurora
