//
// Created on 2026/09/25.
//

#include <network/NetworkHost.h>
#include <network/NetworkSession.h>

#include "Harness.h"

#include <gtest/gtest.h>

using namespace sky::net;
using namespace sky::net::test;

TEST(NetworkResumeTokenTest, IssueVerifyAndExpiry)
{
    ResumeTokenCodec codec(0xABCDEF);

    ResumeToken token = codec.Issue(SessionId{7}, 100, 1000, 1);
    EXPECT_TRUE(codec.Verify(token, 200));
    EXPECT_FALSE(codec.Verify(token, 1200));

    ResumeToken tampered = token;
    tampered.signature ^= 0x1;
    EXPECT_FALSE(codec.Verify(tampered, 200));

    ResumeToken wrongKey = ResumeTokenCodec(0x123456).Issue(SessionId{7}, 100, 1000, 1);
    EXPECT_FALSE(codec.Verify(wrongKey, 200));
}

TEST(NetworkResumeTokenTest, SerializeRoundTrip)
{
    ResumeTokenCodec codec(42);
    ResumeToken token = codec.Issue(SessionId{99}, 500, 2000, 3);

    uint8_t buffer[ResumeToken::SERIALIZED_SIZE] = {};
    ASSERT_EQ(token.Serialize(buffer, sizeof(buffer)), ResumeToken::SERIALIZED_SIZE);

    ResumeToken restored;
    ASSERT_TRUE(ResumeToken::Deserialize(buffer, sizeof(buffer), restored));
    EXPECT_EQ(restored.session, token.session);
    EXPECT_EQ(restored.expiresAtMs, token.expiresAtMs);
    EXPECT_EQ(restored.nonce, token.nonce);
    EXPECT_EQ(restored.signature, token.signature);
}

TEST(NetworkSessionTest, ServerAssignsSessionOnConnect)
{
    NetPair pair;
    pair.ConnectAndSettle();
    ASSERT_EQ(pair.serverConnected, 1);

    SessionId serverSession = pair.server->GetSession(pair.serverConn);
    EXPECT_TRUE(IsValid(serverSession));
    EXPECT_EQ(pair.server->GetConnection(serverSession), pair.serverConn);

    SessionId clientSession = pair.client->GetSession(pair.clientConn);
    EXPECT_TRUE(IsValid(clientSession));
    EXPECT_EQ(clientSession, serverSession);
}

TEST(NetworkSessionTest, ReconnectRebindsSameSessionWithToken)
{
    NetPair pair;
    pair.ConnectAndSettle();
    ASSERT_TRUE(IsValid(pair.server->GetSession(pair.serverConn)));

    const SessionId original = pair.server->GetSession(pair.serverConn);
    const ResumeToken token = pair.client->GetResumeToken();
    ASSERT_FALSE(token.IsZero());

    // Simulate a reconnect carrying the resume token.
    pair.client->Connect(NetworkAddress::Parse(LOOPBACK_ADDR), &token);
    pair.Pump(20);

    EXPECT_TRUE(IsValid(pair.server->GetSession(pair.serverConn)));
    EXPECT_EQ(pair.server->GetSession(pair.serverConn), original);
}

TEST(NetworkSessionTest, HandoverHoldsMultipleConnectionsPerSession)
{
    NetPair pair;
    pair.ConnectAndSettle();

    const SessionId session = pair.server->GetSession(pair.serverConn);
    ASSERT_TRUE(IsValid(session));

    const ConnectionId extra{999, 1};
    EXPECT_TRUE(pair.server->BindSession(extra, session));

    // Both connections resolve to the same session; GetConnection returns a live one.
    EXPECT_EQ(pair.server->GetSession(pair.serverConn), session);
    EXPECT_EQ(pair.server->GetSession(extra), session);
    EXPECT_TRUE(IsValid(pair.server->GetConnection(session)));
}
