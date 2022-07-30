//
// Created by Zach Lee on 2022/7/19.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/type/Type.h>
#include <render/shapes/RenderShape.h>

namespace sky {

    class ShapeManager : public Singleton<ShapeManager> {
    public:

        template <typename T>
        RDShaperPtr GetOrCreate()
        {
            auto id = TypeInfo<T>::Hash();
            auto iter = shapes.find(id);
            if (iter != shapes.end()) {
                return iter->second;
            }

            static_assert(std::is_base_of_v<RenderShape, T>);
            auto res = std::make_shared<T>();
            res->Init();
            shapes.emplace(id, res);
            return res;
        }

        void Clear();

    private:
        friend class Singleton<ShapeManager>;
        ShapeManager() = default;
        ~ShapeManager() = default;

        std::unordered_map<uint32_t, RDShaperPtr> shapes;
    };
}