//
// Created by yjrj on 2023/1/13.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/DynamicModule.h>

namespace sky {

    class DBManager : public Singleton<DBManager> {
    public:
        DBManager() = default;
        ~DBManager();

        void Init();

    private:
        std::unique_ptr<DynamicModule> module;
    };

}
