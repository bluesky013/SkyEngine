//
// Created by Zach Lee on 2023/5/20.
//

#include "rhi/Instance.h"
#include "gles/Instance.h"
#include "core/platform/Platform.h"

extern "C" SKY_EXPORT sky::rhi::Instance *CreateInstance()
{
    return new sky::gles::Instance();
}
