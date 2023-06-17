//
// Created by Zach Lee on 2023/1/13.
//

#pragma once

#include <string>

struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_api_routines;

namespace sky {

    namespace db {
        class Statement {
        public:
            Statement(sqlite3_stmt *handle, sqlite3_api_routines *api) : stmt(handle), sqlite3_api(api) {}
            ~Statement();

            int GetNamedParamIdx(const char* name) const;

            bool BindBlob(int col, void* data, int dataSize) const;
            bool BindDouble(int col, double data) const;
            bool BindInt(int col, int32_t data) const;
            bool BindText(int col, const std::string &data) const;
            bool BindInt64(int col, int64_t data) const;

            int GetInt(int col) const;
            double GetDouble(int col) const;
            const void* GetBlob(int col) const;
            int GetBlobBytes(int col) const;
            int64_t GetInt64(int col) const;
            std::string GetText(int col) const;

            int Step();
            void Finalize();
            void Reset();

        private:
            sqlite3_stmt *stmt;
            sqlite3_api_routines *sqlite3_api = nullptr;
        };
    }

    class DataBase {
    public:
        DataBase() = default;
        ~DataBase();

        bool Init(const std::string &);
        void Shutdown();

        db::Statement *CreateStatement(const std::string &stmt);

    private:
        sqlite3 *db = nullptr;
        sqlite3_api_routines *sqlite3_api = nullptr;
    };

} // namespace sky
