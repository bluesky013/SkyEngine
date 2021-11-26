//
// Created by Zach Lee on 2021/11/26.
//

#include <core/platform/Platform.h>
#include <Sample.h>

extern "C" SKY_EXPORT void StartModule(sky::Application& app)
{
    auto module = new sky::Sample(app);
    app.GetEngine()->RegisterModule(module);
}