#include <velk/api/any.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_variant.h>
#include <velk/string.h>

#include <gtest/gtest.h>

// Variant standalone tests

TEST(Variant, StoreAndReadFloat)
{
    auto any = ::velk::instance().create_variant();
    ASSERT_TRUE(any);

    float value = 42.f;
    EXPECT_EQ(any->set_data(&value, sizeof(float), velk::type_uid<float>()), velk::ReturnValue::Success);

    float result = 0.f;
    EXPECT_EQ(any->get_data(&result, sizeof(float), velk::type_uid<float>()), velk::ReturnValue::Success);
    EXPECT_FLOAT_EQ(result, 42.f);
}

TEST(Variant, StoredType)
{
    auto any = ::velk::instance().create_variant();
    ASSERT_TRUE(any);
    auto* va = velk::interface_cast<velk::IVariant>(any);
    ASSERT_TRUE(va);

    EXPECT_EQ(va->stored_type(), velk::Uid{});

    float value = 1.f;
    any->set_data(&value, sizeof(float), velk::type_uid<float>());
    EXPECT_EQ(va->stored_type(), velk::type_uid<float>());
}

TEST(Variant, ConvertFloatToDouble)
{
    auto any = ::velk::instance().create_variant();
    ASSERT_TRUE(any);

    float value = 3.14f;
    any->set_data(&value, sizeof(float), velk::type_uid<float>());

    double result = 0.0;
    EXPECT_EQ(any->get_data(&result, sizeof(double), velk::type_uid<double>()), velk::ReturnValue::Success);
    EXPECT_NEAR(result, 3.14, 0.001);
}

TEST(Variant, ConvertFloatToInt32)
{
    auto any = ::velk::instance().create_variant();
    ASSERT_TRUE(any);

    float value = 7.9f;
    any->set_data(&value, sizeof(float), velk::type_uid<float>());

    int32_t result = 0;
    EXPECT_EQ(any->get_data(&result, sizeof(int32_t), velk::type_uid<int32_t>()), velk::ReturnValue::Success);
    EXPECT_EQ(result, 7);
}

TEST(Variant, StoreString)
{
    auto any = ::velk::instance().create_variant();
    ASSERT_TRUE(any);

    velk::string value = "hello";
    any->set_data(&value, sizeof(velk::string), velk::type_uid<velk::string>());

    velk::string result;
    EXPECT_EQ(any->get_data(&result, sizeof(velk::string), velk::type_uid<velk::string>()),
              velk::ReturnValue::Success);
    EXPECT_EQ(result, "hello");
}

TEST(Variant, StringReadAsFloatFails)
{
    auto any = ::velk::instance().create_variant();
    ASSERT_TRUE(any);

    velk::string value = "hello";
    any->set_data(&value, sizeof(velk::string), velk::type_uid<velk::string>());

    float result = 0.f;
    EXPECT_EQ(any->get_data(&result, sizeof(float), velk::type_uid<float>()), velk::ReturnValue::Fail);
}

TEST(Variant, CanConvertTo)
{
    auto any = ::velk::instance().create_variant();
    ASSERT_TRUE(any);
    auto* va = velk::interface_cast<velk::IVariant>(any);
    ASSERT_TRUE(va);

    float value = 1.f;
    any->set_data(&value, sizeof(float), velk::type_uid<float>());

    EXPECT_TRUE(va->can_convert_to(velk::type_uid<double>()));
    EXPECT_TRUE(va->can_convert_to(velk::type_uid<int32_t>()));
    EXPECT_FALSE(va->can_convert_to(velk::type_uid<velk::string>()));
}

TEST(Variant, Clone)
{
    auto any = ::velk::instance().create_variant();
    float value = 99.f;
    any->set_data(&value, sizeof(float), velk::type_uid<float>());

    auto cloned = any->clone();
    ASSERT_TRUE(cloned);

    float result = 0.f;
    EXPECT_EQ(cloned->get_data(&result, sizeof(float), velk::type_uid<float>()), velk::ReturnValue::Success);
    EXPECT_FLOAT_EQ(result, 99.f);
}

TEST(Variant, CopyFromRegularAny)
{
    auto any = ::velk::instance().create_variant();
    velk::Any<float> typed(42.f);

    EXPECT_EQ(any->copy_from(*typed.get_any_interface()), velk::ReturnValue::Success);

    float result = 0.f;
    EXPECT_EQ(any->get_data(&result, sizeof(float), velk::type_uid<float>()), velk::ReturnValue::Success);
    EXPECT_FLOAT_EQ(result, 42.f);
}

TEST(Variant, CopyFromVariant)
{
    auto src = ::velk::instance().create_variant();
    float value = 123.f;
    src->set_data(&value, sizeof(float), velk::type_uid<float>());

    auto dst = ::velk::instance().create_variant();
    EXPECT_EQ(dst->copy_from(*src), velk::ReturnValue::Success);

    float result = 0.f;
    EXPECT_EQ(dst->get_data(&result, sizeof(float), velk::type_uid<float>()), velk::ReturnValue::Success);
    EXPECT_FLOAT_EQ(result, 123.f);
}

TEST(Variant, SetDataChangesType)
{
    auto any = ::velk::instance().create_variant();
    auto* va = velk::interface_cast<velk::IVariant>(any);

    float fval = 1.f;
    any->set_data(&fval, sizeof(float), velk::type_uid<float>());
    EXPECT_EQ(va->stored_type(), velk::type_uid<float>());

    int32_t ival = 42;
    any->set_data(&ival, sizeof(int32_t), velk::type_uid<int32_t>());
    EXPECT_EQ(va->stored_type(), velk::type_uid<int32_t>());

    int32_t result = 0;
    any->get_data(&result, sizeof(int32_t), velk::type_uid<int32_t>());
    EXPECT_EQ(result, 42);
}

TEST(Variant, CompatibleTypesIncludeConversions)
{
    auto any = ::velk::instance().create_variant();
    float value = 1.f;
    any->set_data(&value, sizeof(float), velk::type_uid<float>());

    auto types = any->get_compatible_types();
    EXPECT_GE(types.size(), 2u);

    bool has_float = false;
    bool has_double = false;
    for (auto uid : types) {
        if (uid == velk::type_uid<float>()) has_float = true;
        if (uid == velk::type_uid<double>()) has_double = true;
    }
    EXPECT_TRUE(has_float);
    EXPECT_TRUE(has_double);
}

// Variant property integration tests

class IVariantPropTest : public ::velk::Interface<IVariantPropTest>
{
public:
    VELK_INTERFACE(
        (PROP, velk::Variant, value, {})
    )
};

class VariantPropTestImpl : public ::velk::ext::Object<VariantPropTestImpl, IVariantPropTest>
{
public:
    VELK_CLASS_UID("d1e2f3a4-b5c6-7890-abcd-ef1234560001", "VariantTestImpl");
};

TEST(VariantProperty, SetAndGetFloat)
{
    ::velk::register_type<VariantPropTestImpl>(::velk::instance());

    auto obj = ::velk::instance().create<velk::IObject>(VariantPropTestImpl::static_class_id());
    ASSERT_TRUE(obj);

    auto* intf = velk::interface_cast<IVariantPropTest>(obj);
    ASSERT_TRUE(intf);

    auto prop = intf->value();
    ASSERT_TRUE(prop);

    velk::Any<float> av(42.f);
    EXPECT_GE(prop.set_value(av), 0);

    auto val = prop.get_value();
    ASSERT_TRUE(val);
    velk::Any<const float> typed(val);
    ASSERT_TRUE(typed);
    EXPECT_FLOAT_EQ(typed.get_value(), 42.f);
}

TEST(VariantProperty, SetFloatThenString)
{
    auto obj = ::velk::instance().create<velk::IObject>(VariantPropTestImpl::static_class_id());
    auto* intf = velk::interface_cast<IVariantPropTest>(obj);
    ASSERT_TRUE(intf);

    auto prop = intf->value();

    velk::Any<float> av(1.f);
    prop.set_value(av);

    velk::Any<velk::string> sv(velk::string("test"));
    prop.set_value(sv);

    auto val = prop.get_value();
    ASSERT_TRUE(val);
    velk::Any<const velk::string> typed(val);
    ASSERT_TRUE(typed);
    EXPECT_EQ(typed.get_value(), "test");
}

TEST(VariantProperty, OnChangedFires)
{
    auto obj = ::velk::instance().create<velk::IObject>(VariantPropTestImpl::static_class_id());
    auto* intf = velk::interface_cast<IVariantPropTest>(obj);
    ASSERT_TRUE(intf);

    auto prop = intf->value();
    int count = 0;
    auto cb = ::velk::instance().create_owned_callback(
        &count,
        [](void* ctx, velk::FnArgs) -> velk::IAny::Ptr {
            ++(*static_cast<int*>(ctx));
            return nullptr;
        },
        [](void*) {});
    prop.add_on_changed(cb);

    velk::Any<float> av(42.f);
    prop.set_value(av);
    EXPECT_EQ(count, 1);

    velk::Any<float> av2(42.f);
    prop.set_value(av2);
    // Same value, should not fire
    EXPECT_EQ(count, 1);

    velk::Any<float> av3(99.f);
    prop.set_value(av3);
    EXPECT_EQ(count, 2);
}

TEST(VariantProperty, StateStructAccess)
{
    auto obj = ::velk::instance().create<velk::IObject>(VariantPropTestImpl::static_class_id());
    auto* meta = velk::interface_cast<velk::IMetadata>(obj);
    ASSERT_TRUE(meta);

    // Access the property once to trigger createRef (initializes the Variant in state)
    auto* intf = velk::interface_cast<IVariantPropTest>(obj);
    ASSERT_TRUE(intf);
    auto prop = intf->value();
    ASSERT_TRUE(prop);

    // Write via property accessor
    velk::Any<float> av(42.f);
    prop.set_value(av);

    // Read via state struct
    auto reader = meta->read<IVariantPropTest>();
    ASSERT_TRUE(reader);
    EXPECT_FLOAT_EQ(reader->value.get<float>(), 42.f);
    EXPECT_EQ(reader->value.stored_type(), velk::type_uid<float>());
    EXPECT_TRUE(reader->value.can_convert_to(velk::type_uid<double>()));

    // Write via state struct, read back via property
    {
        auto writer = meta->write<IVariantPropTest>();
        ASSERT_TRUE(writer);
        writer->value.set<int32_t>(99);
    }

    auto val = prop.get_value();
    ASSERT_TRUE(val);
    int32_t result = 0;
    EXPECT_EQ(val->get_data(&result, sizeof(int32_t), velk::type_uid<int32_t>()),
              velk::ReturnValue::Success);
    EXPECT_EQ(result, 99);

    obj.reset();
    ::velk::instance().type_registry().unregister_type<VariantPropTestImpl>();
}
