#include <velk/api/any.h>
#include <velk/api/binding.h>
#include <velk/api/callback.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/interface/intf_property.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>
#include <velk/plugins/script/js/plugin.h>
#include <velk/string.h>

#include <gtest/gtest.h>

namespace velk {

class ITestJsWidget : public Interface<ITestJsWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 0.f),
        (PROP, float, height, 0.f),
        (PROP, int, count, 0),
        (PROP, bool, visible, true),
        (PROP, string, label, "")
    )
};

class TestJsWidget : public ext::Object<TestJsWidget, ITestJsWidget>
{
public:
    VELK_CLASS_UID("e0000000-0000-0000-0000-000000000001", "JsWidget");
};

} // namespace velk

using namespace velk;

class JsTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ::velk::instance().type_registry().register_type<TestJsWidget>();
    }

    static void TearDownTestSuite()
    {
        ::velk::instance().type_registry().unregister_type<TestJsWidget>();
    }

    void SetUp() override
    {
        auto& reg = ::velk::instance().plugin_registry();
        reg.load_plugin_from_path(TEST_IMPORTER_DLL_PATH);
        auto js_rv = reg.load_plugin_from_path(TEST_JS_DLL_PATH);
        ASSERT_TRUE(succeeded(js_rv)) << "Failed to load JS plugin from " << TEST_JS_DLL_PATH;

        auto plugin = reg.find_plugin(PluginId::ImporterPlugin);
        ASSERT_TRUE(plugin);
        auto* ip = interface_cast<IImporterPlugin>(plugin);
        ASSERT_NE(nullptr, ip);
        ip->register_class_alias("test.JsWidget", TestJsWidget::static_class_id());

        auto obj = ::velk::instance().create<IStoreImporter>(ClassId::JsonImporter);
        ASSERT_TRUE(obj);
        importer_ = obj;
    }

    void TearDown() override
    {
        importer_ = nullptr;
        auto& reg = ::velk::instance().plugin_registry();
        if (reg.find_plugin(PluginId::JsPlugin)) {
            reg.unload_plugin(PluginId::JsPlugin);
        }
        if (reg.find_plugin(PluginId::ImporterPlugin)) {
            reg.unload_plugin(PluginId::ImporterPlugin);
        }
    }

    ImportResult import_json(const char* json)
    {
        return importer_->import_from(string_view(json, std::strlen(json)));
    }

    IStoreImporter::Ptr importer_;
};

// Verify a plain Callback binding works (sanity check, no JS involved)
TEST_F(JsTest, PlainCallbackBindingWorks)
{
    auto prop = ::velk::create_property<int>(0);
    ASSERT_TRUE(prop);

    Callback fn([](FnArgs) -> IAny::Ptr {
        return Any<int>(42);
    });
    auto binding = ::velk::create_binding(IFunction::ConstPtr(IFunction::Ptr(fn)));
    bool added = binding.add_target(prop);
    EXPECT_TRUE(added) << "add_target failed for plain callback";

    EXPECT_EQ(42, prop.get_value());
}


// Verify that JsImportHandler is registered and discoverable
TEST_F(JsTest, ImportHandlerRegistered)
{
    auto& registry = ::velk::instance().type_registry();
    bool found = false;
    registry.for_each_class(&found, [](void* ctx, const ClassInfo& info) -> bool {
        for (size_t i = 0; i < info.interfaces.size(); i++) {
            if (info.interfaces[i].uid == IImporterExtension::UID) {
                auto* f = static_cast<bool*>(ctx);
                *f = true;
            }
        }
        return true;
    });
    EXPECT_TRUE(found) << "No IImporterExtension found in type registry";
}

// Verify that the JS plugin loads successfully
TEST_F(JsTest, PluginLoads)
{
    auto plugin = ::velk::instance().plugin_registry().find_plugin(PluginId::JsPlugin);
    ASSERT_TRUE(plugin);
}

// Import with no scripts section: should work without errors
TEST_F(JsTest, ImportWithoutScripts)
{
    auto result = import_json(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.JsWidget", "properties": { "width": 100 } }
        ]
    })");
    ASSERT_TRUE(result.store);
    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    auto* tw = interface_cast<ITestJsWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(100.f, tw->width().get_value());
}

// Expression binding: child.width = source.width * 2
TEST_F(JsTest, ExpressionBindingMultiply)
{
    auto result = import_json(R"({
        "version": 1,
        "objects": [
            { "id": "source", "name": "source", "class": "test.JsWidget", "properties": { "width": 50 } },
            { "id": "child", "name": "child", "class": "test.JsWidget" }
        ],
        "scripts": [
            { "target": "child.width", "expr": "source.width * 2" }
        ]
    })");
    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty()) << "Errors: " << result.errors[0].c_str();

    // Flush deferred binding evaluation
    ::velk::instance().update({});

    auto child = result.store->find("child");
    ASSERT_TRUE(child);
    auto* tw = interface_cast<ITestJsWidget>(child);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(100.f, tw->width().get_value());
}

// Expression binding: simple constant expression
TEST_F(JsTest, ExpressionBindingConstant)
{
    auto result = import_json(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "name": "w1", "class": "test.JsWidget" }
        ],
        "scripts": [
            { "target": "w1.count", "expr": "21 + 21" }
        ]
    })");
    ASSERT_TRUE(result.store);

    ::velk::instance().update({});

    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    auto* tw = interface_cast<ITestJsWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_EQ(42, tw->count().get_value());
}

// Expression binding: boolean expression
TEST_F(JsTest, ExpressionBindingBool)
{
    auto result = import_json(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "name": "w1", "class": "test.JsWidget", "properties": { "count": 5 } }
        ],
        "scripts": [
            { "target": "w1.visible", "expr": "w1.count > 3" }
        ]
    })");
    ASSERT_TRUE(result.store);

    ::velk::instance().update({});

    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    auto* tw = interface_cast<ITestJsWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_TRUE(tw->visible().get_value());
}

// Expression binding: string expression
TEST_F(JsTest, ExpressionBindingString)
{
    auto result = import_json(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "name": "w1", "class": "test.JsWidget" }
        ],
        "scripts": [
            { "target": "w1.label", "expr": "'Hello' + ' ' + 'World'" }
        ]
    })");
    ASSERT_TRUE(result.store);

    ::velk::instance().update({});

    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    auto* tw = interface_cast<ITestJsWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_EQ(::velk::string_view("Hello World"), ::velk::string_view(tw->label().get_value()));
}

// Event handler: set a property when an event fires
TEST_F(JsTest, EventHandler)
{
    auto result = import_json(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "name": "w1", "class": "test.JsWidget", "properties": { "label": "before" } }
        ],
        "scripts": [
            { "event": "w1.on_changed_label", "handler": "w1.count = 99" }
        ]
    })");
    ASSERT_TRUE(result.store);

    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    auto* tw = interface_cast<ITestJsWidget>(obj);
    ASSERT_NE(nullptr, tw);

    // Change label to trigger the event
    tw->label().set_value(::velk::string("after"));
    ::velk::instance().update({});

    EXPECT_EQ(99, tw->count().get_value());
}

// Multi-line handler
TEST_F(JsTest, MultiLineHandler)
{
    auto result = import_json(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "name": "w1", "class": "test.JsWidget" }
        ],
        "scripts": [
            {
                "event": "w1.on_changed_width",
                "handler": [
                    "let w = w1.width;",
                    "w1.count = w * 10;"
                ]
            }
        ]
    })");
    ASSERT_TRUE(result.store);

    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    auto* tw = interface_cast<ITestJsWidget>(obj);
    ASSERT_NE(nullptr, tw);

    tw->width().set_value(5.f);
    ::velk::instance().update({});

    EXPECT_EQ(50, tw->count().get_value());
}
