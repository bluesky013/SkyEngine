//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <framework/environment/Singleton.h>

namespace sky {

    class GraphResourceDatabase : public Singleton<GraphResourceDatabase> {
    public:

    private:
        friend class Singleton<GraphResourceDatabase>;
        GraphResourceDatabase() = default;
        ~GraphResourceDatabase() = default;
    };

}