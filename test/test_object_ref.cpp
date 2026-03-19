#include <velk/api/any.h>
#include <velk/api/object_ref.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object_ref.h>

#include <gtest/gtest.h>

// Standalone ObjectRef tests

TEST(ObjectRef, CreateAndSetObject)
{
    auto ref = ::velk::instance().create_object_ref();
    ASSERT_TRUE(ref);
    EXPECT_EQ(ref->get_object(), nullptr);

    // Create a dummy object to reference
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    ASSERT_TRUE(target);

    EXPECT_EQ(ref->set_object(target), velk::ReturnValue::Success);
    EXPECT_EQ(ref->get_object(), target);
}

TEST(ObjectRef, OwningMode)
{
    auto ref = ::velk::instance().create_object_ref();
    ASSERT_TRUE(ref);
    EXPECT_TRUE(ref->is_owning());

    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    ref->set_object(target);

    // Switch to non-owning
    EXPECT_GE(ref->set_owning(false), 0);
    EXPECT_FALSE(ref->is_owning());

    // Target should still be accessible (we hold target locally)
    EXPECT_EQ(ref->get_object(), target);

    // Switch back to owning
    EXPECT_GE(ref->set_owning(true), 0);
    EXPECT_TRUE(ref->is_owning());
    EXPECT_EQ(ref->get_object(), target);
}

TEST(ObjectRef, NonOwningExpires)
{
    auto ref = ::velk::instance().create_object_ref();
    ASSERT_TRUE(ref);

    {
        auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
        ref->set_object(target);
        ref->set_owning(false);
        // Target is still alive through our local variable
        EXPECT_NE(ref->get_object(), nullptr);
    }
    // target is now destroyed, weak ref should be expired
    EXPECT_EQ(ref->get_object(), nullptr);
}

TEST(ObjectRef, SetObjectSameReturnsNothingToDo)
{
    auto ref = ::velk::instance().create_object_ref();
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    ref->set_object(target);

    EXPECT_EQ(ref->set_object(target), velk::ReturnValue::NothingToDo);
}

TEST(ObjectRef, SetNullClearsRef)
{
    auto ref = ::velk::instance().create_object_ref();
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    ref->set_object(target);

    EXPECT_EQ(ref->set_object(nullptr), velk::ReturnValue::Success);
    EXPECT_EQ(ref->get_object(), nullptr);
}

TEST(ObjectRef, GetSetDataRoundtrip)
{
    auto ref = ::velk::instance().create_object_ref();
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);

    // Set via set_data
    auto type = velk::type_uid<velk::IObject::Ptr>();
    EXPECT_EQ(ref->set_data(&target, sizeof(velk::IObject::Ptr), type), velk::ReturnValue::Success);

    // Get via get_data
    velk::IObject::Ptr result;
    EXPECT_EQ(ref->get_data(&result, sizeof(velk::IObject::Ptr), type), velk::ReturnValue::Success);
    EXPECT_EQ(result.get(), target.get());
}

TEST(ObjectRef, Clone)
{
    auto ref = ::velk::instance().create_object_ref();
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    ref->set_object(target);

    auto cloned = ref->clone();
    ASSERT_TRUE(cloned);

    velk::IObject::Ptr result;
    auto type = velk::type_uid<velk::IObject::Ptr>();
    EXPECT_EQ(cloned->get_data(&result, sizeof(velk::IObject::Ptr), type), velk::ReturnValue::Success);
    EXPECT_EQ(result.get(), target.get());
}

TEST(ObjectRef, CopyFrom)
{
    auto src = ::velk::instance().create_object_ref();
    auto dst = ::velk::instance().create_object_ref();
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    src->set_object(target);

    EXPECT_EQ(dst->copy_from(*src), velk::ReturnValue::Success);

    auto* dst_ref = velk::interface_cast<velk::IObjectRef>(dst);
    ASSERT_TRUE(dst_ref);
    EXPECT_EQ(dst_ref->get_object(), target);
}

// Factory helper tests

TEST(ObjectRef, CreateWithObject)
{
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    auto ref = ::velk::create_object_ref(target);
    EXPECT_TRUE(ref);
    EXPECT_TRUE(ref.is_owning());
    EXPECT_EQ(ref.get(), target);
}

TEST(ObjectRef, CreateWeakEmpty)
{
    auto ref = ::velk::create_weak_object_ref();
    EXPECT_TRUE(ref);
    EXPECT_FALSE(ref.is_owning());
    EXPECT_EQ(ref.get(), nullptr);
}

TEST(ObjectRef, CreateWeakWithObject)
{
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    auto ref = ::velk::create_weak_object_ref(target);
    EXPECT_TRUE(ref);
    EXPECT_FALSE(ref.is_owning());
    EXPECT_EQ(ref.get(), target);
}

TEST(ObjectRef, WeakRefExpires)
{
    velk::ObjectRef ref;
    {
        auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
        ref = ::velk::create_weak_object_ref(target);
        EXPECT_EQ(ref.get(), target);
    }
    EXPECT_EQ(ref.get(), nullptr);
}

// Constraint tests

class IConstraintTest : public ::velk::Interface<IConstraintTest>
{
public:
    VELK_INTERFACE(
        (PROP, float, value, 0.f)
    )
};

class ConstraintTestImpl : public ::velk::ext::Object<ConstraintTestImpl, IConstraintTest>
{
public:
    VELK_CLASS_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890", "TestObjRef1");
};

TEST(ObjectRef, ConstraintAcceptsMatchingInterface)
{
    ::velk::register_type<ConstraintTestImpl>(::velk::instance());

    auto ref = ::velk::instance().create_object_ref();
    ref->set_constraint(IConstraintTest::UID);
    EXPECT_EQ(ref->constraint_uid(), IConstraintTest::UID);

    auto target = ::velk::instance().create<velk::IObject>(ConstraintTestImpl::static_class_id());
    ASSERT_TRUE(target);

    EXPECT_EQ(ref->set_object(target), velk::ReturnValue::Success);
    EXPECT_EQ(ref->get_object(), target);
}

TEST(ObjectRef, ConstraintRejectsMismatch)
{
    auto ref = ::velk::instance().create_object_ref();
    ref->set_constraint(IConstraintTest::UID);

    // A plain property object doesn't implement IConstraintTest
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    ASSERT_TRUE(target);

    EXPECT_EQ(ref->set_object(target), velk::ReturnValue::InvalidArgument);
    EXPECT_EQ(ref->get_object(), nullptr);
}

// Property integration tests

class IObjectRefPropTest : public ::velk::Interface<IObjectRefPropTest>
{
public:
    VELK_INTERFACE(
        (PROP, velk::ObjectRef, child, {})
    )
};

class ObjectRefPropTestImpl : public ::velk::ext::Object<ObjectRefPropTestImpl, IObjectRefPropTest>
{
public:
    VELK_CLASS_UID("d1e2f3a4-b5c6-7890-abcd-ef1234560002", "TestObjRef2");
};

TEST(ObjectRefProperty, SetAndGetViaProperty)
{
    ::velk::register_type<ObjectRefPropTestImpl>(::velk::instance());

    auto obj = ::velk::instance().create<velk::IObject>(ObjectRefPropTestImpl::static_class_id());
    ASSERT_TRUE(obj);

    auto* intf = velk::interface_cast<IObjectRefPropTest>(obj);
    ASSERT_TRUE(intf);

    auto prop = intf->child();
    ASSERT_TRUE(prop);

    // Create a target object
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    ASSERT_TRUE(target);

    // Create an ObjectRef wrapper via helper, set target, pass to property
    auto ref = ::velk::create_object_ref();
    ref.set(target);
    EXPECT_GE(prop.set_value(ref), 0);

    // Get via property: the backing IAny is the ObjectRefImpl itself
    auto val = prop.get_value();
    ASSERT_TRUE(val);
    auto* obj_ref = velk::interface_cast<velk::IObjectRef>(val);
    ASSERT_TRUE(obj_ref);
    EXPECT_EQ(obj_ref->get_object(), target);
}

TEST(ObjectRefProperty, StateStructAccess)
{
    auto obj = ::velk::instance().create<velk::IObject>(ObjectRefPropTestImpl::static_class_id());
    auto* meta = velk::interface_cast<velk::IMetadata>(obj);
    ASSERT_TRUE(meta);

    auto* intf = velk::interface_cast<IObjectRefPropTest>(obj);
    ASSERT_TRUE(intf);

    // Access the property once to trigger createRef (initializes the ObjectRef in state)
    auto prop = intf->child();
    ASSERT_TRUE(prop);

    // Create and set a target via state struct
    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    {
        auto writer = meta->write<IObjectRefPropTest>();
        ASSERT_TRUE(writer);
        writer->child.set(target);
    }

    // Read via state struct
    auto reader = meta->read<IObjectRefPropTest>();
    ASSERT_TRUE(reader);
    EXPECT_EQ(reader->child.get(), target);
}

TEST(ObjectRefProperty, OnChangedFires)
{
    auto obj = ::velk::instance().create<velk::IObject>(ObjectRefPropTestImpl::static_class_id());
    auto* intf = velk::interface_cast<IObjectRefPropTest>(obj);
    ASSERT_TRUE(intf);

    auto prop = intf->child();
    int count = 0;
    auto cb = ::velk::instance().create_owned_callback(
        &count,
        [](void* ctx, velk::FnArgs) -> velk::IAny::Ptr {
            ++(*static_cast<int*>(ctx));
            return nullptr;
        },
        [](void*) {});
    prop.add_on_changed(cb);

    auto target1 = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    auto ref1 = ::velk::create_object_ref();
    ref1.set(target1);
    prop.set_value(ref1);
    EXPECT_EQ(count, 1);

    // Same value should not fire
    auto ref2 = ::velk::create_object_ref();
    ref2.set(target1);
    prop.set_value(ref2);
    EXPECT_EQ(count, 1);

    // Different value should fire
    auto target2 = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    auto ref3 = ::velk::create_object_ref();
    ref3.set(target2);
    prop.set_value(ref3);
    EXPECT_EQ(count, 2);
}

TEST(ObjectRefProperty, OwningModeViaWrapper)
{
    auto obj = ::velk::instance().create<velk::IObject>(ObjectRefPropTestImpl::static_class_id());
    auto* meta = velk::interface_cast<velk::IMetadata>(obj);
    ASSERT_TRUE(meta);

    auto* intf = velk::interface_cast<IObjectRefPropTest>(obj);
    auto prop = intf->child();

    auto target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
    {
        auto writer = meta->write<IObjectRefPropTest>();
        writer->child.set(target);
        writer->child.set_owning(false);
    }

    auto reader = meta->read<IObjectRefPropTest>();
    EXPECT_FALSE(reader->child.is_owning());

    // Target still alive through local var
    EXPECT_EQ(reader->child.get(), target);
}

TEST(ObjectRefProperty, ConstraintViaWrapper)
{
    {
        auto obj = ::velk::instance().create<velk::IObject>(ObjectRefPropTestImpl::static_class_id());
        auto* meta = velk::interface_cast<velk::IMetadata>(obj);
        ASSERT_TRUE(meta);

        auto* intf = velk::interface_cast<IObjectRefPropTest>(obj);
        auto prop = intf->child();

        {
            auto writer = meta->write<IObjectRefPropTest>();
            writer->child.set_constraint<IConstraintTest>();
        }

        auto reader = meta->read<IObjectRefPropTest>();
        EXPECT_EQ(reader->child.constraint_uid(), IConstraintTest::UID);

        // Set an object that implements IConstraintTest
        auto good_target = ::velk::instance().create<velk::IObject>(ConstraintTestImpl::static_class_id());
        {
            auto writer = meta->write<IObjectRefPropTest>();
            EXPECT_EQ(writer->child.set(good_target), velk::ReturnValue::Success);
        }

        // Set an object that doesn't implement IConstraintTest
        auto bad_target = ::velk::instance().create<velk::IObject>(velk::ClassId::Property);
        {
            auto writer = meta->write<IObjectRefPropTest>();
            EXPECT_EQ(writer->child.set(bad_target), velk::ReturnValue::InvalidArgument);
        }
    }

    ::velk::instance().type_registry().unregister_type<ConstraintTestImpl>();
    ::velk::instance().type_registry().unregister_type<ObjectRefPropTestImpl>();
}
