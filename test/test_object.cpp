#include <gtest/gtest.h>

#include <api/any.h>
#include <api/function.h>
#include <api/function_context.h>
#include <api/property.h>
#include <api/strata.h>
#include <ext/object.h>
#include <interface/intf_event.h>
#include <interface/intf_metadata.h>
#include <interface/intf_property.h>
#include <interface/types.h>

using namespace strata;

// --- Test interfaces and implementation ---

class ITestWidget : public Interface<ITestWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width, 100.f),
        (PROP, float, height, 50.f),
        (EVT, on_clicked),
        (FN, reset)
    )
};

class ITestSerializable : public Interface<ITestSerializable>
{
public:
    STRATA_INTERFACE(
        (PROP, int, version, 1),
        (FN, serialize)
    )
};

class TestWidget : public ext::Object<TestWidget, ITestWidget, ITestSerializable>
{
public:
    int resetCallCount = 0;
    int serializeCallCount = 0;

    ReturnValue fn_reset(FnArgs) override
    {
        resetCallCount++;
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_serialize(FnArgs) override
    {
        serializeCallCount++;
        return ReturnValue::SUCCESS;
    }
};

// --- Fixture to register once ---

class ObjectTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().register_type<TestWidget>();
    }
};

// --- Tests ---

TEST_F(ObjectTest, RegisterAndCreate)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    ASSERT_TRUE(obj);
}

TEST_F(ObjectTest, InterfaceCastSucceeds)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    ASSERT_TRUE(obj);

    auto* iw = interface_cast<ITestWidget>(obj);
    EXPECT_NE(iw, nullptr);

    auto* is = interface_cast<ITestSerializable>(obj);
    EXPECT_NE(is, nullptr);

    auto* meta = interface_cast<IMetadata>(obj);
    EXPECT_NE(meta, nullptr);
}

TEST_F(ObjectTest, InterfaceCastFailsForNonImplemented)
{
    // IStrata is not implemented by TestWidget
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    ASSERT_TRUE(obj);

    auto* bad = interface_cast<IStrata>(obj);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(ObjectTest, MetadataGetPropertyByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto widthProp = meta->get_property("width");
    EXPECT_TRUE(widthProp);

    auto heightProp = meta->get_property("height");
    EXPECT_TRUE(heightProp);

    auto versionProp = meta->get_property("version");
    EXPECT_TRUE(versionProp);
}

TEST_F(ObjectTest, MetadataGetPropertyReturnsNullForUnknown)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto bogus = meta->get_property("nonexistent");
    EXPECT_FALSE(bogus);
}

TEST_F(ObjectTest, MetadataGetEventByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto event = meta->get_event("on_clicked");
    EXPECT_TRUE(event);
}

TEST_F(ObjectTest, MetadataGetFunctionByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto fn = meta->get_function("reset");
    EXPECT_TRUE(fn);

    auto fn2 = meta->get_function("serialize");
    EXPECT_TRUE(fn2);
}

TEST_F(ObjectTest, PropertyDefaultsFromInterface)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    EXPECT_FLOAT_EQ(iw->width().get_value(), 100.f);
    EXPECT_FLOAT_EQ(iw->height().get_value(), 50.f);

    auto* is = interface_cast<ITestSerializable>(obj);
    ASSERT_NE(is, nullptr);
    EXPECT_EQ(is->version().get_value(), 1);
}

TEST_F(ObjectTest, PropertySetAndGet)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    iw->width().set_value(42.f);
    EXPECT_FLOAT_EQ(iw->width().get_value(), 42.f);
}

TEST_F(ObjectTest, FunctionInvokeViaInterface)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    invoke_function(iw->reset());
    // Verify the virtual fn_reset was called (via the raw pointer)
    auto* raw = dynamic_cast<TestWidget*>(obj.get());
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->resetCallCount, 1);
}

TEST_F(ObjectTest, FunctionInvokeByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    invoke_function(obj.get(), "reset");

    auto* raw = dynamic_cast<TestWidget*>(obj.get());
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->resetCallCount, 1);
}

TEST_F(ObjectTest, StaticMetadataViaGetClassInfo)
{
    auto* info = instance().get_class_info(TestWidget::get_class_uid());
    ASSERT_NE(info, nullptr);

    // ITestWidget: width, height, on_clicked, reset
    // ITestSerializable: version, serialize
    EXPECT_EQ(info->members.size(), 6u);

    // Check first member
    EXPECT_EQ(info->members[0].name, "width");
    EXPECT_EQ(info->members[0].kind, MemberKind::Property);

    EXPECT_EQ(info->members[2].name, "on_clicked");
    EXPECT_EQ(info->members[2].kind, MemberKind::Event);

    EXPECT_EQ(info->members[3].name, "reset");
    EXPECT_EQ(info->members[3].kind, MemberKind::Function);
}

TEST_F(ObjectTest, StaticDefaultValues)
{
    auto* info = instance().get_class_info(TestWidget::get_class_uid());
    ASSERT_NE(info, nullptr);

    EXPECT_FLOAT_EQ(get_default_value<float>(info->members[0]), 100.f);  // width
    EXPECT_FLOAT_EQ(get_default_value<float>(info->members[1]), 50.f);   // height
    EXPECT_EQ(get_default_value<int>(info->members[4]), 1);              // version
}

TEST_F(ObjectTest, PropertyStateReadWrite)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* iw = interface_cast<ITestWidget>(obj);
    auto* ps = interface_cast<IPropertyState>(obj);
    ASSERT_NE(iw, nullptr);
    ASSERT_NE(ps, nullptr);

    auto* state = ps->get_property_state<ITestWidget>();
    ASSERT_NE(state, nullptr);

    // Defaults in state struct
    EXPECT_FLOAT_EQ(state->width, 100.f);
    EXPECT_FLOAT_EQ(state->height, 50.f);

    // Write through property, read from state
    iw->width().set_value(200.f);
    EXPECT_FLOAT_EQ(state->width, 200.f);

    // Write to state directly, read through property
    state->height = 75.f;
    EXPECT_FLOAT_EQ(iw->height().get_value(), 75.f);
}
