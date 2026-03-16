# Velk Usage Patterns

Detailed code examples for consuming the Velk API. All examples assume:

```cpp
#include <velk/api/velk.h>
#include <velk/ext/object.h>
using namespace velk;
```

## 1. Interface Definition

```cpp
#include <velk/interface/intf_metadata.h>

class IMyWidget : public Interface<IMyWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 100.f),       // read-write, default 100
        (PROP, float, height, 50.f),       // read-write, default 50
        (RPROP, int, id, 0),               // read-only via accessor, writable via state
        (EVT, on_clicked),                 // event
        (FN, void, reset),                 // typed function, zero args
        (FN, void, add, (int, x), (float, y)),  // typed function with args
        (FN_RAW, serialize)                // raw function receiving FnArgs
    )
};
```

Interface inheritance:

```cpp
class IShape : public Interface<IShape>
{
public:
    VELK_INTERFACE(
        (PROP, float, x, 0.f),
        (PROP, float, y, 0.f)
    )
};

// IRect inherits IShape's members via the interface chain
class IRect : public Interface<IRect, IShape>
{
public:
    VELK_INTERFACE(
        (PROP, float, w, 0.f),
        (PROP, float, h, 0.f)
    )
};
```

Fixed UID (stable across builds):

```cpp
class IStable : public Interface<IStable, IInterface, VELK_UID("a0b1c2d3-e4f5-6789-abcd-ef0123456789")>
{
public:
    VELK_INTERFACE(
        (PROP, int, version, 1)
    )
};
```

Array properties:

```cpp
class IContainer : public Interface<IContainer>
{
public:
    VELK_INTERFACE(
        (ARR, int, items),          // read-write array
        (RARR, float, weights)     // read-only array
    )
};
```

## 2. Object Implementation

```cpp
class MyWidget : public ext::Object<MyWidget, IMyWidget>
{
    // FN members: override fn_Name with matching signature
    void fn_reset() override
    {
        // implementation
    }

    void fn_add(int x, float y) override
    {
        // implementation
    }

    // FN_RAW members: override fn_Name(FnArgs) -> IAny::Ptr
    IAny::Ptr fn_serialize(FnArgs args) override
    {
        // implementation
        return nullptr;
    }
};
```

Multiple interfaces:

```cpp
class MyObject : public ext::Object<MyObject, IMyWidget, ISerializable, IContainer>
{
    // Override fn_ methods from ALL interfaces
    void fn_reset() override { /* ... */ }
    IAny::Ptr fn_serialize(FnArgs) override { return nullptr; }
};
```

User-specified class UID (stable across builds):

```cpp
class StableWidget : public ext::Object<StableWidget, IMyWidget>
{
public:
    VELK_CLASS_UID("a0b1c2d3-e4f5-6789-abcd-ef0123456789");

    void fn_reset() override {}
};
```

## 3. Type Registration and Creation

```cpp
#include <velk/api/velk.h>

// Register types (once, before any create calls)
auto& r = instance();
r.type_registry().register_type<MyWidget>();

// Create an object
auto obj = r.create<IObject>(MyWidget::static_class_id());

// Cast to interface
auto* iw = interface_cast<IMyWidget>(obj);

// Shared pointer cast
auto ptr = interface_pointer_cast<IMyWidget>(obj);
```

## 4. Property Access

### Via typed interface accessors (preferred)

```cpp
auto* iw = interface_cast<IMyWidget>(obj);

// Read
float w = iw->width().get_value();

// Write
iw->width().set_value(42.f);

// Read-only property
int myId = iw->id().get_value(); // returns ConstProperty<int>
```

### Via IMetadata (name-based lookup)

```cpp
auto* meta = interface_cast<IMetadata>(obj);
auto prop = meta->get_property("width");   // IProperty::Ptr
auto evt  = meta->get_event("on_clicked"); // IEvent::Ptr
auto fn   = meta->get_function("reset");   // IFunction::ConstPtr
```

### Via velk::Object wrapper

```cpp
::velk::Object obj(instance().create<IObject>(MyWidget::static_class_id()));

auto prop = obj.get_property("width");
auto evt  = obj.get_event("on_clicked");
auto fn   = obj.get_function("reset");

// Interface cast
auto* iw = obj.as<IMyWidget>();
auto ptr = obj.as_ptr<IMyWidget>();
```

### Standalone properties (no object)

```cpp
#include <velk/api/property.h>

auto p = create_property<float>(3.14f);
p.set_value(42.f);
float v = p.get_value();

// Read-only standalone
auto rp = create_property<const int>(42);
// rp.set_value(1) would return ReturnValue::ReadOnly
```

### Deferred updates

```cpp
auto p = create_property<int>(0);
p.set_value(42, Deferred);       // not applied yet
// p.get_value() still returns 0

instance().update();              // flushes all deferred updates
// p.get_value() now returns 42

// Multiple deferred sets coalesce: only last value applied, on_changed fires once
p.set_value(1, Deferred);
p.set_value(2, Deferred);
p.set_value(3, Deferred);
instance().update(); // value is 3, on_changed fires once
```

### Change notifications

```cpp
#include <velk/api/callback.h>

auto p = create_property<float>();

Callback handler([](FnArgs args) -> ReturnValue {
    if (auto v = Any<const float>(args[0])) {
        // v.get_value() is the new value
    }
    return ReturnValue::Success;
});

p.add_on_changed(handler);
p.set_value(42.f); // handler fires

p.remove_on_changed(handler);
```

### StateReader and StateWriter (direct state access)

Prefer the free functions `read_state<T>()` / `write_state<T>()` over manually fetching `IMetadata` and calling `meta->read/write`. Only fall back to `meta->read<T>()` / `meta->write<T>()` when you already have an `IMetadata*` for other reasons. These accept both raw pointers and `shared_ptr`.

```cpp
// Read state (zero-copy, no on_changed) accepts raw pointer or shared_ptr
auto reader = read_state<IMyWidget>(obj);   // obj is IObject::Ptr or raw pointer
float w = reader->width;
float h = reader->height;

// Write state (fires on_changed for ALL properties of that interface on destruction)
{
    auto writer = write_state<IMyWidget>(obj);
    writer->width = 300.f;
    writer->height = 150.f;
} // ~StateWriter fires on_changed here

// Callback-based write (immediate)
write_state<IMyWidget>(obj, [](IMyWidget::State& s) {
    s.width = 500.f;
});

// Callback-based write (deferred)
write_state<IMyWidget>(obj, [](IMyWidget::State& s) {
    s.width = 700.f;
}, Deferred);
instance().update(); // apply

// Via Object wrapper
::velk::Object obj(/* ... */);
auto reader = obj.read_state<IMyWidget>();
auto writer = obj.write_state<IMyWidget>();
obj.write_state<IMyWidget>([](IMyWidget::State& s) { s.width = 42.f; });
obj.write_state<IMyWidget>([](IMyWidget::State& s) { s.width = 42.f; }, Deferred);
```

## 5. Functions

### Invocation

```cpp
// By interface accessor
auto* iw = interface_cast<IMyWidget>(obj);
invoke_function(iw->reset());                    // zero-arg
invoke_function(iw, "reset");                    // by name
invoke_function(iw, "add", Any<int>(10), Any<float>(3.14f)); // typed args

// By IObject pointer + name
invoke_function(obj.get(), "reset");
invoke_function(obj.get(), "add", Any<int>(5), Any<float>(2.5f));

// By IFunction::Ptr with raw values (auto-wrapped in Any)
invoke_function(IFunction::Ptr(callback), 10.f, 20);

// Via Object wrapper
::velk::Object obj(/* ... */);
obj.invoke_function("reset");
obj.invoke_function("add", 3, 7); // auto-wraps in Any
```

### Callbacks (standalone functions)

```cpp
#include <velk/api/callback.h>

// Simple callback
Callback cb([](FnArgs args) -> ReturnValue {
    return ReturnValue::Success;
});

// Zero-arg callback
Callback cb([]() { /* ... */ });

// Callback returning IAny::Ptr
Callback cb([](FnArgs args) -> IAny::Ptr {
    return Any<int>(42);
});
```

### FunctionContext (multi-arg)

```cpp
#include <velk/api/function_context.h>

Callback add([](FnArgs args) -> ReturnValue {
    auto ctx = FunctionContext(args, 2); // expect exactly 2 args
    if (!ctx) {
        return ReturnValue::InvalidArgument;
    }
    auto a = ctx.arg<float>(0); // Any<const float>
    auto b = ctx.arg<int>(1);   // Any<const int>
    if (a && b) {
        float result = a.get_value() + b.get_value();
    }
    return ReturnValue::Success;
});

invoke_function(IFunction::Ptr(add), 10.f, 20);
```

## 6. Events

```cpp
auto* iw = interface_cast<IMyWidget>(obj);
auto evt = iw->on_clicked();

// Add handler
Callback handler([]() { /* clicked! */ });
evt.add_handler(handler);

// Add handler with deferred dispatch
evt.add_handler(handler, Deferred);

// Remove handler
evt.remove_handler(handler);

// Check and invoke
if (evt.has_handlers()) {
    evt.invoke();
}

// Invoke with arguments
evt.invoke(someArg);

// Via Object wrapper
::velk::Object obj(/* ... */);
obj.invoke_event("on_clicked");
```

## 7. Bindings

### Property-to-property

```cpp
#include <velk/api/binding.h>

auto source = create_property<int>(42);
auto target = create_property<int>(0);

// Create and install in one call
auto binding = ::velk::create_binding(target, source);
// target.get_value() == 42 (reads from source)
// target.set_value(99) fails (OneWay)

// Source changes propagate to target
source.set_value(100);
// target.get_value() == 100

// Unbind
binding.remove(); // target retains last value, writes work again
```

### Two-way binding

```cpp
auto source = create_property<int>(42);
auto target = create_property<int>(0);

auto binding = ::velk::create_binding(target, source, Immediate, BindingMode::TwoWay);
// target.get_value() == 42

// Writes to target forward to source
target.set_value(99);
// source.get_value() == 99
// target.get_value() == 99
```

### Function binding with explicit deps

```cpp
auto target = create_property<int>(0);
auto a = create_property<int>(10);
auto b = create_property<int>(20);

Callback fn([](FnArgs args) -> IAny::Ptr {
    auto x = Any<const int>(args[0]);
    auto y = Any<const int>(args[1]);
    return Any<int>(x.get_value() + y.get_value());
});

auto binding = ::velk::create_binding(target, fn, {a, b});
// target.get_value() == 30

a.set_value(5);
// target.get_value() == 25 (re-evaluated)
```

### Function binding with auto-tracked deps

```cpp
auto target = create_property<int>(0);
auto a = create_property<int>(10);

// The function reads properties directly; deps auto-detected
Callback fn([&a](FnArgs) -> IAny::Ptr {
    return Any<int>(a.get_value() * 2);
});

auto binding = ::velk::create_binding(target, fn);
// target.get_value() == 20

a.set_value(5);
// target.get_value() == 10
```

### Multi-target binding

```cpp
auto source = create_property<int>(42);
auto t1 = create_property<int>(0);
auto t2 = create_property<int>(0);

auto binding = ::velk::create_binding(source);
binding.add_target(t1);
binding.add_target(t2);
// t1.get_value() == 42, t2.get_value() == 42

binding.remove_target(t1); // unbind just t1
binding.remove();          // unbind all remaining
```

### Deferred binding

```cpp
auto binding = ::velk::create_binding(target, source, Deferred);
// Changes to source are batched; call instance().update() to flush
```

## 8. Any<T>

```cpp
#include <velk/api/any.h>

// Create
Any<int> a(42);
Any<float> b(3.14f);

// Read
int val = a.get_value();

// Write
a.set_value(100);

// Read-only view
Any<const int> constView(a);
int v = constView.get_value();
// constView.set_value() not available

// Pass to functions
invoke_function(fn, Any<int>(10), Any<float>(3.14f));

// Get underlying IAny pointer
const IAny* ptr = a.get_any_interface();

// Clone (creates independent copy)
IAny::Ptr cloned = a.clone();
```

### ArrayAny

```cpp
ArrayAny<int> arr({1, 2, 3});    // from initializer list
int first = arr.at(0);            // element access
auto vec = arr.get_value();        // vector<int> copy

arr.push_back(4);
arr.set_at(0, 10);
arr.erase_at(1);
arr.clear();
```

## 9. Variant Properties

Variant properties hold any type at runtime with built-in numeric conversions. They cost more than typed properties (runtime type dispatch per access), so only use them when the type is not known at compile time.

### Declaration

```cpp
class IPort : public Interface<IPort>
{
public:
    VELK_INTERFACE(
        (PROP, velk::Variant, value, {})
    )
};
```

### Read/write via accessor

```cpp
auto* port = interface_cast<IPort>(obj);

// Write a float
Any<float> fv(42.f);
port->value().set_value(fv);

// Read back
auto val = port->value().get_value();    // IAny::ConstPtr
Any<const float> typed(val);
float f = typed.get_value();             // 42.f

// Write a different type (replaces stored type)
Any<string> sv(string("hello"));
port->value().set_value(sv);
```

### State struct access

The `Variant` in the State struct shares the same backing `IAny` as the property:

```cpp
// Read via state
auto reader = read_state<IPort>(port);
float f = reader->value.get<float>();
Uid type = reader->value.stored_type();
bool ok = reader->value.can_convert_to(type_uid<double>());

// Write via state (fires on_changed on scope exit)
{
    auto writer = write_state<IPort>(port);
    writer->value.set<int32_t>(99);
}
```

### Numeric conversions

Variant converts between `bool`, `int32_t`, `int64_t`, `uint32_t`, `uint64_t`, `float`, `double`:

```cpp
Any<float> fv(3.14f);
port->value().set_value(fv);

// Read as double (succeeds via conversion)
auto val = port->value().get_value();
double d = 0.0;
val->get_data(&d, sizeof(double), type_uid<double>());  // 3.14
```

## 10. Testing Patterns

```cpp
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <gtest/gtest.h>

using namespace velk;

// Qualify ::velk::Object to avoid ambiguity with ext::Object
// Qualify ::velk::invoke_function similarly if needed

class ITestWidget : public Interface<ITestWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 100.f),
        (EVT, on_clicked),
        (FN, void, reset)
    )
};

class TestWidget : public ext::Object<TestWidget, ITestWidget>
{
public:
    int resetCount = 0;
    void fn_reset() override { resetCount++; }
};

class MyTest : public ::testing::Test
{
protected:
    // Register once per test suite, not per test
    static void SetUpTestSuite()
    {
        instance().type_registry().register_type<TestWidget>();
    }
};

TEST_F(MyTest, CreateAndUse)
{
    auto obj = instance().create<IObject>(TestWidget::static_class_id());
    ASSERT_TRUE(obj);

    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    EXPECT_FLOAT_EQ(iw->width().get_value(), 100.f);
    iw->width().set_value(42.f);
    EXPECT_FLOAT_EQ(iw->width().get_value(), 42.f);

    invoke_function(iw->reset());
}

TEST_F(MyTest, ObjectWrapper)
{
    ::velk::Object obj(instance().create<IObject>(TestWidget::static_class_id()));
    ASSERT_TRUE(obj);

    auto prop = obj.get_property("width");
    ASSERT_TRUE(prop);

    auto reader = obj.read_state<ITestWidget>();
    EXPECT_FLOAT_EQ(reader->width, 100.f);
}
```

## 11. Plugins

### Plugin class definition

**Header** (`src/my_plugin.h`):

```cpp
#include <velk/ext/plugin.h>

namespace mylib {

class MyPlugin final : public velk::ext::Plugin<MyPlugin>
{
public:
    VELK_PLUGIN_UID("a0b1c2d3-e4f5-6789-abcd-ef0123456789");
    VELK_PLUGIN_NAME("my_plugin");
    VELK_PLUGIN_VERSION(1, 0, 0);

    // Optional: declare dependencies
    // VELK_PLUGIN_DEPS({SomeOtherPlugin::class_uid, velk::make_version(1, 0, 0)});

    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig& config) override;
    velk::ReturnValue shutdown(velk::IVelk&) override;

    // Optional: update hooks (requires config.enableUpdate = true)
    // void pre_update(const IPlugin::PreUpdateInfo& info) override;
    // void post_update(const IPlugin::PostUpdateInfo& info) override;
};

} // namespace mylib

// MUST be outside any namespace
VELK_PLUGIN(mylib::MyPlugin)
```

**Implementation** (`src/my_plugin.cpp`):

```cpp
#include "my_plugin.h"
#include "widget_impl.h"  // ext::Object<WidgetImpl, IWidget>

namespace mylib {

velk::ReturnValue MyPlugin::initialize(velk::IVelk& velk, velk::PluginConfig& config)
{
    // Register types using the free function (takes IVelk&)
    // Use &= to chain: first failure is preserved, all operations still execute
    auto rv = ::velk::register_type<WidgetImpl>(velk);
    rv &= ::velk::register_type<AnotherImpl>(velk);

    // Enable update hooks if needed
    // config.enableUpdate = true;

    return rv;
}

velk::ReturnValue MyPlugin::shutdown(velk::IVelk&)
{
    return velk::ReturnValue::Success;
}

} // namespace mylib
```

### Public constants header

Expose stable UIDs for consumers (`include/mylib/plugin.h`):

```cpp
#ifndef MYLIB_PLUGIN_H
#define MYLIB_PLUGIN_H

#include <velk/common.h>

namespace mylib {

namespace ClassId {
inline constexpr velk::Uid Widget{"11111111-2222-3333-4444-555555555555"};
} // namespace ClassId

namespace PluginId {
inline constexpr velk::Uid MyPlugin{"a0b1c2d3-e4f5-6789-abcd-ef0123456789"};
} // namespace PluginId

} // namespace mylib

#endif // MYLIB_PLUGIN_H
```

The implementation class must use `VELK_CLASS_UID` with the same UUID as the `ClassId` constant:

```cpp
class WidgetImpl : public velk::ext::Object<WidgetImpl, IWidget>
{
public:
    VELK_CLASS_UID("11111111-2222-3333-4444-555555555555");
    // ...
};
```

### CMake setup

```cmake
add_library(my_plugin SHARED
    src/my_plugin.h
    src/my_plugin.cpp
    src/widget_impl.h
    include/mylib/intf_widget.h
    include/mylib/plugin.h
)

target_link_libraries(my_plugin PRIVATE velk)

target_include_directories(my_plugin
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(my_plugin PRIVATE VELK_EXPORTS)
```

### Loading and using from an application

```cpp
#include <velk/api/velk.h>
#include <mylib/intf_widget.h>
#include <mylib/plugin.h>

// Load the plugin DLL
::velk::instance().plugin_registry().load_plugin_from_path("my_plugin.dll");

// Create objects using the public ClassId constants
auto obj = ::velk::instance().create<velk::IObject>(mylib::ClassId::Widget);
auto* widget = velk::interface_cast<mylib::IWidget>(obj);

// Lazy load (loads if not already loaded)
auto& reg = ::velk::instance().plugin_registry();
auto plugin = reg.get_or_load_plugin(mylib::PluginId::MyPlugin); // IPlugin::Ptr
```

## 12. Importer Extensions

### Extension class

```cpp
#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>

namespace mylib {

class MyImportHandler : public velk::ext::ObjectCore<MyImportHandler, velk::IImporterExtension>
{
public:
    VELK_CLASS_UID("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx", "MyImportHandler");

    velk::string_view collection_key() const override { return "my_data"; }

    void process(const velk::IImportData& data, velk::IStore& store,
                 const velk::IImportResolver& resolver) const override
    {
        for (size_t i = 0; i < data.count(); i++) {
            auto& entry = data.at(i);

            // IImportData uses null object pattern: safe to chain without null checks
            auto name = entry.find("name").as_string();     // "" if missing
            double value = entry.find("value").as_number();  // 0.0 if missing

            // Resolve objects and properties via the resolver
            auto target_path = entry.find("target").as_string();
            if (!target_path.empty()) {
                // Returns IObject::Ptr; cast to IProperty for property paths
                auto resolved = resolver.resolve(target_path);
                auto prop = velk::interface_pointer_cast<velk::IProperty>(resolved);
                if (prop) {
                    // ... use the property ...
                }
            }

            // Check for null explicitly when needed
            auto& optional = entry.find("optional_field");
            if (!optional.is_null()) {
                // field exists
            }
        }
    }
};

} // namespace mylib
```

### Registration (in plugin initialize)

```cpp
velk::ReturnValue MyPlugin::initialize(velk::IVelk& velk, velk::PluginConfig&) override
{
    return ::velk::register_type<MyImportHandler>(velk);
}
```

### Using the importer

```cpp
#include <velk/plugins/importer/api/importer.h>

// Register class aliases (optional)
::velk::register_import_alias("mylib.Widget", MyWidget::static_class_id());

// Create a JSON importer (loads plugin automatically) and import
auto importer = ::velk::create_json_importer();
auto result = importer.import_from(R"({
    "version": 1,
    "objects": [
        { "id": "w1", "class": "mylib.Widget", "properties": { "width": 100.0 } }
    ],
    "my_data": [
        { "target": "w1.width", "value": 42.0 }
    ]
})");

// Check result
if (result.store) {
    auto obj = result.store->find("w1");
    // ...
}
for (auto& err : result.errors) {
    // handle errors (non-fatal, import continues past errors)
}
```

### JSON property value forms

```json
{
    "properties": {
        "width": 100.0,
        "height": { "value": 200.0, "flags": 1 },
        "target": { "ref": "other_object" },
        "target": { "ref": "other_object", "type": "weak" },
        "source_width": { "bind": "other_object.width" }
    }
}
```

- Bare value: `100.0` (type inferred from class metadata)
- Object form: `{ "value": ..., "flags": ... }`
- Object ref: `{ "ref": "id_or_path" }` (ObjectRef properties only, error on other types)
- Inline binding: `{ "bind": "object.property" }` (one-way, auto invoke)

### Hierarchy format

```json
{
    "hierarchies": {
        "scene": {
            "root": ["panel", "button"],
            "panel": ["label_a", "label_b"]
        }
    }
}
```

Keys are parent ids, values are arrays of child ids. Root is auto-detected (appears as key but never as child).
