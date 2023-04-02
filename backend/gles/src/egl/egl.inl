#define ACCESS(module, name)       \
    using Type = decltype(name);   \
    static Type *access = nullptr; \
    if (access == nullptr) {       \
        access = (Type *)module->GetAddress(#name); \
    }

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *procname)
{
    ACCESS(g_Egl, eglGetProcAddress);
    return access(procname);
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    ACCESS(g_Egl, eglChooseConfig)
    return access(dpy, attrib_list, configs, config_size, num_config);
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
{
    ACCESS(g_Egl, eglCreateContext)
    return access(dpy, config, share_context, attrib_list);
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    ACCESS(g_Egl, eglCreatePbufferSurface)
    return access(dpy, config, attrib_list);
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
    ACCESS(g_Egl, eglCreateWindowSurface)
    return access(dpy, config, win, attrib_list);
}

EGLSurface eglCreatePlatformWindowSurface(EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list)
{
    ACCESS(g_Egl, eglCreatePlatformWindowSurface)
    return access(dpy, config, native_window, attrib_list);
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    ACCESS(g_Egl, eglDestroyContext)
    return access(dpy, ctx);
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    ACCESS(g_Egl, eglDestroySurface)
    return access(dpy, surface);
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    ACCESS(g_Egl, eglGetConfigAttrib)
    return access(dpy, config, attribute, value);
}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    ACCESS(g_Egl, eglGetConfigs)
    return access(dpy, configs, config_size, num_config);
}

EGLDisplay eglGetCurrentDisplay(void)
{
    ACCESS(g_Egl, eglGetCurrentDisplay)
    return access();
}

EGLSurface eglGetCurrentSurface(EGLint read_draw)
{
    ACCESS(g_Egl, eglGetCurrentSurface)
    return access(read_draw);
}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
    ACCESS(g_Egl, eglGetDisplay)
    return access(display_id);
}

EGLint eglGetError(void)
{
    ACCESS(g_Egl, eglGetError)
    return access();
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    ACCESS(g_Egl, eglInitialize)
    return access(dpy, major, minor);
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    ACCESS(g_Egl, eglMakeCurrent)
    return access(dpy, draw, read, ctx);
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    ACCESS(g_Egl, eglSwapBuffers)
    return access(dpy, surface);
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
    ACCESS(g_Egl, eglTerminate)
    return access(dpy);
}

EGLBoolean eglBindAPI(EGLenum api)
{
    ACCESS(g_Egl, eglBindAPI)
    return access(api);
}

EGLContext eglGetCurrentContext(void)
{
    ACCESS(g_Egl, eglGetCurrentContext)
    return access();
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
{
    ACCESS(g_Egl, eglQuerySurface)
    return access(dpy, surface, attribute, value);
}

//EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target);
//EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list);
//EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);
//const char *eglQueryString(EGLDisplay dpy, EGLint name);
//EGLBoolean eglWaitGL(void);
//EGLBoolean eglWaitNative(EGLint engine);
//EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
//EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
//EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
//EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval);
//EGLenum eglQueryAPI(void);
//EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list);
//EGLBoolean eglReleaseThread(void);
//EGLBoolean eglWaitClient(void);
//EGLSync eglCreateSync(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list);
//EGLBoolean eglDestroySync(EGLDisplay dpy, EGLSync sync);
//EGLint eglClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout);
//EGLBoolean eglGetSyncAttrib(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value);
//EGLImage eglCreateImage(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list);
//EGLBoolean eglDestroyImage(EGLDisplay dpy, EGLImage image);
//EGLDisplay eglGetPlatformDisplay(EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
//EGLSurface eglCreatePlatformPixmapSurface(EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list);
//EGLBoolean eglWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags);
