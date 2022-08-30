//
// Created by Zach Lee on 2021/11/26.
//

#include <core/environment/Environment.h>
#include <core/platform/Platform.h>
#include <engine/SkyEngine.h>

extern "C" SKY_EXPORT sky::IEngine *StartEngine(sky::Environment *env)
{
    sky::Environment::Attach(env);
    return sky::SkyEngine::Get();
}

extern "C" SKY_EXPORT void ShutdownEngine(sky::IEngine *engine)
{
    sky::SkyEngine::Destroy();
}