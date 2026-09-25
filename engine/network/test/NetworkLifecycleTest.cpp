//
// Created on 2026/09/25.
//

#include <network/NetworkHost.h>

#include "Harness.h"

#include <gtest/gtest.h>
#include <string>

using namespace sky::net;
using namespace sky::net::test;

TEST(NetworkLifecycleTest, InitialStateIsAccepting)
{
    NetPair pair;
    pair.ConnectAndSettle();
    EXPECT_EQ(pair.server->GetLifecycleState(), NetworkLifecycle::Accepting);
}

TEST(NetworkLifecycleTest, DrainRejectsNewConnections)
{
    NetPair pair;
    pair.ConnectAndSettle();
    ASSERT_EQ(pair.serverConnected, 1);

    pair.server->DrainToClosed();
    EXPECT_EQ(pair.server->GetLifecycleState(), NetworkLifecycle::Draining);

    // A second client connects while the server is draining.
    LoopbackBackend secondBackend;
    NetworkHostConfig secondConfig;
    secondConfig.role = NetworkRole::Client;
    NetworkHost second(secondConfig);
    second.AttachBackend(NetworkRole::Client, &secondBackend);
    second.Connect(NetworkAddress::Parse(LOOPBACK_ADDR));

    for (int i = 0; i < 20; ++i) {
        second.Update();
        pair.server->Update();
    }

    EXPECT_EQ(pair.serverConnected, 1);
}

TEST(NetworkLifecycleTest, RedirectMovesClient)
{
    NetPair pair;
    pair.ConnectAndSettle();
    ASSERT_EQ(pair.serverConnected, 1);

    NetworkAddress target = NetworkAddress::Parse("loopback:7000");
    bool redirected = false;
    NetworkAddress received;
    pair.client->SetRedirectHandler([&](const NetworkAddress &address, SessionId, const ResumeToken &) {
        redirected = true;
        received   = address;
    });

    EXPECT_EQ(pair.server->RedirectTo(pair.serverConn, target), NetResult::Ok);
    pair.Pump(4);

    EXPECT_TRUE(redirected);
    EXPECT_EQ(received, target);
}

TEST(NetworkLifecycleTest, StopClosesHost)
{
    NetPair pair;
    pair.ConnectAndSettle();

    pair.client->Stop();
    EXPECT_EQ(pair.client->GetLifecycleState(), NetworkLifecycle::Closed);

    // Updating a stopped caller-pump host must be safe.
    pair.client->Update();
}
