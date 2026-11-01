#include "platform/NativeTextInputState.h"

#include <gtest/gtest.h>
#include <limits>
#include <thread>
#include <vector>

using namespace sky;

class NativeTextInputStateTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        state = NativeTextInputState::Get();
        state->SetEnabled(false);
        state->Cancel();
        state->SetEnabled(true);
        owner = std::this_thread::get_id();
    }
    void TearDown() override
    {
        state->SetEnabled(false);
        state->Cancel();
        state->SetEnabled(true);
    }
    NativeTextInputState             *state = nullptr;
    std::vector<NativeTextInputEvent> received;
    NativeTextInputEvent              receivedEvent;
    std::thread::id                   owner;
    uint64_t                          next     = 0;
    unsigned                          count    = 0;
    bool                              notified = false;
};

TEST_F(NativeTextInputStateTest, PreservesOwnerThreadAndDeduplicatesCommittedSnapshots)
{
    const auto token = state->Begin({}, [this](const NativeTextInputEvent &event) {
        EXPECT_EQ(std::this_thread::get_id(), owner);
        received.push_back(event);
    });
    ASSERT_NE(token, 0U);
    std::thread ui([&] {
        EXPECT_TRUE(state->Post(token, {NativeTextInputEventType::CHANGED, "hello", 0}));
        EXPECT_TRUE(state->Post(token, {NativeTextInputEventType::CHANGED, "hello", 0}));
        EXPECT_TRUE(state->Post(token, {NativeTextInputEventType::CONFIRMED, "hello", 0}));
    });
    ui.join();
    EXPECT_TRUE(received.empty());
    state->Dispatch();
    ASSERT_EQ(received.size(), 2U);
    EXPECT_EQ(received[0].type, NativeTextInputEventType::CHANGED);
    EXPECT_EQ(received[1].type, NativeTextInputEventType::CONFIRMED);
}

TEST_F(NativeTextInputStateTest, RejectsBusyRequestsAndOldUiCallbacksAcrossSessions)
{
    const auto first = state->Begin({}, [this](const NativeTextInputEvent &) { ++count; });
    ASSERT_NE(first, 0U);
    EXPECT_EQ(state->Begin({}, [](const NativeTextInputEvent &) {}), 0U);
    state->Cancel();
    EXPECT_EQ(count, 1U);
    next = state->Begin({}, [this](const NativeTextInputEvent &) { ++count; });
    ASSERT_NE(next, first);
    EXPECT_FALSE(state->Post(first, {NativeTextInputEventType::CONFIRMED, "old", 0}));
    EXPECT_FALSE(state->Post(first, {NativeTextInputEventType::ERROR, "\xF0\x80", 42}));
    EXPECT_TRUE(state->Post(next, {NativeTextInputEventType::CONFIRMED, "new", 0}));
    EXPECT_FALSE(state->Post(next, {NativeTextInputEventType::CANCELLED, "late", 0}));
    state->Dispatch();
    EXPECT_EQ(count, 2U);
}

TEST_F(NativeTextInputStateTest, CancellationFlushesChangesAndUsesLatestQueuedText)
{
    const auto token = state->Begin({}, [this](const NativeTextInputEvent &event) { received.push_back(event); });
    ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::CHANGED, "typed", 0}));
    state->Cancel();
    ASSERT_EQ(received.size(), 2U);
    EXPECT_EQ(received.back().type, NativeTextInputEventType::CANCELLED);
    EXPECT_EQ(received.back().text, "typed");
    state->Cancel();
    EXPECT_EQ(received.size(), 2U);
}

TEST_F(NativeTextInputStateTest, ClosingCollectsAnAlreadyQueuedConfirmation)
{
    const auto token = state->Begin({}, [this](const NativeTextInputEvent &event) { received.push_back(event); });
    ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::CONFIRMED, "final", 0}));
    state->Cancel();
    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front().type, NativeTextInputEventType::CONFIRMED);
    EXPECT_EQ(state->ActiveToken(), 0U);
}

TEST_F(NativeTextInputStateTest, TerminalCallbackCanOpenTheNextRequest)
{
    const auto first = state->Begin({}, [this](const NativeTextInputEvent &) { next = state->Begin({}, [](const NativeTextInputEvent &) {}); });
    ASSERT_TRUE(state->Post(first, {NativeTextInputEventType::CONFIRMED, "", 0}));
    state->Dispatch();
    EXPECT_NE(next, 0U);
    EXPECT_NE(next, first);
}

TEST_F(NativeTextInputStateTest, SessionShutdownPreventsReentrantDialogs)
{
    next = 1;
    ASSERT_NE(state->Begin({}, [this](const NativeTextInputEvent &) { next = state->Begin({}, [](const NativeTextInputEvent &) {}); }), 0U);
    state->SetEnabled(false);
    state->Cancel();
    EXPECT_EQ(next, 0U);
    EXPECT_EQ(state->ActiveToken(), 0U);
}

TEST_F(NativeTextInputStateTest, UiCancellationWaitsForOwnerDispatch)
{
    ASSERT_NE(state->Begin({}, [this](const NativeTextInputEvent &) { notified = true; }), 0U);
    std::thread ui([&] { state->RequestCancel(); });
    ui.join();
    EXPECT_FALSE(notified);
    state->Dispatch();
    EXPECT_TRUE(notified);
}

TEST_F(NativeTextInputStateTest, RejectsInvalidOptionsAndCountsUnicodeScalars)
{
    NativeTextInputOptions options;
    options.multiline = options.password = true;
    const auto callback                  = [](const NativeTextInputEvent &) {};
    EXPECT_EQ(state->Begin(options, callback), 0U);
    options.multiline = options.password = false;
    options.text                         = "\xF0\x9F\x98\x80";
    options.maxLength                    = 1;
    ASSERT_NE(state->Begin(options, callback), 0U);
    state->Cancel();
    options.text += "a";
    EXPECT_EQ(state->Begin(options, callback), 0U);
    options.text = "\xF0\x80";
    EXPECT_EQ(state->Begin(options, callback), 0U);
    options.text = std::string("a\0b", 3);
    EXPECT_EQ(state->Begin(options, callback), 0U);
}

TEST_F(NativeTextInputStateTest, RejectsEnumBoundaryAndPreservesNativeError)
{
    const auto token = state->Begin({}, [this](const NativeTextInputEvent &event) { receivedEvent = event; });
    EXPECT_FALSE(state->Post(token, {static_cast<NativeTextInputEventType>(-1), "\xF0\x80", 0}));
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::END, std::string("a\0b", 3), 0}));
    EXPECT_FALSE(state->Post(token, {static_cast<NativeTextInputEventType>(static_cast<int>(NativeTextInputEventType::END) + 1), "", 0}));
    EXPECT_TRUE(state->Post(token, {NativeTextInputEventType::ERROR, "", 42}));
    state->Dispatch();
    EXPECT_EQ(receivedEvent.type, NativeTextInputEventType::ERROR);
    EXPECT_EQ(receivedEvent.nativeError, 42);
}

TEST_F(NativeTextInputStateTest, WrongThreadCannotConsumePendingEvents)
{
    const auto token = state->Begin({}, [this](const NativeTextInputEvent &event) {
        EXPECT_EQ(std::this_thread::get_id(), owner);
        received.push_back(event);
    });
    ASSERT_NE(token, 0U);
    ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::CONFIRMED, "final", 0}));
    std::thread ui([this] { state->Dispatch(); });
    ui.join();
    EXPECT_TRUE(received.empty());
    EXPECT_EQ(state->ActiveToken(), token);
    state->Dispatch();
    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front().text, "final");
    EXPECT_EQ(state->ActiveToken(), 0U);
}

TEST_F(NativeTextInputStateTest, ChangeCallbackCanCancelAndOpenAnotherRequest)
{
    const auto first = state->Begin({}, [this](const NativeTextInputEvent &event) {
        received.push_back(event);
        if (event.type == NativeTextInputEventType::CHANGED) {
            state->Cancel();
            next = state->Begin({}, [this](const NativeTextInputEvent &nextEvent) { received.push_back(nextEvent); });
        }
    });
    ASSERT_NE(first, 0U);
    ASSERT_TRUE(state->Post(first, {NativeTextInputEventType::CHANGED, "typed", 0}));
    state->Dispatch();
    ASSERT_EQ(received.size(), 2U);
    EXPECT_EQ(received.back().type, NativeTextInputEventType::CANCELLED);
    ASSERT_NE(next, 0U);
    EXPECT_NE(next, first);
    EXPECT_FALSE(state->Post(first, {NativeTextInputEventType::CONFIRMED, "late", 0}));
    ASSERT_TRUE(state->Post(next, {NativeTextInputEventType::CONFIRMED, "new", 0}));
    state->Dispatch();
    ASSERT_EQ(received.size(), 3U);
    EXPECT_EQ(received.back().text, "new");
}

TEST_F(NativeTextInputStateTest, RacingUiConfirmationAndCancellationDeliverOneTerminalEvent)
{
    const auto token = state->Begin({}, [this](const NativeTextInputEvent &event) { received.push_back(event); });
    ASSERT_NE(token, 0U);
    std::thread confirm([this, token] { state->Post(token, {NativeTextInputEventType::CONFIRMED, "final", 0}); });
    std::thread cancel([this] { state->RequestCancel(); });
    confirm.join();
    cancel.join();
    EXPECT_TRUE(received.empty());
    state->Cancel();
    state->Dispatch();
    ASSERT_EQ(received.size(), 1U);
    EXPECT_TRUE(received.front().type == NativeTextInputEventType::CONFIRMED || received.front().type == NativeTextInputEventType::CANCELLED);
    EXPECT_EQ(state->ActiveToken(), 0U);
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::CANCELLED, "late", 0}));
}

TEST_F(NativeTextInputStateTest, InvalidUiChangesDoNotReplaceTheLastValidSnapshotOrCloseTheRequest)
{
    NativeTextInputOptions options;
    options.maxLength = 1;
    const auto token  = state->Begin(options, [this](const NativeTextInputEvent &event) { received.push_back(event); });
    ASSERT_NE(token, 0U);
    ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::CHANGED, "\xF0\x9F\x98\x80", 0}));
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::CHANGED, "ab", 0}));
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::CHANGED, "\xF0\x80", 0}));
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::CHANGED, std::string("a\0b", 3), 0}));
    state->Cancel();
    ASSERT_EQ(received.size(), 2U);
    EXPECT_EQ(received.front().text, "\xF0\x9F\x98\x80");
    EXPECT_EQ(received.back().type, NativeTextInputEventType::CANCELLED);
    EXPECT_EQ(received.back().text, received.front().text);
}

TEST_F(NativeTextInputStateTest, InvalidTerminalSnapshotsFinishOnceWithTheLastValidText)
{
    const NativeTextInputEvent invalidTerminals[] = {
        {NativeTextInputEventType::CONFIRMED, "\xF0\x80", 42},
        {NativeTextInputEventType::CANCELLED, std::string("a\0b", 3), 0},
        {NativeTextInputEventType::ERROR, "ab", 0},
    };
    NativeTextInputOptions options;
    options.maxLength = 1;
    options.password  = true;
    for (const auto &terminal : invalidTerminals) {
        received.clear();
        const auto token = state->Begin(options, [this](const NativeTextInputEvent &event) { received.push_back(event); });
        ASSERT_NE(token, 0U);
        ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::CHANGED, "\xF0\x9F\x98\x80", 0}));
        EXPECT_TRUE(state->Post(token, terminal));
        EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::CONFIRMED, "x", 0}));
        state->Cancel();
        state->Dispatch();
        ASSERT_EQ(received.size(), 2U);
        EXPECT_EQ(received.back().type, NativeTextInputEventType::ERROR);
        EXPECT_EQ(received.back().nativeError, -2);
        EXPECT_EQ(received.back().text, received.front().text);
        EXPECT_EQ(received.back().text, "\xF0\x9F\x98\x80");
        EXPECT_EQ(state->ActiveToken(), 0U);
        const auto reopened = state->Begin({}, [this](const NativeTextInputEvent &event) { received.push_back(event); });
        ASSERT_NE(reopened, 0U);
        EXPECT_NE(reopened, token);
        EXPECT_FALSE(state->Post(token, terminal));
        ASSERT_TRUE(state->Post(reopened, {NativeTextInputEventType::CONFIRMED, "next", 0}));
        state->Dispatch();
        ASSERT_EQ(received.size(), 3U);
        EXPECT_EQ(received.back().type, NativeTextInputEventType::CONFIRMED);
        EXPECT_EQ(received.back().text, "next");
    }
}

TEST_F(NativeTextInputStateTest, InvalidErrorSnapshotKeepsNativeErrorAndAllowsReentrantShow)
{
    NativeTextInputOptions options;
    options.text      = "a";
    options.maxLength = 1;
    options.password  = true;
    const auto token  = state->Begin(options, [this](const NativeTextInputEvent &event) {
        receivedEvent = event;
        ++count;
        next = state->Begin({}, [](const NativeTextInputEvent &) {});
    });
    ASSERT_NE(token, 0U);
    ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::ERROR, "\xED\xA0\x80", 42}));
    state->Cancel();
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(receivedEvent.type, NativeTextInputEventType::ERROR);
    EXPECT_EQ(receivedEvent.nativeError, 42);
    EXPECT_EQ(receivedEvent.text, "a");
    EXPECT_NE(next, 0U);
    EXPECT_NE(next, token);
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::ERROR, "", 17}));
}

TEST_F(NativeTextInputStateTest, FailurePreservesQueuedChangesCompletesOnceAndRejectsRetiredTokens)
{
    NativeTextInputOptions options;
    options.text     = "initial";
    options.password = true;
    const auto token = state->Begin(options, [this](const NativeTextInputEvent &event) { received.push_back(event); });
    ASSERT_NE(token, 0U);
    ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::CHANGED, "queued", 0}));
    ASSERT_TRUE(state->Post(token, {NativeTextInputEventType::CHANGED, "latest", 0}));
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::CHANGED, "\xF0\x80", 0}));
    std::thread ui([this, token] {
        EXPECT_TRUE(state->Fail(token, 42));
        EXPECT_FALSE(state->Fail(token, 43));
    });
    ui.join();
    EXPECT_TRUE(received.empty());
    EXPECT_FALSE(state->Post(token, {NativeTextInputEventType::CHANGED, "late", 0}));
    state->Dispatch();
    ASSERT_EQ(received.size(), 3U);
    EXPECT_EQ(received[0].text, "queued");
    EXPECT_EQ(received[1].text, "latest");
    EXPECT_EQ(received.back().type, NativeTextInputEventType::ERROR);
    EXPECT_EQ(received.back().text, "latest");
    EXPECT_EQ(received.back().nativeError, 42);
    EXPECT_EQ(state->ActiveToken(), 0U);
    EXPECT_FALSE(state->Fail(token, 44));
    const auto reopened = state->Begin({}, [this](const NativeTextInputEvent &event) { received.push_back(event); });
    ASSERT_NE(reopened, 0U);
    EXPECT_NE(reopened, token);
    EXPECT_FALSE(state->Fail(token, 45));
    EXPECT_FALSE(state->Fail(0, 46));
    ASSERT_TRUE(state->Post(reopened, {NativeTextInputEventType::CONFIRMED, "new request", 0}));
    state->Dispatch();
    ASSERT_EQ(received.size(), 4U);
    EXPECT_EQ(received.back().type, NativeTextInputEventType::CONFIRMED);
    EXPECT_EQ(received.back().text, "new request");
}

TEST_F(NativeTextInputStateTest, InvalidMetadataAndLengthLimitsLeaveTheReceiverAvailable)
{
    NativeTextInputOptions options;
    const auto             callback = [](const NativeTextInputEvent &) {};
    EXPECT_EQ(state->Begin(options, {}), 0U);
    options.placeholder = "\xED\xA0\x80";
    EXPECT_EQ(state->Begin(options, callback), 0U);
    options.placeholder.clear();
    options.title = std::string("a\0b", 3);
    EXPECT_EQ(state->Begin(options, callback), 0U);
    options.title.clear();
    options.maxLength = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(state->Begin(options, callback), 0U);
    options.maxLength = 0;
    ASSERT_NE(state->Begin(options, callback), 0U);
}
