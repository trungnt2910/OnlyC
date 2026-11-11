#include <gtest/gtest.h>

extern "C"
{
#include "app/model/ApplicationState.h"
}

static void count_change(void* context, ApplicationChange)
{
    ++*(int*)context;
}

typedef struct
{
    ApplicationSubscription* subscription;
    int calls;
} DetachingObserver;

static void detach_observer(void* context, ApplicationChange)
{
    DetachingObserver* observer = (DetachingObserver*)context;
    ++observer->calls;
    application_state_unsubscribe(observer->subscription);
}

class Given_ApplicationState : public ::testing::Test
{
protected:
    ApplicationState state = {};
    ApplicationSubscription first = {}, second = {}, third = {};
    int first_calls = 0, second_calls = 0, third_calls = 0;

    void SetUp() override
    {
        static const char* sources[] = {""};
        static const char* titles[] = {"Example"};
        static const char* slugs[] = {"example"};
        ASSERT_TRUE(application_state_init(&state, {1, sources, titles, slugs}));
    }

    void TearDown() override
    {
        application_state_destroy(&state);
    }
};

TEST_F(Given_ApplicationState, When_MultipleViewsSubscribe_Then_AllReceiveChanges)
{
    application_state_subscribe(&state, &first, count_change, &first_calls);
    application_state_subscribe(&state, &second, count_change, &second_calls);

    application_state_notify(&state, APPLICATION_RENDER);

    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 1);
}

TEST_F(Given_ApplicationState, When_OneViewDetaches_Then_OtherViewsRemainSubscribed)
{
    application_state_subscribe(&state, &first, count_change, &first_calls);
    application_state_subscribe(&state, &second, count_change, &second_calls);

    application_state_unsubscribe(&first);
    application_state_notify(&state, APPLICATION_RENDER);

    EXPECT_EQ(first_calls, 0);
    EXPECT_EQ(second_calls, 1);
    EXPECT_EQ(first.state, nullptr);
}

TEST_F(Given_ApplicationState, When_ViewDetachesItself_Then_RemainingObserversStillRun)
{
    DetachingObserver observer = {&first, 0};
    application_state_subscribe(&state, &first, detach_observer, &observer);
    application_state_subscribe(&state, &second, count_change, &second_calls);

    application_state_notify(&state, APPLICATION_RENDER);
    application_state_notify(&state, APPLICATION_RENDER);

    EXPECT_EQ(observer.calls, 1);
    EXPECT_EQ(second_calls, 2);
}

TEST_F(Given_ApplicationState, When_NextObserverDetaches_Then_LaterObserversStillRun)
{
    DetachingObserver observer = {&second, 0};
    application_state_subscribe(&state, &first, detach_observer, &observer);
    application_state_subscribe(&state, &second, count_change, &second_calls);
    application_state_subscribe(&state, &third, count_change, &third_calls);

    application_state_notify(&state, APPLICATION_RENDER);

    EXPECT_EQ(observer.calls, 1);
    EXPECT_EQ(second_calls, 0);
    EXPECT_EQ(third_calls, 1);
}

TEST_F(Given_ApplicationState, When_RegistrationReused_Then_ObserverIsCalledOnce)
{
    application_state_subscribe(&state, &first, count_change, &first_calls);

    application_state_subscribe(&state, &first, count_change, &first_calls);
    application_state_notify(&state, APPLICATION_RENDER);

    EXPECT_EQ(first_calls, 1);
}

TEST_F(Given_ApplicationState, When_StateDestroyed_Then_RegistrationsAreDetached)
{
    application_state_subscribe(&state, &first, count_change, &first_calls);
    application_state_subscribe(&state, &second, count_change, &second_calls);

    application_state_destroy(&state);

    EXPECT_EQ(first.state, nullptr);
    EXPECT_EQ(second.state, nullptr);
    EXPECT_EQ(state.subscriptions, nullptr);
}
