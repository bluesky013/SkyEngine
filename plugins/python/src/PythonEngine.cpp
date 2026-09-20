//
// Created by blues on 2024/6/2.
//

#include <python/PythonEngine.h>
#include <python/PythonBinding.h>

#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#if defined(SKY_PLATFORM_WINDOWS)
extern "C" {
PyObject *PyInit_unicodedata(void);
PyObject *PyInit__decimal(void);
PyObject *PyInit__uuid(void);
PyObject *PyInit__zoneinfo(void);
PyObject *PyInit__elementtree(void);
PyObject *PyInit_pyexpat(void);
PyObject *PyInit__bz2(void);
PyObject *PyInit__lzma(void);
PyObject *PyInit__socket(void);
PyObject *PyInit_select(void);
PyObject *PyInit__overlapped(void);
PyObject *PyInit__queue(void);
}
#endif

static const char *TAG = "PythonEngine";

namespace sky::py {

    static void RegisterBuiltinModules()
    {
#if defined(SKY_PLATFORM_WINDOWS)
        static bool registered = false;
        if (registered) {
            return;
        }
        registered = true;

        PyImport_AppendInittab("unicodedata", PyInit_unicodedata);
        PyImport_AppendInittab("_decimal", PyInit__decimal);
        PyImport_AppendInittab("_uuid", PyInit__uuid);
        PyImport_AppendInittab("_zoneinfo", PyInit__zoneinfo);
        PyImport_AppendInittab("_elementtree", PyInit__elementtree);
        PyImport_AppendInittab("pyexpat", PyInit_pyexpat);
        PyImport_AppendInittab("_bz2", PyInit__bz2);
        PyImport_AppendInittab("_lzma", PyInit__lzma);
        PyImport_AppendInittab("_socket", PyInit__socket);
        PyImport_AppendInittab("select", PyInit_select);
        PyImport_AppendInittab("_overlapped", PyInit__overlapped);
        PyImport_AppendInittab("_queue", PyInit__queue);
#endif
    }

    static bool HasStdLib(const std::string &home)
    {
        if (home.empty()) {
            return false;
        }

        std::error_code ec;
        const std::filesystem::path root(home);
#if defined(SKY_PLATFORM_WINDOWS)
        return std::filesystem::is_directory(root / "Lib", ec);
#else
        const auto libDir = root / "lib";
        if (!std::filesystem::is_directory(libDir, ec)) {
            return false;
        }
        for (const auto &entry : std::filesystem::directory_iterator(libDir, ec)) {
            if (entry.path().filename().string().rfind("python3.", 0) == 0) {
                return true;
            }
        }
        return false;
#endif
    }

    static void ReportPythonError(const char *context)
    {
        if (PyErr_Occurred() == nullptr) {
            LOG_E(TAG, "%s failed", context);
            return;
        }

        PyObject *type = nullptr;
        PyObject *value = nullptr;
        PyObject *traceback = nullptr;
        PyErr_Fetch(&type, &value, &traceback);
        PyErr_NormalizeException(&type, &value, &traceback);

        PyObject *valueStr = value != nullptr ? PyObject_Str(value) : nullptr;
        const char *message = valueStr != nullptr ? PyUnicode_AsUTF8(valueStr) : nullptr;
        LOG_E(TAG, "%s failed: %s", context, message != nullptr ? message : "<unknown>");

        Py_XDECREF(valueStr);
        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);
    }

    std::string PythonEngine::ResolveHome()
    {
        std::vector<std::string> candidates;

        for (const char *name : {"SKY_PYTHON_HOME", "PYTHONHOME"}) {
            const char *value = std::getenv(name);
            if (value != nullptr && value[0] != '\0') {
                candidates.emplace_back(value);
            }
        }

        if (auto *platform = Platform::Get(); platform != nullptr && platform->GetImpl() != nullptr) {
            for (const std::string &base : {platform->GetInternalPath(), platform->GetBundlePath()}) {
                if (base.empty()) {
                    continue;
                }
                const std::filesystem::path root(base);
                candidates.push_back((root / "cpython").string());
                candidates.push_back((root / ".." / "cpython").string());
            }
        }

        for (const auto &candidate : candidates) {
            if (HasStdLib(candidate)) {
                return candidate;
            }
        }
        return {};
    }

    bool PythonEngine::Init()
    {
        if (isInited) {
            return true;
        }

        if (!Environment::IsAttached()) {
            LOG_E(TAG, "environment is not attached; attach the host Environment before PythonInit");
            return false;
        }

        const std::string home = ResolveHome();
        if (home.empty()) {
            LOG_W(TAG, "python home not found; using interpreter defaults");
        } else {
            LOG_I(TAG, "python home: %s", home.c_str());
        }

        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        config.install_signal_handlers = 0;
        config.site_import = 0;

        RegisterBuiltinModules();

        // Android ships the standard library as a zip (SKY_PYTHON_ZIP); add it to the module search path.
        const char *zipEnv = std::getenv("SKY_PYTHON_ZIP");
        if (zipEnv != nullptr && zipEnv[0] != '\0') {
            wchar_t *zipPath = Py_DecodeLocale(zipEnv, nullptr);
            if (zipPath != nullptr) {
                PyStatus zipStatus = PyWideStringList_Append(&config.module_search_paths, zipPath);
                PyMem_RawFree(zipPath);
                if (PyStatus_Exception(zipStatus) != 0) {
                    LOG_E(TAG, "set python zip path failed: %s", zipStatus.err_msg);
                    PyConfig_Clear(&config);
                    return false;
                }
                config.module_search_paths_set = 1;
            }
        }

        if (!home.empty()) {
            PyStatus homeStatus = PyConfig_SetBytesString(&config, &config.home, home.c_str());
            if (PyStatus_Exception(homeStatus) != 0) {
                LOG_E(TAG, "set python home failed: %s", homeStatus.err_msg);
                PyConfig_Clear(&config);
                return false;
            }
        }

        PyStatus status = Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);
        if (PyStatus_Exception(status) != 0) {
            LOG_E(TAG, "Python init failed: %s", status.err_msg);
            return false;
        }

        isInited = (Py_IsInitialized() != 0);
        if (isInited && !InitializeReflectionBindings()) {
            LOG_W(TAG, "reflection bindings failed to initialize");
        }
        return isInited;
    }

    void PythonEngine::Shutdown()
    {
        if (!isInited) {
            return;
        }

        Py_Finalize();
        isInited = false;
        ShutdownReflectionBindings();
    }

    bool PythonEngine::RunString(std::string_view source)
    {
        if (!isInited) {
            LOG_E(TAG, "interpreter is not initialized");
            return false;
        }

        PyGILState_STATE gil = PyGILState_Ensure();
        PyObject *main = PyImport_AddModule("__main__");
        PyObject *globals = main != nullptr ? PyModule_GetDict(main) : nullptr;
        PyObject *result = nullptr;
        if (globals != nullptr) {
            result = PyRun_String(std::string(source).c_str(), Py_file_input, globals, globals);
        }

        bool ok = result != nullptr;
        if (!ok) {
            ReportPythonError("run string");
        } else {
            Py_DECREF(result);
        }
        PyGILState_Release(gil);
        return ok;
    }

    bool PythonEngine::RunFile(const std::string &path)
    {
        if (!isInited) {
            LOG_E(TAG, "interpreter is not initialized");
            return false;
        }

        FILE *file = nullptr;
#if defined(SKY_PLATFORM_WINDOWS)
        if (fopen_s(&file, path.c_str(), "r") != 0) {
            file = nullptr;
        }
#else
        file = fopen(path.c_str(), "r");
#endif
        if (file == nullptr) {
            LOG_E(TAG, "cannot open script: %s", path.c_str());
            return false;
        }

        PyGILState_STATE gil = PyGILState_Ensure();
        PyObject *main = PyImport_AddModule("__main__");
        PyObject *globals = main != nullptr ? PyModule_GetDict(main) : nullptr;
        PyObject *result = nullptr;
        if (globals != nullptr) {
            result = PyRun_File(file, path.c_str(), Py_file_input, globals, globals);
        }

        bool ok = result != nullptr;
        if (!ok) {
            ReportPythonError("run file");
        } else {
            Py_DECREF(result);
        }
        PyGILState_Release(gil);
        fclose(file);
        return ok;
    }

} // namespace sky::py
