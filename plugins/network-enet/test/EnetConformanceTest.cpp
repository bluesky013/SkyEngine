//
// Created on 2026/09/25.
//

#include <network/enet/EnetBackend.h>

#include "Conformance.h"

#include <gtest/gtest.h>
#include <memory>

using namespace sky::net;
using namespace sky::net::test;

TEST(EnetNetworkTest, ConformanceOverUdp)
{
    RunBackendConformance(
        []() -> std::unique_ptr<INetBackend> { return std::make_unique<EnetBackend>(); },
        []() -> std::unique_ptr<INetBackend> { return std::make_unique<EnetBackend>(); },
        "127.0.0.1:39555");
}
