#include <velk/api/hierarchy.h>
#include <velk/api/store.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>
#include <velk/string.h>

#include <gtest/gtest.h>

namespace velk {

class ITestImportWidget : public Interface<ITestImportWidget>
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

class TestImportWidget : public ext::Object<TestImportWidget, ITestImportWidget>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000010");
};

class ITestImportPanel : public Interface<ITestImportPanel>
{
public:
    VELK_INTERFACE(
        (PROP, double, opacity, 1.0)
    )
};

class TestImportPanel : public ext::Object<TestImportPanel, ITestImportPanel>
{
public:
    VELK_CLASS_UID("c0000000-0000-0000-0000-000000000011");
};

} // namespace velk

using namespace velk;

class ImporterTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ::velk::instance().type_registry().register_type<TestImportWidget>();
        ::velk::instance().type_registry().register_type<TestImportPanel>();
    }

    static void TearDownTestSuite()
    {
        ::velk::instance().type_registry().unregister_type<TestImportWidget>();
        ::velk::instance().type_registry().unregister_type<TestImportPanel>();
    }

    void load_importer()
    {
        auto& reg = ::velk::instance().plugin_registry();
        reg.load_plugin_from_path(TEST_IMPORTER_DLL_PATH);
        auto plugin = reg.find_plugin(PluginId::ImporterPlugin);
        ASSERT_TRUE(plugin);
        importer_ = interface_cast<IImporterPlugin>(plugin);
        ASSERT_NE(nullptr, importer_);
        importer_->register_class_alias("test.Widget", TestImportWidget::class_id());
        importer_->register_class_alias("test.Panel", TestImportPanel::class_id());
    }

    void TearDown() override
    {
        auto& reg = ::velk::instance().plugin_registry();
        if (reg.find_plugin(PluginId::ImporterPlugin)) {
            reg.unload_plugin(PluginId::ImporterPlugin);
        }
    }

    IImporterPlugin* importer_ = nullptr;
};

TEST_F(ImporterTest, ImportSingleObject)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            {
                "id": "widget_1",
                "class": "test.Widget",
                "properties": {
                    "width": 800.0,
                    "height": 600.0,
                    "count": 42,
                    "visible": false,
                    "label": "Hello"
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(1u, result.store->object_count());

    auto obj = result.store->find("widget_1");
    ASSERT_TRUE(obj);

    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(800.0f, tw->width().get_value());
    EXPECT_FLOAT_EQ(600.0f, tw->height().get_value());
    EXPECT_EQ(42, tw->count().get_value());
    EXPECT_FALSE(tw->visible().get_value());
    EXPECT_EQ(::velk::string_view("Hello"), ::velk::string_view(tw->label().get_value()));
}

TEST_F(ImporterTest, ImportMultipleObjects)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            {
                "id": "widget_1",
                "class": "test.Widget",
                "properties": { "width": 100.0 }
            },
            {
                "id": "panel_1",
                "class": "test.Panel",
                "properties": { "opacity": 0.5 }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(2u, result.store->object_count());

    auto w = result.store->find("widget_1");
    ASSERT_TRUE(w);
    EXPECT_FLOAT_EQ(100.0f, interface_cast<ITestImportWidget>(w)->width().get_value());

    auto p = result.store->find("panel_1");
    ASSERT_TRUE(p);
    EXPECT_DOUBLE_EQ(0.5, interface_cast<ITestImportPanel>(p)->opacity().get_value());
}

TEST_F(ImporterTest, ImportWithHierarchy)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            { "id": "root", "class": "test.Widget", "properties": { "width": 1.0 } },
            { "id": "child_a", "class": "test.Widget", "properties": { "width": 2.0 } },
            { "id": "child_b", "class": "test.Widget", "properties": { "width": 3.0 } }
        ],
        "hierarchies": {
            "scene": [
                { "parent": "root", "child": "child_a" },
                { "parent": "root", "child": "child_b" }
            ]
        }
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(3u + 1u, result.store->object_count()); // 3 objects + 1 hierarchy

    auto hierarchy_obj = result.store->find("hierarchy:scene");
    ASSERT_TRUE(hierarchy_obj);

    Hierarchy h(hierarchy_obj);
    ASSERT_TRUE(h);

    auto root_node = h.root();
    ASSERT_TRUE(root_node);

    auto root_obj = result.store->find("root");
    EXPECT_EQ(root_obj, root_node.object());

    auto children = root_node.get_children();
    EXPECT_EQ(2u, children.size());
}

TEST_F(ImporterTest, ImportByClassUid)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            {
                "id": "widget_uid",
                "class": "c0000000-0000-0000-0000-000000000010",
                "properties": { "width": 42.0 }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    auto obj = result.store->find("widget_uid");
    ASSERT_TRUE(obj);

    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(42.0f, tw->width().get_value());
}

TEST_F(ImporterTest, UnknownClassReportsError)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            { "id": "unknown", "class": "test.Unknown", "properties": {} },
            { "id": "known", "class": "test.Widget", "properties": { "width": 1.0 } }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_EQ(1u, result.store->object_count());
    EXPECT_FALSE(result.store->find("unknown"));
    EXPECT_TRUE(result.store->find("known"));

    ASSERT_EQ(1u, result.errors.size());
    EXPECT_NE(::velk::string::npos, result.errors[0].find("test.Unknown"));
}

TEST_F(ImporterTest, InvalidJsonReportsError)
{
    load_importer();
    auto result = importer_->import_from_json("{invalid");
    EXPECT_FALSE(result.store);
    ASSERT_FALSE(result.errors.empty());
}

TEST_F(ImporterTest, UnknownPropertyReportsError)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "test.Widget",
                "properties": {
                    "width": 1.0,
                    "nonexistent": 42
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_EQ(1u, result.store->object_count());

    ASSERT_EQ(1u, result.errors.size());
    EXPECT_NE(::velk::string::npos, result.errors[0].find("nonexistent"));
}

TEST_F(ImporterTest, PropertyObjectForm)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            {
                "id": "w1",
                "class": "test.Widget",
                "properties": {
                    "width": { "value": 999.0 }
                }
            }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);
    EXPECT_FLOAT_EQ(999.0f, interface_cast<ITestImportWidget>(obj)->width().get_value());
}

TEST_F(ImporterTest, DefaultValues)
{
    load_importer();
    auto result = importer_->import_from_json(R"({
        "version": 1,
        "objects": [
            { "id": "w1", "class": "test.Widget", "properties": {} }
        ]
    })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    auto obj = result.store->find("w1");
    ASSERT_TRUE(obj);

    auto* tw = interface_cast<ITestImportWidget>(obj);
    ASSERT_NE(nullptr, tw);
    EXPECT_FLOAT_EQ(0.0f, tw->width().get_value());
    EXPECT_TRUE(tw->visible().get_value());
}

TEST_F(ImporterTest, EmptyStore)
{
    load_importer();
    auto result = importer_->import_from_json(R"({ "version": 1, "objects": [] })");

    ASSERT_TRUE(result.store);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(0u, result.store->object_count());
}
