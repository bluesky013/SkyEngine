//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/environment/Singleton.h>

namespace sky::ai {

    class NaviMapData {
    public:
        NaviMapData() = default;
        virtual ~NaviMapData() = default;
    };

    class NaviMap {
    public:
        NaviMap() = default;
        virtual ~NaviMap() = default;
    };

    class NaviMapFactory : public Singleton<NaviMapFactory> {
    public:
        NaviMapFactory() = default;
        ~NaviMapFactory() override = default;

        NaviMap* CreateNaviMap();

        class Impl {
        public:
            Impl() = default;
            virtual ~Impl() = default;

            virtual NaviMap* CreateNaviMap() = 0;
        };

    private:
        std::unique_ptr<Impl> factory;
    };

} // namespace sky::ai
