//
// Created by Zach Lee on 2021/12/23.
//

#pragma once

#include <engine/world/Component.h>

namespace sky {

    template <typename T>
    class SHandle {
    public:
        using Type                           = T;
        static constexpr uint32_t INVALID_ID = ~(0u);
        SHandle() : index(INVALID_ID)
        {
        }
        SHandle(uint32_t hnd) : index(hnd)
        {
        }
        ~SHandle() = default;

        operator bool() const
        {
            return index != INVALID_ID;
        }

        void Reset()
        {
            index = INVALID_ID;
        }

        uint32_t GetIndex() const
        {
            return index;
        }

    private:
        uint32_t index = INVALID_ID;
    };

    class IService {
    public:
        IService()          = default;
        virtual ~IService() = default;

        virtual void OnTick(float time)
        {
        }
    };

} // namespace sky