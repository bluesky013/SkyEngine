//
// Created by Zach Lee on 2021/11/26.
//

#include <engine/SkyEngine.h>
#include <framework/environment/Environment.h>
#include <core/platform/Platform.h>

extern "C" SKY_EXPORT sky::IEngine* StartEngine(sky::Environment* env)
{
    sky::Environment::Attach(env);
    return sky::SkyEngine::Get();
}

extern "C" SKY_EXPORT void ShutdownEngine(sky::IEngine* engine)
{
    sky::SkyEngine::Destroy();
}