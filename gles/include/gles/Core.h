//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <core/platform/Platform.h>
#include <gles/Forward.h>

namespace sky::gles {

#ifdef _DEBUG
#define CHECK(x) do { x; SKY_ASSERT(glGetError() == GL_NO_ERROR); } while(0);
#else
#define CHECK(x) do { x; } while(0);
#endif
}
