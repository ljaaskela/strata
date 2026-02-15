#include <gtest/gtest.h>

#include <api/any.h>
#include <api/function.h>
#include <api/function_context.h>
#include <api/strata.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>
#include <interface/types.h>

using namespace strata;

// --- FnArgs ---

TEST(FnArgs, DefaultIsEmpty)
{
    FnArgs args;
    EXPECT_EQ(args.count, 0u);
    EXPECT_TRUE(args.empty());
    EXPECT_EQ(args.begin(), args.end());
}

TEST(FnArgs, IndexOutOfBoundsReturnsNullptr)
{
    FnArgs args;
    EXPECT_EQ(args[0], nullptr);
    EXPECT_EQ(args[100], nullptr);
}

TEST(FnArgs, IndexInBoundsReturnsPointer)
{
    Any<int> a(42);
    const IAny* ptrs[] = {a};
    FnArgs args{ptrs, 1};
    EXPECT_NE(args[0], nullptr);
    EXPECT_EQ(args[1], nullptr);
}

TEST(FnArgs, Iteration)
{
    Any<int> a(1);
    Any<float> b(2.f);
    const IAny* ptrs[] = {a, b};
    FnArgs args{ptrs, 2};

    int count = 0;
    for (auto* arg : args) {
        EXPECT_NE(arg, nullptr);
        count++;
    }
    EXPECT_EQ(count, 2);
}

// --- Function ---

namespace {
    static bool s_fnCalled = false;
}

TEST(Function, LambdaCallbackInvoked)
{
    s_fnCalled = false;

    Function fn([](FnArgs) -> ReturnValue {
        s_fnCalled = true;
        return ReturnValue::SUCCESS;
    });

    auto result = fn.invoke();
    EXPECT_TRUE(s_fnCalled);
    EXPECT_EQ(result, ReturnValue::SUCCESS);
}

namespace {
    static float s_fnReceived = 0.f;
}

TEST(Function, InvokeWithArgs)
{
    s_fnReceived = 0.f;

    Function fn([](FnArgs args) -> ReturnValue {
        if (auto v = Any<const float>(args[0])) {
            s_fnReceived = v.get_value();
        }
        return ReturnValue::SUCCESS;
    });

    Any<float> arg(3.14f);
    const IAny* ptrs[] = {arg};
    fn.invoke(FnArgs{ptrs, 1});
    EXPECT_FLOAT_EQ(s_fnReceived, 3.14f);
}

// --- invoke_function variadic with values ---

namespace {
    static float s_varA = 0.f;
    static int s_varB = 0;
}

TEST(InvokeFunction, VariadicWithValues)
{
    s_varA = 0.f;
    s_varB = 0;

    Function fn([](FnArgs args) -> ReturnValue {
        if (auto a = Any<const float>(args[0])) s_varA = a.get_value();
        if (auto b = Any<const int>(args[1])) s_varB = b.get_value();
        return ReturnValue::SUCCESS;
    });

    invoke_function(IFunction::Ptr(fn), 10.f, 20);
    EXPECT_FLOAT_EQ(s_varA, 10.f);
    EXPECT_EQ(s_varB, 20);
}

// --- invoke_function variadic with IAny* pointers ---

namespace {
    static float s_ptrA = 0.f;
    static int s_ptrB = 0;
}

TEST(InvokeFunction, VariadicWithAnyPointers)
{
    s_ptrA = 0.f;
    s_ptrB = 0;

    Function fn([](FnArgs args) -> ReturnValue {
        if (auto a = Any<const float>(args[0])) s_ptrA = a.get_value();
        if (auto b = Any<const int>(args[1])) s_ptrB = b.get_value();
        return ReturnValue::SUCCESS;
    });

    Any<float> arg0(5.f);
    Any<int> arg1(7);
    invoke_function(IFunction::Ptr(fn), arg0, arg1);
    EXPECT_FLOAT_EQ(s_ptrA, 5.f);
    EXPECT_EQ(s_ptrB, 7);
}

// --- FunctionContext ---

TEST(FunctionContext, DefaultIsEmpty)
{
    FunctionContext ctx;
    EXPECT_FALSE(ctx);
    EXPECT_EQ(ctx.size(), 0u);
}

TEST(FunctionContext, MatchingCountAccepts)
{
    Any<int> a(1);
    Any<int> b(2);
    const IAny* ptrs[] = {a, b};
    FnArgs args{ptrs, 2};

    FunctionContext ctx(args, 2);
    EXPECT_TRUE(ctx);
    EXPECT_EQ(ctx.size(), 2u);
}

TEST(FunctionContext, MismatchedCountRejects)
{
    Any<int> a(1);
    const IAny* ptrs[] = {a};
    FnArgs args{ptrs, 1};

    FunctionContext ctx(args, 2);
    EXPECT_FALSE(ctx);
    EXPECT_EQ(ctx.size(), 0u);
}

TEST(FunctionContext, ArgTypedAccess)
{
    Any<float> a(3.14f);
    Any<int> b(42);
    const IAny* ptrs[] = {a, b};
    FnArgs args{ptrs, 2};

    FunctionContext ctx(args, 2);
    ASSERT_TRUE(ctx);

    auto fa = ctx.arg<float>(0);
    ASSERT_TRUE(fa);
    EXPECT_FLOAT_EQ(fa.get_value(), 3.14f);

    auto ib = ctx.arg<int>(1);
    ASSERT_TRUE(ib);
    EXPECT_EQ(ib.get_value(), 42);
}

TEST(FunctionContext, ArgOutOfRangeReturnsNull)
{
    Any<int> a(1);
    const IAny* ptrs[] = {a};
    FnArgs args{ptrs, 1};

    FunctionContext ctx(args, 1);
    EXPECT_EQ(ctx.arg(5), nullptr);
}

// --- Event handler add/remove ---

namespace {
    static int s_eventCallCount = 0;
}

TEST(Event, HandlerAddRemove)
{
    s_eventCallCount = 0;

    auto event = instance().create<IEvent>(ClassId::Event);
    ASSERT_TRUE(event);

    Function handler([](FnArgs) -> ReturnValue {
        s_eventCallCount++;
        return ReturnValue::SUCCESS;
    });

    event->add_handler(handler);
    EXPECT_TRUE(event->has_handlers());

    event->invoke({});
    EXPECT_EQ(s_eventCallCount, 1);

    event->remove_handler(handler);
    EXPECT_FALSE(event->has_handlers());

    event->invoke({});
    EXPECT_EQ(s_eventCallCount, 1); // no longer called
}

// --- Deferred invocation ---

namespace {
    static int s_deferredCallCount = 0;
}

TEST(Function, DeferredInvocationQueuesAndExecutesOnUpdate)
{
    s_deferredCallCount = 0;

    Function fn([](FnArgs) -> ReturnValue {
        s_deferredCallCount++;
        return ReturnValue::SUCCESS;
    });

    fn.invoke({}, Deferred);
    EXPECT_EQ(s_deferredCallCount, 0); // Not called yet

    instance().update();
    EXPECT_EQ(s_deferredCallCount, 1); // Now called
}
