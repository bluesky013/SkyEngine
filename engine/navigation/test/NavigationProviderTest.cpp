//
// Created on 2026/09/22.
//

#include <navigation/NavigationSystem.h>
#include <navigation/NaviGeometryProvider.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::ai;

namespace {

    class FakeProvider : public INaviGeometryProvider {
    public:
        bool Collect(const AABB &bounds, INaviGeometrySink &sink) override { return true; }
    };

} // namespace

TEST(NavigationProviderTest, RegisterDedupeAndRemove)
{
    NavigationSystem system;
    FakeProvider    a;
    FakeProvider    b;

    EXPECT_TRUE(system.GetGeometryProviders().empty());

    system.AddGeometryProvider(&a);
    system.AddGeometryProvider(&a);
    system.AddGeometryProvider(nullptr);
    system.AddGeometryProvider(&b);

    ASSERT_EQ(system.GetGeometryProviders().size(), 2u);
    EXPECT_EQ(system.GetGeometryProviders()[0], &a);
    EXPECT_EQ(system.GetGeometryProviders()[1], &b);

    system.RemoveGeometryProvider(&a);
    ASSERT_EQ(system.GetGeometryProviders().size(), 1u);
    EXPECT_EQ(system.GetGeometryProviders()[0], &b);
}
