//
// Created by Zach Lee on 2022/1/30.
//

#pragma once
#include <core/math/Vector.h>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <framework/asset/Asset.h>
#include <core/util/Uuid.h>

namespace glm {
    template<class Archive>
    void serialize(Archive &ar, glm::vec4 &s)
    {
        ar(s.x, s.y, s.z, s.w);
    }

    template<class Archive>
    void serialize(Archive &ar, glm::vec3 &s)
    {
        ar(s.x, s.y, s.z);
    }

    template<class Archive>
    void serialize(Archive &ar, glm::vec2 &s)
    {
        ar(s.x, s.y);
    }
}

namespace sky {
    template<class Archive>
    void serialize(Archive &ar, Uuid& s)
    {
        ar(s.ToString());
    }
}

namespace cereal {
    template <class Archive>
    struct specialize<Archive, sky::Vector4, cereal::specialization::non_member_serialize> {};

    template <class Archive>
    struct specialize<Archive, sky::Vector3, cereal::specialization::non_member_serialize> {};

    template <class Archive>
    struct specialize<Archive, sky::Vector2, cereal::specialization::non_member_serialize> {};

    template<class Archive>
    struct specialize<Archive, sky::Uuid, cereal::specialization::non_member_serialize> {};
}
