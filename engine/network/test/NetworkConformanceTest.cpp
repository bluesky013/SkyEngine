//
// Created on 2026/09/25.
//

#include "Conformance.h"
#include "Harness.h"
#include "LoopbackBackend.h"

#include <gtest/gtest.h>
#include <memory>

using namespace sky::net::test;

TEST(NetworkConformanceTest, LoopbackBackend)
{
    RunBackendConformance(
        []() -> std::unique_ptr<sky::net::INetBackend> { return std::make_unique<LoopbackBackend>(); },
        []() -> std::unique_ptr<sky::net::INetBackend> { return std::make_unique<LoopbackBackend>(); },
        LOOPBACK_ADDR);
}
