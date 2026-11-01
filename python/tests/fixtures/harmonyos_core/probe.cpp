#include <core/logger/Logger.h>
#include <core/math/Vector4.h>
#include <core/platform/Platform.h>

#if !defined(SKY_PLATFORM_OHOS) || defined(SKY_PLATFORM_ANDROID) || defined(SKY_PLATFORM_LINUX)
    #error Incorrect HarmonyOS platform selection
#endif
#if !defined(SKY_PLATFORM_ARCH_ARM64)
    #error Expected arm64 HarmonyOS target
#endif
#if SKY_MATH_SIMD && !SKY_SIMD_NEON
    #error Expected NEON for HarmonyOS arm64 SIMD
#endif

static_assert(static_cast<uint8_t>(sky::PlatformType::Android) == 4);
static_assert(static_cast<uint8_t>(sky::PlatformType::IOS) == 5);
static_assert(sky::PlatformType::HarmonyOS < sky::PlatformType::UNDEFINED);
static_assert(sky::PlatformType::HarmonyOS != sky::PlatformType::Linux);

extern "C" float SkyHarmonyOSCoreProbe(float value)
{
    sky::Logger::Print("HarmonyOSCoreProbe", "INFO", "Core link probe");
    const sky::Vector4 vector(value, value, value, value);
    return vector.Dot(vector);
}
