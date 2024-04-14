#define ACCESS(name)       \
    using Type = decltype(name);   \
    static Type *access = nullptr; \
    if (access == nullptr) {       \
        access = (Type *)eglGetProcAddress(#name); \
    }