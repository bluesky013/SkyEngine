//
// Created by Zach Lee on 2023/1/30.
//

#pragma once

#ifdef WIN32
#include <glad/glad.h>
#include <glad/glad_egl.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#endif

#include <rhi/Core.h>
