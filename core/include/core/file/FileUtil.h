//
// Created by Zach Lee on 2022/3/11.
//


#pragma once
#include <string>

namespace sky {

    bool ConstructFullPath(const std::string& root, const std::string& relative, std::string& out);

}