//
// Created on 2026/10/01.
//

#include <network/lockstep/LockstepHost.h>

#include <network/NetworkHost.h>

#include "LoopbackBackend.h"
#include "LoopbackHarness.h"
#include "SimpleDeterministicSim.h"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace sky::net;
using namespace sky::net::test;

namespace {

    struct Peer {
        LoopbackBackend       backend;
        SimpleDeterministicSim sim{4};
        NetworkHost           host;
        LockstepHost          lockstep;

        Peer(NetworkRole role, const LockstepConfig &config)
            : host(role == NetworkRole::Server ? ServerHostConfig() : ClientHostConfig()),
              lockstep(host, sim, config)
        {
            backend.Init();
            host.AttachBackend(role, &backend);
        }

        void Update(double dt)
        {
            host.Update();
            lockstep.Update(dt);
        }
    };

} // namespace

TEST(LockstepHostTest, BlockingWaitsForAllInputs)
{
    LockstepConfig config;
    SimpleDeterministicSim sim;
    NetworkHost host(ServerHostConfig());
    LockstepHost lockstep(host, sim, config);
    ASSERT_EQ(lockstep.Start(), LockstepResult::Ok);
    lockstep.SetAuthority(true);
    lockstep.SetPlayerCount(2);   // expects two players, but only the local one submits
    lockstep.SetLocalPlayer(0);

    const uint8_t input = 1;
    lockstep.SubmitInput(std::span<const uint8_t>(&input, 1));
    lockstep.Update(1.0 / 30.0);

    EXPECT_TRUE(lockstep.IsWaitingForInputs());
    EXPECT_EQ(lockstep.CurrentFrame(), 0u);
}

TEST(LockstepHostTest, TwoPeersConverge)
{
    LockstepConfig config;
    config.hashCadenceFrames = 0;
    config.inputDelayFrames = 0;

    Peer authority(NetworkRole::Server, config);
    authority.host.SetServer(true);
    authority.host.Listen(NetworkAddress::Parse("loopback:9500"));
    ASSERT_EQ(authority.lockstep.Start(), LockstepResult::Ok);
    authority.lockstep.SetAuthority(true);
    authority.lockstep.SetPlayerCount(2);
    authority.lockstep.SetLocalPlayer(0);

    Peer client(NetworkRole::Client, config);
    ASSERT_EQ(client.lockstep.Start(), LockstepResult::Ok);
    client.lockstep.SetAuthority(false);
    client.lockstep.SetPlayerCount(2);
    client.lockstep.SetLocalPlayer(1);
    client.host.Connect(NetworkAddress::Parse("loopback:9500"));

    // Establish the connection before exchanging inputs.
    for (int i = 0; i < 10; ++i) {
        authority.Update(0.0);
        client.Update(0.0);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        const uint8_t a = 1;
        const uint8_t b = 2;
        authority.lockstep.SubmitInput(std::span<const uint8_t>(&a, 1));
        client.lockstep.SubmitInput(std::span<const uint8_t>(&b, 1));
        authority.Update(1.0 / 30.0);
        client.Update(1.0 / 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (authority.lockstep.CurrentFrame() >= 20 &&
            client.lockstep.CurrentFrame() == authority.lockstep.CurrentFrame()) {
            break;
        }
    }

    EXPECT_GT(authority.lockstep.CurrentFrame(), 10u);
    EXPECT_EQ(client.lockstep.CurrentFrame(), authority.lockstep.CurrentFrame());
    EXPECT_EQ(client.sim.StateHash(), authority.sim.StateHash());
}

TEST(LockstepHostTest, RollbackCorrectsLateInput)
{
    LockstepConfig config;
    config.inputDelayFrames = 0;
    SimpleDeterministicSim sim(2);
    NetworkHost host(ServerHostConfig());
    LockstepHost lockstep(host, sim, config);
    ASSERT_EQ(lockstep.Start(), LockstepResult::Ok);
    lockstep.SetAuthority(true);
    lockstep.SetPlayerCount(1);
    lockstep.SetLocalPlayer(0);

    for (LockstepFrame frame = 0; frame < 5; ++frame) {
        const uint8_t input = 1;
        lockstep.SubmitInput(std::span<const uint8_t>(&input, 1));
        lockstep.Update(1.0 / 30.0);
    }
    ASSERT_EQ(lockstep.CurrentFrame(), 5u);

    const uint8_t corrected = 9;
    ASSERT_EQ(lockstep.ApplyCorrection(2, 0, std::span<const uint8_t>(&corrected, 1)), LockstepResult::Ok);

    // Reference: the same frames but with the corrected input applied from the start.
    SimpleDeterministicSim reference(2);
    const std::vector<std::vector<uint8_t>> expected = {
        {1}, {1}, {9}, {1}, {1}};
    for (LockstepFrame frame = 0; frame < 5; ++frame) {
        reference.Advance(frame, {expected[frame]});
    }
    EXPECT_EQ(sim.StateHash(), reference.StateHash());
}

TEST(LockstepHostTest, RollbackDepthIsBounded)
{
    LockstepConfig config;
    config.rollbackMaxFrames = 2;
    config.inputDelayFrames = 0;
    SimpleDeterministicSim sim(2);
    NetworkHost host(ServerHostConfig());
    LockstepHost lockstep(host, sim, config);
    ASSERT_EQ(lockstep.Start(), LockstepResult::Ok);
    lockstep.SetAuthority(true);
    lockstep.SetPlayerCount(1);
    lockstep.SetLocalPlayer(0);

    for (LockstepFrame frame = 0; frame < 6; ++frame) {
        const uint8_t input = 1;
        lockstep.SubmitInput(std::span<const uint8_t>(&input, 1));
        lockstep.Update(1.0 / 30.0);
    }
    const uint8_t corrected = 9;
    EXPECT_EQ(lockstep.ApplyCorrection(0, 0, std::span<const uint8_t>(&corrected, 1)),
              LockstepResult::RollbackDepthExceeded);
}

TEST(LockstepHostTest, DivergenceIsDetectedAndResynced)
{
    LockstepConfig config;
    config.hashCadenceFrames = 3;
    config.inputDelayFrames = 0;

    Peer authority(NetworkRole::Server, config);
    authority.host.SetServer(true);
    authority.host.Listen(NetworkAddress::Parse("loopback:9510"));
    ASSERT_EQ(authority.lockstep.Start(), LockstepResult::Ok);
    authority.lockstep.SetAuthority(true);
    authority.lockstep.SetPlayerCount(2);
    authority.lockstep.SetLocalPlayer(0);
    authority.sim.InjectDivergence(0);   // authority stays correct

    Peer client(NetworkRole::Client, config);
    ASSERT_EQ(client.lockstep.Start(), LockstepResult::Ok);
    client.lockstep.SetAuthority(false);
    client.lockstep.SetPlayerCount(2);
    client.lockstep.SetLocalPlayer(1);
    client.host.Connect(NetworkAddress::Parse("loopback:9510"));
    client.sim.InjectDivergence(1);      // client diverges once

    for (int i = 0; i < 10; ++i) {
        authority.Update(0.0);
        client.Update(0.0);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && client.lockstep.ResyncCount() == 0) {
        const uint8_t a = 1;
        const uint8_t b = 2;
        authority.lockstep.SubmitInput(std::span<const uint8_t>(&a, 1));
        client.lockstep.SubmitInput(std::span<const uint8_t>(&b, 1));
        authority.Update(1.0 / 30.0);
        client.Update(1.0 / 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_GT(client.lockstep.DesyncCount(), 0u);
    EXPECT_GT(client.lockstep.ResyncCount(), 0u);
}
