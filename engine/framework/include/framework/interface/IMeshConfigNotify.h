//
// Created by blues on 2024/10/3.
//

#pragma once

#include <core/event/Event.h>
#include <core/util/Uuid.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct MeshConfigBase {
        MeshConfigBase() = default;
        virtual ~MeshConfigBase() = default;
    };

    class IMeshConfigNotify : public EventTraits {
    public:
        IMeshConfigNotify() = default;
        virtual ~IMeshConfigNotify() = default;

        using KeyType   = void;
        using MutexType = void;

        virtual void GatherConfigTypes(std::set<Uuid> &typeId) = 0;
    };
    using MeshConfigNotify = Event<IMeshConfigNotify>;

} // namespace sky