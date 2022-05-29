//
// Created by Zach Lee on 2022/5/24.
//


#include <render/resources/DescirptorGroup.h>

namespace sky {

    bool DescriptorGroup::IsValid() const
    {
        return !!set;
    }
}