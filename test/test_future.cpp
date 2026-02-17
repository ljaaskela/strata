#include <gtest/gtest.h>

#include <api/any.h>
#include <api/callback.h>
#include <api/future.h>
#include <api/strata.h>
#include <interface/intf_future.h>
#include <interface/types.h>

#include <thread>

using namespace strata;

// --- Basic Promise/Future ---

TEST(Future, CreatePair)
{
    auto promise = make_promise();
    ASSERT_TRUE(promise);

    auto future = promise.get_future<int>();
    ASSERT_TRUE(future);
}

TEST(Future, InitiallyNotReady)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    EXPECT_FALSE(future.is_ready());
}

TEST(Future, SetValueMakesReady)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    auto ret = promise.set_value(42);
    EXPECT_EQ(ret, ReturnValue::SUCCESS);
    EXPECT_TRUE(future.is_ready());
}

TEST(Future, GetValueReturnsSetValue)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    promise.set_value(42);
    EXPECT_EQ(future.get_result().get_value(), 42);
}

TEST(Future, GetValueBlocksUntilReady)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    std::thread writer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        promise.set_value(99);
    });

    // This will block until the writer thread sets the value
    int val = future.get_result().get_value();
    EXPECT_EQ(val, 99);

    writer.join();
}

TEST(Future, DoubleSetReturnsNothingToDo)
{
    auto promise = make_promise();

    auto ret1 = promise.set_value(1);
    EXPECT_EQ(ret1, ReturnValue::SUCCESS);

    auto ret2 = promise.set_value(2);
    EXPECT_EQ(ret2, ReturnValue::NOTHING_TO_DO);

    // First value persists
    auto future = promise.get_future<int>();
    EXPECT_EQ(future.get_result().get_value(), 1);
}

// --- Continuations ---

TEST(Future, ImmediateContinuationFiresOnSet)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    bool called = false;
    future.then([&](FnArgs) -> ReturnValue {
        called = true;
        return ReturnValue::SUCCESS;
    });

    EXPECT_FALSE(called);
    promise.set_value(42);
    EXPECT_TRUE(called);
}

TEST(Future, ImmediateContinuationFiresWhenAlreadyReady)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    promise.set_value(42);

    bool called = false;
    future.then([&](FnArgs) -> ReturnValue {
        called = true;
        return ReturnValue::SUCCESS;
    });

    EXPECT_TRUE(called);
}

TEST(Future, DeferredContinuationQueuesAndFiresOnUpdate)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    bool called = false;
    future.then([&](FnArgs) -> ReturnValue {
        called = true;
        return ReturnValue::SUCCESS;
    }, Deferred);

    promise.set_value(42);
    EXPECT_FALSE(called); // Deferred, not yet called

    instance().update();
    EXPECT_TRUE(called);
}

TEST(Future, ContinuationReceivesValue)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    int received = 0;
    future.then([&](FnArgs args) -> ReturnValue {
        if (auto v = Any<const int>(args[0])) {
            received = v.get_value();
        }
        return ReturnValue::SUCCESS;
    });

    promise.set_value(42);
    EXPECT_EQ(received, 42);
}

TEST(Future, ContinuationReceivesValueTyped)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    int received = 0;
    future.then([&](int val) {
        received = val;
    });

    promise.set_value(42);
    EXPECT_EQ(received, 42);
}

// --- Void future ---

TEST(Future, VoidFuture)
{
    auto promise = make_promise();
    auto future = promise.get_future<void>();

    EXPECT_FALSE(future.is_ready());

    auto ret = promise.complete();
    EXPECT_EQ(ret, ReturnValue::SUCCESS);
    EXPECT_TRUE(future.is_ready());
}

TEST(Future, VoidFutureContinuation)
{
    auto promise = make_promise();
    auto future = promise.get_future<void>();

    bool called = false;
    future.then([&](FnArgs) -> ReturnValue {
        called = true;
        return ReturnValue::SUCCESS;
    });

    promise.complete();
    EXPECT_TRUE(called);
}

// --- Multiple continuations ---

TEST(Future, MultipleContinuations)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    int count = 0;
    future.then([&](FnArgs) -> ReturnValue { count++; return ReturnValue::SUCCESS; });
    future.then([&](FnArgs) -> ReturnValue { count++; return ReturnValue::SUCCESS; });
    future.then([&](FnArgs) -> ReturnValue { count++; return ReturnValue::SUCCESS; });

    promise.set_value(1);
    EXPECT_EQ(count, 3);
}

// --- Thread safety ---

TEST(Future, WaitFromMultipleThreads)
{
    auto promise = make_promise();
    auto future = promise.get_future<int>();

    std::atomic<int> ready_count{0};
    constexpr int NUM_THREADS = 4;

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&] {
            future.wait();
            EXPECT_TRUE(future.is_ready());
            EXPECT_EQ(future.get_result().get_value(), 77);
            ready_count++;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    promise.set_value(77);

    for (auto& t : threads) t.join();
    EXPECT_EQ(ready_count.load(), NUM_THREADS);
}

// --- Then chaining ---

TEST(Future, ThenChaining)
{
    auto promise1 = make_promise();
    auto future1 = promise1.get_future<int>();

    auto promise2 = make_promise();
    auto future2 = promise2.get_future<int>();

    // When future1 resolves, resolve future2 with value + 1
    future1.then([&](int val) {
        promise2.set_value(val + 1);
    });

    promise1.set_value(10);
    EXPECT_TRUE(future2.is_ready());
    EXPECT_EQ(future2.get_result().get_value(), 11);
}

// --- Float type ---

TEST(Future, FloatValue)
{
    auto promise = make_promise();
    auto future = promise.get_future<float>();

    promise.set_value(3.14f);
    EXPECT_TRUE(future.is_ready());
    EXPECT_FLOAT_EQ(future.get_result().get_value(), 3.14f);
}
