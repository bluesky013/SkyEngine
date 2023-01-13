//
// Created by Zach Lee on 2023/1/13.
//

#pragma once

#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace sky {

    namespace db {
        class Statement {
        public:
            Statement(sqlite3_stmt *handle) : stmt(handle) {}
            ~Statement();

            bool BindBlob(int col, void* data, int dataSize);
            bool BindDouble(int col, double data);
            bool BindInt(int col, int32_t data);
            bool BindText(int col, const std::string &data);
            bool BindInt64(int col, int64_t data);

            int GetInt(int col);
            double GetDouble(int col);
            const void* GetBlob(int col);
            int GetBlobBytes(int col);
            int64_t GetInt64(int col);
            std::string GetText(int col);

            void Step();
            void Finalize();
            void Reset();

        private:
            sqlite3_stmt *stmt;
        };
    }

    class DataBase {
    public:
        DataBase() = default;
        ~DataBase() = default;

        bool Init(const std::string &);
        void Shutdown();

        db::Statement *CreateStatement(const std::string &stmt);

    private:
        sqlite3 *db = nullptr;
    };

} // namespace sky
