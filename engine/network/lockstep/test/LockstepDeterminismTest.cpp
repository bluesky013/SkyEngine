//
// Created on 2026/10/01.
//

#include <network/lockstep/DeterministicRng.h>
#include <network/lockstep/LockstepConfig.h>
#include <network/lockstep/LockstepHost.h>

#include "SimpleDeterministicSim.h"

#include <gtest/gtest.h>

using namespace sky::net;
using namespace sky::net::test;

TEST(LockstepDeterminismTest, RejectsNonDeterministicMathMode)
{
    LockstepConfig config;
    config.mathMode = DeterministicMathMode::Fast;
    EXPECT_EQ(ValidateLockstepConfig(config), LockstepResult::NonDeterministicMathMode);

    config.mathMode = DeterministicMathMode::Exact;
    EXPECT_EQ(ValidateLockstepConfig(config), LockstepResult::Ok);
}

TEST(LockstepDeterminismTest, RngIsReproducible)
{
    DeterministicRng a;
    DeterministicRng b;
    a.Seed(12345);
    b.Seed(12345);
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(a.NextU32(), b.NextU32());
    }
}

TEST(LockstepDeterminismTest, IdenticalInputsProduceIdenticalState)
{
    SimpleDeterministicSim a(4);
    SimpleDeterministicSim b(4);

    std::vector<std::vector<uint8_t>> inputs = {{1, 2}, {3}, {4, 5, 6}};
    for (LockstepFrame frame = 0; frame < 20; ++frame) {
        a.Advance(frame, inputs);
        b.Advance(frame, inputs);
    }
    EXPECT_EQ(a.StateHash(), b.StateHash());
}

TEST(LockstepDeterminismTest, RollbackRequiresDeterministicMode)
{
    NetworkHostConfig hostConfig;
    hostConfig.role = NetworkRole::Client;
    NetworkHost host(hostConfig);

    SimpleDeterministicSim sim;
    LockstepConfig config;
    config.mathMode = DeterministicMathMode::Fast;
    LockstepHost lockstep(host, sim, config);

    EXPECT_EQ(lockstep.Start(), LockstepResult::NonDeterministicMathMode);
    uint8_t input = 1;
    EXPECT_EQ(lockstep.ApplyCorrection(0, 0, std::span<const uint8_t>(&input, 1)),
              LockstepResult::RollbackUnavailable);
}

TEST(LockstepDeterminismTest, InputDelaySchedulesFutureFrame)
{
    NetworkHostConfig hostConfig;
    hostConfig.role = NetworkRole::Server;
    NetworkHost host(hostConfig);

    SimpleDeterministicSim sim;
    LockstepConfig config;
    config.inputDelayFrames = 2;
    LockstepHost lockstep(host, sim, config);
    ASSERT_EQ(lockstep.Start(), LockstepResult::Ok);
    lockstep.SetAuthority(true);
    lockstep.SetPlayerCount(1);
    lockstep.SetLocalPlayer(0);

    EXPECT_EQ(lockstep.NextSubmitFrame(), 2u);
    const uint8_t input = 1;
    lockstep.SubmitInput(std::span<const uint8_t>(&input, 1));
    EXPECT_EQ(lockstep.NextSubmitFrame(), 3u);
}
