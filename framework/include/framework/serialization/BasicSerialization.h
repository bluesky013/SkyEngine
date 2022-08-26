//
// Created by Zach Lee on 2022/1/30.
//

#pragma once
#include <core/math/Vector.h>
#include <core/math/Matrix.h>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/memory.hpp>
#include <core/util/Uuid.h>

namespace glm {
    template<class Archive>
    void serialize(Archive &ar, glm::mat4 &s)
    {
        ar(s[0][0], s[0][1], s[0][2], s[0][3],
           s[1][0], s[1][1], s[1][2], s[1][3],
           s[2][0], s[2][1], s[2][2], s[2][3],
           s[3][0], s[3][1], s[3][2], s[3][3]);
    }

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

namespace cereal {
    template <class Archive>
    struct specialize<Archive, sky::Vector4, cereal::specialization::non_member_serialize> {};

    template <class Archive>
    struct specialize<Archive, sky::Vector3, cereal::specialization::non_member_serialize> {};

    template <class Archive>
    struct specialize<Archive, sky::Vector2, cereal::specialization::non_member_serialize> {};
}
