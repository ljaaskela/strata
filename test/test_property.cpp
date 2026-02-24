#include <velk/api/any.h>
#include <velk/api/callback.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_property.h>

#include <gtest/gtest.h>

using namespace velk;

// ReadWrite property

TEST(Property, DefaultConstructedHasInitialValue)
{
    auto p = create_property<float>();
    EXPECT_FLOAT_EQ(p.get_value(), 0.f);
}

TEST(Property, ConstructWithValue)
{
    auto p = create_property<int>(42);
    EXPECT_EQ(p.get_value(), 42);
}

TEST(Property, SetGetRoundTrip)
{
    auto p = create_property<float>();
    p.set_value(3.14f);
    EXPECT_FLOAT_EQ(p.get_value(), 3.14f);
}

TEST(Property, CopySemanticsShareSameIProperty)
{
    auto p = create_property<float>();
    p.set_value(10.f);
    Property<float> copy = p;
    EXPECT_FLOAT_EQ(copy.get_value(), 10.f);

    // Both copies reference the same IProperty
    p.set_value(20.f);
    EXPECT_FLOAT_EQ(copy.get_value(), 20.f);
}

TEST(Property, OnChangedEventFires)
{
    int callCount = 0;
    float receivedValue = 0.f;

    auto p = create_property<float>();

    Callback handler([&](FnArgs args) -> ReturnValue {
        callCount++;
        if (auto v = Any<const float>(args[0])) {
            receivedValue = v.get_value();
        }
        return ReturnValue::Success;
    });

    p.add_on_changed(handler);
    p.set_value(42.f);

    EXPECT_EQ(callCount, 1);
    EXPECT_FLOAT_EQ(receivedValue, 42.f);
}

TEST(Property, SetSameValueReturnsNothingToDo)
{
    auto p = create_property<int>(5);

    auto iprop = p.get_property_interface();
    ASSERT_TRUE(iprop);

    Any<int> val(5);
    auto result = iprop->set_value(val);
    EXPECT_EQ(result, ReturnValue::NothingToDo);
}

TEST(Property, SetDifferentValueReturnsSuccess)
{
    auto p = create_property<int>(5);
    ASSERT_TRUE(p);

    auto result = p.set_value(10);
    EXPECT_EQ(result, ReturnValue::Success);
    EXPECT_EQ(p.get_value(), 10);
}

TEST(Property, OnChangedDoesNotFireOnSameValue)
{
    int callCount = 0;

    auto p = create_property<int>(5);

    Callback handler([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::Success;
    });

    p.add_on_changed(handler);
    EXPECT_EQ(p.set_value(5), ReturnValue::NothingToDo); // same value
    EXPECT_EQ(callCount, 0);
}

TEST(Property, BoolConversion)
{
    auto p = create_property<float>();
    EXPECT_TRUE(p);
}

// ReadOnly property

TEST(Property, DefaultConstructedReadOnlyHasInitialValue)
{
    auto p = create_property<const float>();
    EXPECT_FLOAT_EQ(p.get_value(), 0.f);
}

TEST(Property, ConstructReadOnlyWithValue)
{
    auto p = create_property<const int>(42);
    EXPECT_EQ(p.get_value(), 42);
}

TEST(Property, ConstructReadOnlySetFails)
{
    auto p = create_property<const int>(42);
    EXPECT_EQ(p.get_value(), 42);

    // Can still create a "ReadWrite" Property<T> using the interface, but writes should still fail.
    Property<int> pp(p.get_property_interface());
    EXPECT_TRUE(pp);
    EXPECT_EQ(pp.set_value(1), ReturnValue::ReadOnly);
}

// Deferred property updates

TEST(Property, DeferredSetValue)
{
    auto p = create_property<int>(0);
    EXPECT_EQ(p.set_value(42, Deferred), ReturnValue::Success);
    // Value should not be applied yet.
    EXPECT_EQ(p.get_value(), 0);

    instance().update();
    EXPECT_EQ(p.get_value(), 42);
}

TEST(Property, DeferredCoalescing)
{
    auto p = create_property<int>(0);
    int callCount = 0;

    Callback handler([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::Success;
    });

    p.add_on_changed(handler);

    p.set_value(1, Deferred);
    p.set_value(2, Deferred);
    p.set_value(3, Deferred);

    EXPECT_EQ(p.get_value(), 0);

    instance().update();

    // Only last value should be applied, on_changed fires once.
    EXPECT_EQ(p.get_value(), 3);
    EXPECT_EQ(callCount, 1);
}

TEST(Property, DeferredMultipleProperties)
{
    auto p1 = create_property<int>(0);
    auto p2 = create_property<float>(0.f);

    p1.set_value(10, Deferred);
    p2.set_value(3.14f, Deferred);

    EXPECT_EQ(p1.get_value(), 0);
    EXPECT_FLOAT_EQ(p2.get_value(), 0.f);

    instance().update();

    EXPECT_EQ(p1.get_value(), 10);
    EXPECT_FLOAT_EQ(p2.get_value(), 3.14f);
}

TEST(Property, DeferredBatchedNotifications)
{
    auto p1 = create_property<int>(0);
    auto p2 = create_property<int>(0);

    // When p1's on_changed fires, p2 should already have its new value.
    int p2ValueSeenByP1Handler = -1;
    Callback handler([&](FnArgs) -> ReturnValue {
        p2ValueSeenByP1Handler = p2.get_value();
        return ReturnValue::Success;
    });
    p1.add_on_changed(handler);

    p1.set_value(1, Deferred);
    p2.set_value(2, Deferred);

    instance().update();

    EXPECT_EQ(p1.get_value(), 1);
    EXPECT_EQ(p2.get_value(), 2);
    EXPECT_EQ(p2ValueSeenByP1Handler, 2);
}

TEST(Property, DeferredPropertyDestroyedBeforeUpdate)
{
    {
        auto p = create_property<int>(0);
        p.set_value(42, Deferred);
        // p goes out of scope here, destroying the property.
    }
    // update() should not crash when the weak_ptr is expired.
    instance().update();
}
