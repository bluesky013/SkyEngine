//
// Created by yjrj on 2023/1/13.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/DynamicModule.h>

struct sqlite3_api_routines;

namespace sky {

    class DBManager : public Singleton<DBManager> {
    public:
        DBManager() = default;
        ~DBManager() override;

        void Init();
        sqlite3_api_routines *GetRoutines() const { return routines; }

    private:
        std::unique_ptr<DynamicModule> module;
        sqlite3_api_routines *routines = nullptr;
    };

}
