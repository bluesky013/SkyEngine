//
// Created by Zach Lee on 2023/5/21.
//

#pragma once

#include <gles/Forward.h>

using PFN_FramebufferFetchBarrier = void(*)(void);
extern PFN_FramebufferFetchBarrier FramebufferFetchBarrier;

using PFN_MultiDrawArraysIndirectEXT = void(*)(GLenum mode, const void *indirect, GLsizei count, GLsizei stride);
extern PFN_MultiDrawArraysIndirectEXT MultiDrawArraysIndirectEXT;
using PFN_MultiDrawElementsIndirectEXT = void(*)(GLenum mode, GLenum type, const void *indirect, GLsizei count, GLsizei stride);
extern PFN_MultiDrawElementsIndirectEXT MultiDrawElementsIndirectEXT;

using PFN_RenderbufferStorageMultisampleEXT = void(*)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
using PFN_FramebufferTexture2DMultisampleEXT = void(*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
extern PFN_RenderbufferStorageMultisampleEXT RenderbufferStorageMultisampleEXT;
extern PFN_FramebufferTexture2DMultisampleEXT FramebufferTexture2DMultisampleEXT;
