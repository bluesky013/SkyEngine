//
// Created by blues on 2024/6/2.
//

#include <python/PythonEngine.h>

#include <core/logger/Logger.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

static const char* TAG = "PythonEngine";

namespace sky::py {

    bool PythonEngine::Init()
    {
        PyConfig config;
        PyConfig_InitPythonConfig(&config);

        PyConfig_SetString(&config, &config.home, L"D:\\Code\\3rd\\cpython");

        auto status = Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);
        if (PyStatus_Exception(status) != 0) {
            LOG_E(TAG, "Python Init failed... %s", status.err_msg);
            return false;
        }

        isInited = (Py_IsInitialized() != 0);
        return isInited;
    }

    void PythonEngine::Shutdown()
    {
        Py_Finalize();
    }


} // namespace sky::py