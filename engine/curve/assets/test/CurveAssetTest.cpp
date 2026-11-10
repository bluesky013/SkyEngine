#include <core/archive/MemoryStreamArchive.h>
#include <curve/assets/CurveAsset.h>
#include <curve/assets/CurveSource.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::curve;

namespace {
    Curve MakeCurve()
    {
        Curve curve;
        curve.channels      = 3;
        curve.interpolation = CurveInterpolation::LINEAR;
        curve.preWrap       = CurveWrap::MIRROR;
        curve.postWrap      = CurveWrap::MIRROR;
        curve.keys.push_back({0.f, {1.f, 2.f, 3.f, 0.f}});
        curve.keys.push_back({1.f, {4.f, 5.f, 6.f, 0.f}});
        curve.keys[0].tangentOut = {3.f, -4.f, 5.f, 0.f};
        curve.keys[1].tangentIn  = {-1.f, 2.f, -3.f, 0.f};
        return curve;
    }
} // namespace

TEST(CurveSource, RoundTrips)
{
    const auto  curve = MakeCurve();
    std::string text;
    ASSERT_TRUE(WriteCurveSource(curve, text));

    Curve       parsed;
    std::string error;
    ASSERT_TRUE(ParseCurveSource(text, parsed, error)) << error;
    EXPECT_EQ(parsed.channels, 3u);
    EXPECT_EQ(parsed.interpolation, CurveInterpolation::LINEAR);
    EXPECT_EQ(parsed.preWrap, CurveWrap::MIRROR);
    EXPECT_EQ(parsed.postWrap, CurveWrap::MIRROR);
    ASSERT_EQ(parsed.keys.size(), 2u);
    EXPECT_FLOAT_EQ(parsed.keys[1].value.y, 5.f);
    EXPECT_FLOAT_EQ(parsed.keys[0].tangentOut.y, -4.f);
    EXPECT_FLOAT_EQ(parsed.keys[1].tangentIn.z, -3.f);

    std::string rewritten;
    ASSERT_TRUE(WriteCurveSource(parsed, rewritten));
    EXPECT_EQ(rewritten, text);
}

TEST(CurveSource, RejectsInvalidDocuments)
{
    Curve       curve;
    std::string error;
    EXPECT_FALSE(
        ParseCurveSource(R"({"version":2,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[]})", curve, error));
    EXPECT_FALSE(ParseCurveSource(
        R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0.5,"value":[1]},{"time":0.4,"value":[2]}]})",
        curve, error));
    EXPECT_FALSE(ParseCurveSource(
        R"({"version":1,"channels":2,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[1]}]})", curve, error));
}

TEST(CurveSource, RejectsInvalidChannelsAndPreservesPreviousCurve)
{
    auto        curve = MakeCurve();
    std::string before;
    ASSERT_TRUE(WriteCurveSource(curve, before));
    for (const uint32_t channels : {0u, 5u, 16u, 4294967295u}) {
        const std::string text =
            "{\"version\":1,\"channels\":" + std::to_string(channels) +
            R"(,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]}]})";
        std::string error = "old error";
        EXPECT_FALSE(ParseCurveSource(text, curve, error));
        EXPECT_NE(error.find("channels"), std::string::npos);
        std::string after;
        ASSERT_TRUE(WriteCurveSource(curve, after));
        EXPECT_EQ(before, after);
    }
}

TEST(CurveSource, EnforcesSourceLimitWithoutOverwritingPreviousCurve)
{
    auto        curve = MakeCurve();
    std::string original;
    ASSERT_TRUE(WriteCurveSource(curve, original));
    std::string text = original;
    text.resize(MAX_CURVE_SOURCE_BYTES, ' ');
    std::string error;
    ASSERT_TRUE(ParseCurveSource(text, curve, error)) << error;
    text.push_back(' ');
    EXPECT_FALSE(ParseCurveSource(text, curve, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(ParseCurveSource({}, curve, error));
    std::string after;
    ASSERT_TRUE(WriteCurveSource(curve, after));
    EXPECT_EQ(original, after);
}

TEST(CurveSource, HandlesDeepJsonWithoutRecursiveStackGrowth)
{
    std::string text =
        R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[1]}],"unused":)";
    text.append(120000, '[');
    text += '0';
    text.append(120000, ']');
    text += '}';
    ASSERT_LT(text.size(), MAX_CURVE_SOURCE_BYTES);
    Curve       curve;
    std::string error;
    ASSERT_TRUE(ParseCurveSource(text, curve, error)) << error;
    EXPECT_FLOAT_EQ(curve.Evaluate(0.f).x, 1.f);
}

TEST(CurveSource, RoundTripsAllEnumValuesAndRejectsEnumBoundaries)
{
    for (uint32_t interpolation = 0; interpolation < static_cast<uint32_t>(CurveInterpolation::END); ++interpolation) {
        for (uint32_t wrap = 0; wrap < static_cast<uint32_t>(CurveWrap::END); ++wrap) {
            auto curve          = MakeCurve();
            curve.interpolation = static_cast<CurveInterpolation>(interpolation);
            curve.preWrap = curve.postWrap = static_cast<CurveWrap>(wrap);
            std::string text;
            ASSERT_TRUE(WriteCurveSource(curve, text));
            Curve       parsed;
            std::string error;
            ASSERT_TRUE(ParseCurveSource(text, parsed, error)) << error;
            EXPECT_EQ(parsed.interpolation, curve.interpolation);
            EXPECT_EQ(parsed.preWrap, curve.preWrap);
            EXPECT_EQ(parsed.postWrap, curve.postWrap);
        }
    }
    auto        curve   = MakeCurve();
    std::string text    = "previous output";
    curve.interpolation = CurveInterpolation::END;
    EXPECT_FALSE(WriteCurveSource(curve, text));
    EXPECT_EQ(text, "previous output");
    curve.interpolation = CurveInterpolation::LINEAR;
    curve.postWrap      = CurveWrap::END;
    EXPECT_FALSE(WriteCurveSource(curve, text));
    EXPECT_EQ(text, "previous output");
}

TEST(CurveProduct, RoundTrips)
{
    CurveAssetData source;
    source.sourceHash = 7;
    source.curve      = MakeCurve();
    source.valid      = true;

    OMemoryArchive      output;
    BinaryOutputArchive writer(output);
    source.Save(writer);

    IMemoryArchive     input(output.Data(), output.Size());
    BinaryInputArchive reader(input);
    CurveAssetData     decoded;
    decoded.Load(reader);
    ASSERT_TRUE(decoded.IsLoadValid());
    EXPECT_EQ(decoded.sourceHash, 7u);
    EXPECT_EQ(decoded.curve.channels, 3u);
    ASSERT_EQ(decoded.curve.keys.size(), 2u);
    EXPECT_FLOAT_EQ(decoded.curve.keys[0].tangentOut.y, -4.f);
    EXPECT_FLOAT_EQ(decoded.curve.keys[1].tangentIn.z, -3.f);
}

TEST(CurveProduct, RejectsMismatchedHeader)
{
    OMemoryArchive      output;
    BinaryOutputArchive writer(output);
    writer.SaveValue(uint32_t{0x56435243});
    writer.SaveValue(uint32_t{2});
    writer.SaveValue(uint32_t{1});
    writer.SaveValue(uint64_t{1});
    writer.SaveValue(uint32_t{1});
    writer.SaveValue(uint32_t{0});
    writer.SaveValue(uint32_t{0});
    writer.SaveValue(uint32_t{0});
    writer.SaveValue(uint32_t{1});
    writer.SaveValue(0.f);
    writer.SaveValue(1.f);
    writer.SaveValue(0.f);
    writer.SaveValue(0.f);
    writer.SaveValue(0.f);
    for (int i = 0; i < 8; ++i)
        writer.SaveValue(0.f);

    IMemoryArchive     input(output.Data(), output.Size());
    BinaryInputArchive reader(input);
    CurveAssetData     decoded;
    decoded.Load(reader);
    EXPECT_FALSE(decoded.IsLoadValid());
}

TEST(CurveAssets, RegistersTypeAndTraits)
{
    EXPECT_NE(sky::SerializationContext::Get()->FindType("Curve"), nullptr);
    const auto *key = sky::SerializationContext::Get()->FindType("CurveKey");
    ASSERT_NE(key, nullptr);
    EXPECT_TRUE(key->members.contains("tangentIn"));
    EXPECT_TRUE(key->members.contains("tangentOut"));
    EXPECT_EQ(sky::AssetTraits<CurveAsset>::ASSET_TYPE, "Curve");
}
