//
// Created by Zach Lee on 2021/12/26.
//

#pragma once

#include <unordered_map>
#include <set>
#include <list>
#include <string>

namespace sky {

    class DeferredRendering {
    public:
        DeferredRendering();
        ~DeferredRendering();

        std::set<std::string> viewTags;
        std::list<std::string_view> resizable;
    };

}