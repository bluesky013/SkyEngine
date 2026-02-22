//
// Created by Zach Lee on 2023/1/13.
//

//#include <framework/database/DataBase.h>
//#include <framework/database/DBManager.h>
//#include <core/logger/Logger.h>
//#include <sqlite/sqlite3.h>
//#include <sqlite/sqlite3ext.h>
//
//static const char *TAG = "Asset";
//
//namespace sky {
//    namespace db {
//        Statement::~Statement()
//        {
//            Finalize();
//        }
//
//        bool Statement::BindBlob(int col, void *data, int dataSize) const
//        {
//            return sqlite3_bind_blob(stmt, col, data, dataSize, nullptr) == SQLITE_OK;
//        }
//
//        bool Statement::BindDouble(int col, double data) const
//        {
//            return sqlite3_bind_double(stmt, col, data) == SQLITE_OK;
//        }
//
//        bool Statement::BindInt(int col, int32_t data) const
//        {
//            return sqlite3_bind_int(stmt, col, data) == SQLITE_OK;
//        }
//
//        bool Statement::BindText(int col, const std::string &data) const
//        {
//            return sqlite3_bind_text(stmt, col, data.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
//        }
//
//        bool Statement::BindInt64(int col, int64_t data) const
//        {
//            return sqlite3_bind_int64(stmt, col, data) == SQLITE_OK;
//        }
//
//        int Statement::GetInt(int col) const
//        {
//            return sqlite3_column_int(stmt, col);
//        }
//
//        double Statement::GetDouble(int col) const
//        {
//            return sqlite3_column_double(stmt, col);
//        }
//
//        const void* Statement::GetBlob(int col) const
//        {
//            return sqlite3_column_blob(stmt, col);
//        }
//
//        int Statement::GetBlobBytes(int col) const
//        {
//            return sqlite3_column_bytes(stmt, col);
//        }
//
//        int64_t Statement::GetInt64(int col) const
//        {
//            return sqlite3_column_int64(stmt, col);
//        }
//
//        std::string Statement::GetText(int col) const
//        {
//            auto *data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
//            if (data != nullptr) {
//                return data;
//            }
//            return "";
//        }
//
//        int Statement::Step()
//        {
//            int res = SQLITE_BUSY;
//            while (res == SQLITE_BUSY) {
//                res = sqlite3_step(stmt);
//            }
//            return res;
//        }
//
//        void Statement::Finalize()
//        {
//            sqlite3_finalize(stmt);
//            stmt = nullptr;
//        }
//
//        void Statement::Reset()
//        {
//            sqlite3_reset(stmt);
//            sqlite3_clear_bindings(stmt);
//        }
//
//        int Statement::GetNamedParamIdx(const char* name) const
//        {
//            return sqlite3_bind_parameter_index(stmt, name);
//        }
//    }
//
//    DataBase::~DataBase()
//    {
//        Shutdown();
//    }
//
//    bool DataBase::Init(const std::string &path)
//    {
//        sqlite3_api = DBManager::Get()->GetRoutines();
//        if (db != nullptr) {
//            sqlite3_close(db);
//            db = nullptr;
//        }
//
//        int res = sqlite3_open(path.c_str(), &db);
//        if (res) {
//            sqlite3_close(db);
//            return false;
//        }
//        return true;
//    }
//
//    void DataBase::Shutdown()
//    {
//        if (db != nullptr) {
//            sqlite3_close(db);
//            db = nullptr;
//        }
//    }
//
//    db::Statement *DataBase::CreateStatement(const std::string &stmt)
//    {
//        sqlite3_stmt *sqlite3Stmt = nullptr;
//        int res = sqlite3_prepare_v2(db, stmt.c_str(), static_cast<int32_t>(stmt.length() + 1), &sqlite3Stmt, NULL);
//        if (res != SQLITE_OK) {
//            LOG_E(TAG, "prepare statement failed. %s", sqlite3_errmsg(db));
//            return nullptr;
//        }
//        return new db::Statement(sqlite3Stmt, sqlite3_api);
//    }
//
//} // namespace sky
