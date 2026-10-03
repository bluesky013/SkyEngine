//
// Created by blues on 2026/10/3.
//

#include <gtest/gtest.h>

#include <framework/asset/CookProtocol.h>

#include <string>

using namespace sky;

TEST(CookProtocolTest, CookRoundTrip)
{
    CookMessage message = {};
    message.type = CookMessageType::Cook;
    message.id = 42;
    message.uuid = Uuid::Create();
    message.target = "Win32";
    message.path = "assets/mesh/a.t1";

    CookMessage decoded;
    ASSERT_TRUE(DecodeCookMessage(EncodeCookMessage(message), decoded));

    EXPECT_EQ(decoded.type, CookMessageType::Cook);
    EXPECT_EQ(decoded.id, 42u);
    EXPECT_EQ(decoded.uuid, message.uuid);
    EXPECT_EQ(decoded.target, "Win32");
    EXPECT_EQ(decoded.path, "assets/mesh/a.t1");
}

TEST(CookProtocolTest, ResultRoundTrip)
{
    CookMessage message = {};
    message.type = CookMessageType::Result;
    message.id = 7;
    message.uuid = Uuid::Create();
    message.target = "common";
    message.retCode = 1;
    message.error = "builder failed";

    CookMessage decoded;
    ASSERT_TRUE(DecodeCookMessage(EncodeCookMessage(message), decoded));

    EXPECT_EQ(decoded.type, CookMessageType::Result);
    EXPECT_EQ(decoded.id, 7u);
    EXPECT_EQ(decoded.uuid, message.uuid);
    EXPECT_EQ(decoded.retCode, 1);
    EXPECT_EQ(decoded.error, "builder failed");
}

TEST(CookProtocolTest, ReadyCarriesProtocolAndPlatform)
{
    CookMessage message = {};
    message.type = CookMessageType::Ready;
    message.protocol = COOK_PROTOCOL_VERSION;
    message.platform = "Win32";

    CookMessage decoded;
    ASSERT_TRUE(DecodeCookMessage(EncodeCookMessage(message), decoded));

    EXPECT_EQ(decoded.type, CookMessageType::Ready);
    EXPECT_EQ(decoded.protocol, COOK_PROTOCOL_VERSION);
    EXPECT_EQ(decoded.platform, "Win32");
}

TEST(CookProtocolTest, ControlMessagesRoundTrip)
{
    for (const auto type : {CookMessageType::Hello, CookMessageType::Ping, CookMessageType::Pong, CookMessageType::Shutdown}) {
        CookMessage message = {};
        message.type = type;

        CookMessage decoded;
        ASSERT_TRUE(DecodeCookMessage(EncodeCookMessage(message), decoded)) << "type index " << static_cast<uint32_t>(type);
        EXPECT_EQ(decoded.type, type);
    }
}

TEST(CookProtocolTest, UnknownTypeRejected)
{
    CookMessage decoded;
    EXPECT_FALSE(DecodeCookMessage("{\"type\":\"bogus\"}", decoded));
}

TEST(CookProtocolTest, MalformedPayloadRejected)
{
    CookMessage decoded;
    EXPECT_FALSE(DecodeCookMessage("not json", decoded));
}
