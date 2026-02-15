#include <gtest/gtest.h>

#include <api/any.h>
#include <api/function.h>
#include <api/property.h>
#include <api/strata.h>
#include <interface/intf_property.h>

using namespace strata;

TEST(Property, DefaultConstructedHasInitialValue)
{
    Property<float> p;
    EXPECT_FLOAT_EQ(p.get_value(), 0.f);
}

TEST(Property, ConstructWithValue)
{
    Property<int> p(42);
    EXPECT_EQ(p.get_value(), 42);
}

TEST(Property, SetGetRoundTrip)
{
    Property<float> p;
    p.set_value(3.14f);
    EXPECT_FLOAT_EQ(p.get_value(), 3.14f);
}

TEST(Property, CopySemanticsShareSameIProperty)
{
    Property<float> p;
    p.set_value(10.f);
    Property<float> copy = p;
    EXPECT_FLOAT_EQ(copy.get_value(), 10.f);

    // Both copies reference the same IProperty
    p.set_value(20.f);
    EXPECT_FLOAT_EQ(copy.get_value(), 20.f);
}

namespace {
    static int s_onChangedCallCount = 0;
    static float s_onChangedReceivedValue = 0.f;
}

TEST(Property, OnChangedEventFires)
{
    s_onChangedCallCount = 0;
    s_onChangedReceivedValue = 0.f;

    Property<float> p;

    Function handler([](FnArgs args) -> ReturnValue {
        s_onChangedCallCount++;
        if (auto v = Any<const float>(args[0])) {
            s_onChangedReceivedValue = v.get_value();
        }
        return ReturnValue::SUCCESS;
    });

    p.add_on_changed(handler);
    p.set_value(42.f);

    EXPECT_EQ(s_onChangedCallCount, 1);
    EXPECT_FLOAT_EQ(s_onChangedReceivedValue, 42.f);
}

TEST(Property, SetSameValueReturnsNothingToDo)
{
    Property<int> p(5);

    auto iprop = p.get_property_interface();
    ASSERT_TRUE(iprop);

    Any<int> val(5);
    auto result = iprop->set_value(val);
    EXPECT_EQ(result, ReturnValue::NOTHING_TO_DO);
}

TEST(Property, SetDifferentValueReturnsSuccess)
{
    Property<int> p(5);
    auto iprop = p.get_property_interface();
    ASSERT_TRUE(iprop);

    Any<int> val(10);
    auto result = iprop->set_value(val);
    EXPECT_EQ(result, ReturnValue::SUCCESS);
    EXPECT_EQ(p.get_value(), 10);
}

namespace {
    static int s_sameValueCallCount = 0;
}

TEST(Property, OnChangedDoesNotFireOnSameValue)
{
    s_sameValueCallCount = 0;

    Property<int> p(5);

    Function handler([](FnArgs) -> ReturnValue {
        s_sameValueCallCount++;
        return ReturnValue::SUCCESS;
    });

    p.add_on_changed(handler);
    p.set_value(5); // same value
    EXPECT_EQ(s_sameValueCallCount, 0);
}

TEST(Property, BoolConversion)
{
    Property<float> p;
    EXPECT_TRUE(p);
}
