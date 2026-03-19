#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_metadata.h>

#include <gtest/gtest.h>

namespace test_detail {

class ITestInt : public ::velk::Interface<ITestInt>
{
public:
    VELK_INTERFACE((PROP, int, value, 0))
};

class TestIntObj : public ::velk::ext::Object<TestIntObj, ITestInt>
{
};

class ITestFloat : public ::velk::Interface<ITestFloat>
{
public:
    VELK_INTERFACE((PROP, float, value, 0.f))
};

class TestFloatObj : public ::velk::ext::Object<TestFloatObj, ITestFloat>
{
};

inline void ensure_registered()
{
    static bool done = false;
    if (done) return;
    done = true;
    ::velk::instance().type_registry().register_type<TestIntObj>();
    ::velk::instance().type_registry().register_type<TestFloatObj>();
}

inline void ensure_unregistered()
{
    static bool done = false;
    if (done) return;
    done = true;
    ::velk::instance().type_registry().unregister_type<TestIntObj>();
    ::velk::instance().type_registry().unregister_type<TestFloatObj>();
}

/// Property wrapper that keeps the owning object alive.
/// Inherits Property<T> so template deduction (e.g. create_tween) works unchanged.
template <class T>
struct PropOwner : public ::velk::Property<T>
{
    ::velk::IObject::Ptr obj_;

    PropOwner(::velk::IObject::Ptr obj, ::velk::IProperty::Ptr prop)
        : ::velk::Property<T>(std::move(prop)), obj_(std::move(obj))
    {}
};

template <class T, class = std::enable_if_t<!std::is_const_v<T>>>
PropOwner<T> create_owned_property(const T& value = {})
{
    ensure_registered();
    ::velk::IObject::Ptr obj;
    if constexpr (std::is_same_v<T, int>) {
        obj = ::velk::instance().create<::velk::IObject>(TestIntObj::static_class_id());
    } else if constexpr (std::is_same_v<T, float>) {
        obj = ::velk::instance().create<::velk::IObject>(TestFloatObj::static_class_id());
    }
    auto iprop = ::velk::interface_cast<::velk::IMetadata>(obj)->get_property("value");
    PropOwner<T> r(std::move(obj), iprop);
    if (value != T{}) r.set_value(value);
    return r;
}

class TestHelpersCleanup : public ::testing::Environment
{
public:
    void TearDown() override { ensure_unregistered(); }
};

inline ::testing::Environment* const test_helpers_env_ =
    ::testing::AddGlobalTestEnvironment(new TestHelpersCleanup());

} // namespace test_detail

// Redirect create_property to the ObjectStorage-backed version for test files.
#define create_property test_detail::create_owned_property

#endif // TEST_HELPERS_H
