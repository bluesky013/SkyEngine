//
// Created by Zach Lee on 2023/9/5.
//

#pragma once

#include <unordered_map>
#include <mutex>
#include <string>
#include <core/environment/Singleton.h>

namespace sky {

    class RenderNameHandle : public Singleton<RenderNameHandle> {
    public:
        RenderNameHandle() = default;
        ~RenderNameHandle() override = default;

        uint32_t GetOrRegisterName(const std::string &key);

    private:
        mutable std::mutex mutex;
        std::unordered_map<std::string, uint32_t> nameHandleMap;
    };

} // namespace sky
