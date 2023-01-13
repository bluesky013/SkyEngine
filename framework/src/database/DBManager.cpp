//
// Created by yjrj on 2023/1/13.
//

#include <framework/database/DBManager.h>
#include <core/logger/Logger.h>
#include <sqlite/sqlite3.h>
#include <sqlite/sqlite3ext.h>

SQLITE_EXTENSION_INIT1

namespace sky {
    static const char* TAG = "DBManager";

    DBManager::~DBManager()
    {
        if (sqlite3_api != nullptr) {
            delete sqlite3_api;
            sqlite3_api = nullptr;
        }
    }

    void DBManager::Init()
    {
        module = std::make_unique<DynamicModule>("sqlite3");
        if (!module->Load()) {
            LOG_E(TAG, "load sqlite failed");
            return;
        }
        auto* routines = new sqlite3_api_routines();
        routines->open = decltype(routines->open)(module->GetAddress("sqlite3_open"));
        routines->close = decltype(routines->close)(module->GetAddress("sqlite3_close"));
        routines->prepare_v2 = decltype(routines->prepare_v2)(module->GetAddress("sqlite3_prepare_v2"));

        routines->bind_blob   = decltype(routines->bind_blob)(module->GetAddress("sqlite3_bind_blob"));
        routines->bind_double = decltype(routines->bind_double)(module->GetAddress("sqlite3_bind_double"));
        routines->bind_int    = decltype(routines->bind_int)(module->GetAddress("sqlite3_bind_int"));
        routines->bind_text   = decltype(routines->bind_text)(module->GetAddress("sqlite3_bind_text"));
        routines->bind_int64  = decltype(routines->bind_int64)(module->GetAddress("sqlite3_bind_int64"));

        routines->column_int    = decltype(routines->column_int)(module->GetAddress("sqlite3_column_int"));
        routines->column_double = decltype(routines->column_double)(module->GetAddress("sqlite3_column_double"));
        routines->column_blob   = decltype(routines->column_blob)(module->GetAddress("sqlite3_column_blob"));
        routines->column_bytes  = decltype(routines->column_bytes)(module->GetAddress("sqlite3_column_bytes"));
        routines->column_int64  = decltype(routines->column_int64)(module->GetAddress("sqlite3_column_int64"));
        routines->column_text   = decltype(routines->column_text)(module->GetAddress("sqlite3_column_text"));

        routines->step     = decltype(routines->step)(module->GetAddress("sqlite3_step"));
        routines->reset    = decltype(routines->reset)(module->GetAddress("sqlite3_reset"));
        routines->finalize = decltype(routines->finalize)(module->GetAddress("sqlite3_finalize"));

        routines->clear_bindings = decltype(routines->clear_bindings)(module->GetAddress("sqlite3_clear_bindings"));

        sqlite3_api = routines;
    }

}
