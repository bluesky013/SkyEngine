//
// Created on 2026/10/23.
//

#include <core/platform/Platform.h>
#include <gtest/gtest.h>

TEST(BuildConfigTest, AssertionEvaluatesExpressionOnce)
{
    int evaluations = 0;
    SKY_ASSERT(++evaluations == 1);
    EXPECT_EQ(evaluations, 1);
}

TEST(BuildConfigDeathTest, FailedInvariantTerminatesWithDiagnostic)
{
#if SKY_ENABLE_ASSERT
    EXPECT_DEATH_IF_SUPPORTED({ SKY_ASSERT(false); }, "Assertion failed: false");
#else
    int evaluations = 0;
    SKY_ASSERT(++evaluations == 0);
    EXPECT_EQ(evaluations, 1);
#endif
}
