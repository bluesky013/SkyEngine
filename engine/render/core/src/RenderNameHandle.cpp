//
// Created by Zach Lee on 2023/9/5.
//

#include <render/RenderNameHandle.h>

namespace sky {

    uint32_t RenderNameHandle::GetOrRegisterName(const std::string &key)
    {
        auto iter = nameHandleMap.find(key);
        if (iter == nameHandleMap.end()) {
            std::lock_guard<std::mutex> lock(mutex);
            iter = nameHandleMap.find(key);
            if (iter == nameHandleMap.end()) {
                iter = nameHandleMap.emplace(key, static_cast<uint32_t>(nameHandleMap.size() + 1)).first; // avoid 0
            }
        }
        return iter->second;
    }

} // namespace sky
