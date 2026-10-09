//
// Registration for the reflected image cook settings (see ImageCookSettings.h).
//

#include <aurora/cook/image/ImageCookSettings.h>

#include <framework/serialization/SerializationContext.h>

namespace sky::aurora::cook {

    void RegisterImageCookSettings()
    {
        static bool done = false;
        if (done) {
            return;
        }
        done = true;

        auto *context = SerializationContext::Get();

        context->Register<ImageEncode>("ImageEncode").Enum(ImageEncode::NONE, "NONE").Enum(ImageEncode::BC7, "BC7").Enum(ImageEncode::ASTC, "ASTC");

        context->Register<Quality>("ImageQuality")
            .Enum(Quality::ULTRA_FAST, "ULTRA_FAST")
            .Enum(Quality::VERY_FAST, "VERY_FAST")
            .Enum(Quality::FAST, "FAST")
            .Enum(Quality::BASIC, "BASIC")
            .Enum(Quality::SLOW, "SLOW");

        context->Register<ImageCookSettings>("ImageCookSettings")
            .Member<&ImageCookSettings::encode>("encode")
            .Member<&ImageCookSettings::srgb>("srgb")
            .Member<&ImageCookSettings::quality>("quality")
            .Member<&ImageCookSettings::block>("block")
            .Member<&ImageCookSettings::maxSize>("maxSize")
            .Member<&ImageCookSettings::generateMip>("generateMip");
    }

} // namespace sky::aurora::cook
