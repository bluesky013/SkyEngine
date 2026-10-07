//
// Created on 2026/10/07.
//

#include <editor/core/play/PlaySession.h>

#include <framework/world/World.h>

#include <gtest/gtest.h>

using namespace sky::editor;

TEST(PlaySessionTest, StateMachine)
{
    PlaySession session;

    int created = 0;
    session.SetWorldFactory([&created]() {
        ++created;
        sky::WorldPtr world = sky::World::CreateWorld();
        world->Init();
        return world;
    });

    EXPECT_EQ(session.GetState(), PlayState::Editing);
    EXPECT_EQ(session.GetWorld(), nullptr);

    EXPECT_TRUE(session.Play());
    EXPECT_EQ(session.GetState(), PlayState::Playing);
    EXPECT_EQ(created, 1);
    EXPECT_NE(session.GetWorld(), nullptr);

    EXPECT_TRUE(session.Pause());
    EXPECT_EQ(session.GetState(), PlayState::Paused);

    // Resume continues the same runtime world (no re-duplication).
    EXPECT_TRUE(session.Play());
    EXPECT_EQ(session.GetState(), PlayState::Playing);
    EXPECT_EQ(created, 1);

    EXPECT_TRUE(session.Stop());
    EXPECT_EQ(session.GetState(), PlayState::Editing);
    EXPECT_EQ(session.GetWorld(), nullptr);
}

TEST(PlaySessionTest, PlayWithoutWorldFails)
{
    PlaySession session; // no factory set
    EXPECT_FALSE(session.Play());
    EXPECT_EQ(session.GetState(), PlayState::Editing);
}

TEST(PlaySessionTest, TickOnlyAdvancesWhilePlaying)
{
    PlaySession session;
    session.SetWorldFactory([]() {
        sky::WorldPtr world = sky::World::CreateWorld();
        world->Init();
        return world;
    });

    session.Tick(1.0f); // Editing: no advance
    EXPECT_FLOAT_EQ(session.GetTime(), 0.0f);

    session.Play();
    session.Tick(0.5f);
    EXPECT_FLOAT_EQ(session.GetTime(), 0.5f);

    session.Pause();
    session.Tick(0.5f); // Paused: no advance
    EXPECT_FLOAT_EQ(session.GetTime(), 0.5f);
}
