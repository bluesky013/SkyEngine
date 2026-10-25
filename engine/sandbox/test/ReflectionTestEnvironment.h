#pragma once

#include <gtest/gtest.h>

namespace sky::editor::test {
    class ReflectionTestEnvironment final : public testing::Environment {
    public:
        explicit ReflectionTestEnvironment(void (*registerTypes)()) : registerTypes(registerTypes)
        {
        }
        void SetUp() override
        {
            registerTypes();
        }

    private:
        void (*registerTypes)();
    };

    inline testing::Environment *AddReflectionEnvironment(void (*registerTypes)())
    {
        return testing::AddGlobalTestEnvironment(new ReflectionTestEnvironment(registerTypes));
    }
} // namespace sky::editor::test
