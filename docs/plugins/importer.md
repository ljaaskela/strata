# Importer plugin

The importer plugin (`velk_importer`) loads scene files into Velk. It creates objects, sets properties, builds hierarchies, resolves references, creates bindings, and dispatches plugin-specific data to registered extensions.

Currently the plugin ships a JSON importer (`ClassId::JsonImporter`). The architecture supports additional formats (binary, etc.) as separate `IStoreImporter` implementations registered by the same or other plugins. Extensions receive data through the format-neutral `IImportData` interface, so they work with any importer without changes.

See the [JSON schema](../../velk/plugins/importer/schema/velk-store.schema.json) for the formal format definition.

## Contents

- [Usage](#usage)
- [JSON format](#json-format)
  - [Objects](#objects)
  - [Class resolution](#class-resolution)
  - [Properties](#properties)
  - [Attachments](#attachments)
  - [Hierarchies](#hierarchies)
  - [Bindings](#bindings)
  - [Extension collections](#extension-collections)
- [Import result](#import-result)
- [Importer extensions](#importer-extensions)
  - [IImportData](#iimportdata)
  - [IImportResolver](#iimportresolver)
  - [Writing an extension](#writing-an-extension)
  - [Example: animator extension](#example-animator-extension)
- [Type extensions](#type-extensions)
  - [IImporterTypeExtension](#iimportertypeextension)
  - [Writing a type extension](#writing-a-type-extension)
  - [Example: dim type (velk-ui)](#example-dim-type-velk-ui)
- [Advanced](#advanced)
  - [Class aliases](#class-aliases)
  - [Direct interface access](#direct-interface-access)

## Usage

```cpp
#include <velk/plugins/importer/api/importer.h>

auto importer = velk::create_json_importer();
auto result = importer.import_from(json_string);

if (result) {
    auto obj = result.store->find("widget_1");
    // ...
}
```

## JSON format

A store file is a JSON object with a required `version` field and optional top-level collections:

```json
{
    "version": 1,
    "objects": [...],
    "attachments": [...],
    "hierarchies": { ... },
    "bindings": [...]
}
```

Additional top-level keys are dispatched to [importer extensions](#importer-extensions).

### Objects

Objects are a flat array. Each entry has an `id` (unique within the store), a `class`, and optional `properties` and `name`:

```json
{
    "objects": [
        {
            "id": "widget_1",
            "name": "Main Widget",
            "class": "myapp.Widget",
            "properties": {
                "width": 800.0,
                "height": 600.0,
                "visible": true,
                "label": "Hello"
            }
        }
    ]
}
```

The `id` is used for store lookup and reference resolution. The `name` is optional and provides a human-readable label.

### Class resolution

The `class` field supports three formats, tried in order:

1. **UUID string**: `"c0000000-0000-0000-0000-000000000010"` matches directly against the type registry.
2. **Registered alias**: Custom names registered via [`register_import_alias()`](#class-aliases).
3. **Scoped class name**: `"plugin-name.ClassName"` resolves via `ITypeRegistry::find_class_by_name()`. Classes with a friendly name set via `VELK_CLASS_UID(uid, "ClassName")` and registered by a plugin named `"plugin-name"` are found as `"plugin-name.ClassName"`. Unscoped names do a first-match lookup.

### Properties

Property values are inferred from the class metadata (`VELK_INTERFACE` declarations). No type annotations needed in JSON.

**Shorthand** (value only):

```json
"width": 200.0
```

**Object form** (value with flags):

```json
"width": { "value": 200.0, "flags": 1 }
```

**Object reference** (ObjectRef properties only):

```json
"target": { "ref": "widget_1" }
"target": { "ref": "/scene/root/panel" }
"target": { "ref": "widget_1", "type": "weak" }
```

Only valid on properties of type `ObjectRef`. The `ref` path supports direct ids (looked up by `id` then `name`), and hierarchy paths starting with `/` that walk the tree by matching object names at each level. Default is strong (owning); add `"type": "weak"` for a non-owning reference.

**Inline binding** (any property):

```json
"width": { "bind": "source_widget.width" }
```

Creates a one-way binding from the source property to this property. Equivalent to adding a top-level binding entry but more concise for single-source single-target cases.

Supported value types: `float`, `double`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `int`, `bool`, `velk::string`.

### Attachments

Attachments create objects and attach them to target objects via `IObjectStorage::add_attachment()`. Each entry has a `class`, optional `properties`, and a `targets` array of object ids:

```json
{
    "attachments": [
        {
            "targets": ["child1", "child3"],
            "class": "velk-ui.FixedSize",
            "properties": { "height": "150px" }
        },
        {
            "targets": ["child1"],
            "class": "velk-ui.RectVisual",
            "properties": { "color": { "r": 0.9, "g": 0.2, "b": 0.2 } }
        }
    ]
}
```

The importer creates one object per entry and attaches it to every target in the `targets` array. When an entry targets multiple objects, they share the same attachment instance. Class resolution and property setting follow the same rules as top-level objects.

Target objects must have `IObjectStorage` (which they do if they inherit from `ext::Object`). Targets are resolved by id using the same lookup as object references.

### Hierarchies

Hierarchies are a top-level object mapping hierarchy names to parent-children maps. Each key is a parent object id, and its value is an array of child object ids:

```json
{
    "hierarchies": {
        "scene": {
            "root": ["panel", "button"],
            "panel": ["label"]
        }
    }
}
```

The root is determined automatically (a node that appears as a key but never in any child array). Hierarchies are stored in the result store as `"hierarchy:<name>"`.

The same path formats (direct ids and hierarchy paths) are used in bindings, inline binds, and animation targets.

### Bindings

Top-level bindings connect a source property to one or more target properties:

```json
{
    "bindings": [
        {
            "source": "widget_1.width",
            "targets": ["widget_2.width", "widget_3.width"],
            "mode": "oneway",
            "invoke": "auto"
        }
    ]
}
```

- `source`: Property path (`"object_id.property_name"`).
- `targets`: Array of property paths.
- `mode` (optional): `"oneway"` (default) or `"twoway"`.
- `invoke` (optional): `"auto"` (default), `"immediate"`, or `"deferred"`.

Inline bindings can also be created in property values using `{ "bind": "source.property" }`. An inline bind is equivalent to a top-level binding with `"mode": "oneway"`, `"invoke": "auto"`, and a single target:

```json
"width": { "bind": "widget_1.width" }

// equivalent to:
"bindings": [{ "source": "widget_1.width", "targets": ["this_object.width"] }]
```

### Extension collections

Any additional top-level keys are dispatched to registered `IImporterExtension` implementations. For example, the [animator plugin](animator.md) handles the `"animations"` key:

```json
{
    "version": 1,
    "objects": [...],
    "animations": [
        {
            "type": "transition",
            "targets": ["widget_1.width", "widget_1.height"],
            "duration": 0.3,
            "easing": "out_cubic"
        },
        {
            "type": "track",
            "targets": ["widget_1.opacity"],
            "keyframes": [
                { "time": 0.0, "value": 0.0 },
                { "time": 0.5, "value": 1.0, "easing": "out_quad" },
                { "time": 2.0, "value": 0.8 }
            ]
        }
    ]
}
```

Unknown keys without a matching extension are silently ignored.

## Import result

`import_from()` returns an `ImportResult`:

```cpp
struct ImportResult {
    IStore::Ptr store;
    vector<string> errors;
    explicit operator bool() const;  // true if store is valid and no errors
};
```

The importer is lenient: it continues past individual errors (unknown class, bad property, unresolved reference) and reports them in the `errors` vector. The store is still populated with everything that could be resolved. Only a complete parse failure (invalid JSON, root not an object) results in a null store.

## Importer extensions

Extensions let plugins add their own top-level collections to the JSON format without depending on the importer plugin. The extension interface and supporting types are defined in velk core (`intf_importer_extension.h`).

### IImportData

Extensions receive data through `IImportData`, a format-neutral read-only data tree. This decouples extensions from JSON, so the transport format can change (e.g. to binary) without modifying extension code.

```cpp
class IImportData : public Interface<IImportData> {
public:
    /** @brief Type discriminator for data nodes. */
    enum class Kind : uint8_t { Null, Bool, Number, String, Array, Object };
    /** @brief Returns the type of this node. */
    virtual Kind kind() const = 0;
    /** @brief Returns true if this is a null node. */
    virtual bool is_null() const = 0;
    /** @brief Returns the boolean value, or false if not a Bool node. */
    virtual bool as_bool() const = 0;
    /** @brief Returns the numeric value, or 0.0 if not a Number node. */
    virtual double as_number() const = 0;
    /** @brief Returns the string value, or empty if not a String node. */
    virtual string_view as_string() const = 0;
    /** @brief Array and object: number of elements/entries. */
    virtual size_t count() const = 0;
    /** @brief Array: indexed element. Object: value at index (insertion order). */
    virtual const IImportData& at(size_t index) const = 0;
    /** @brief Object: value for key. Returns static null node if missing. */
    virtual const IImportData& find(string_view key) const = 0;
    /** @brief Object: key name at index (for iteration). */
    virtual string_view key_at(size_t index) const = 0;
};
```

`IImportData` uses the null object pattern: `find()` on a missing key and `at()` out of bounds return a static null node that returns zero/empty for all accessors and itself for further `find`/`at` calls. This makes chaining safe without null checks:

```cpp
auto duration = entry.find("duration").as_number();  // 0.0 if missing
auto name = entry.find("config").find("name").as_string();  // "" if either key missing
```

### IImportResolver

Extensions receive an `IImportResolver` that provides the same path resolution as the core importer:

```cpp
class IImportResolver : public Interface<IImportResolver> {
public:
    /**
     * @brief Resolves a path to an object or property.
     * @param path Direct id ("w1"), hierarchy path ("/scene/root/child"),
     *             or property path ("w1.width"). Property paths return the
     *             IProperty wrapped as IObject::Ptr.
     * @return The resolved object, or nullptr if not found.
     */
    virtual IObject::Ptr resolve(string_view path) const = 0;
};
```

The resolver handles all path formats:

- **Direct id**: `"widget_1"` looks up the object by id or name.
- **Hierarchy path**: `"/scene/root/child"` walks the hierarchy tree.
- **Property path**: `"widget_1.width"` or `"/scene/root/child.width"` resolves the object, then returns the property from its metadata. Cast the result with `interface_pointer_cast<IProperty>()`.

### Writing an extension

1. Define a class implementing `IImporterExtension` via `ext::ObjectCore`:

```cpp
#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>

class MyImportHandler : public ext::ObjectCore<MyImportHandler, IImporterExtension>
{
public:
    VELK_CLASS_UID("...", "MyImportHandler");

    string_view collection_key() const override { return "my_data"; }

    void process(const IImportData& data, IStore& store,
                 const IImportResolver& resolver) const override
    {
        for (size_t i = 0; i < data.count(); i++) {
            auto& entry = data.at(i);
            auto target_path = entry.find("target").as_string();
            auto obj = resolver.resolve(target_path);
            // ... use the resolved object ...
        }
    }
};
```

2. Register the type in your plugin's `initialize()`:

```cpp
ReturnValue MyPlugin::initialize(IVelk& velk, PluginConfig&) override
{
    velk::register_type<MyImportHandler>(velk);
    return ReturnValue::Success;
}
```

The importer discovers extensions automatically via the type registry at import time. No coupling between your plugin and the importer plugin is needed.

### Example: animator extension

The animator plugin registers an `AnimatorImportHandler` that handles the `"animations"` top-level key. It supports two animation types:

**Transitions** (implicit property animations):

```json
{
    "type": "transition",
    "targets": ["widget_1.width", "widget_1.height"],
    "duration": 0.3,
    "easing": "out_cubic"
}
```

After import, any `set_value` call on the target properties animates smoothly to the new value.

**Tracks** (keyframe-based animations):

```json
{
    "type": "track",
    "targets": ["widget_1.opacity"],
    "keyframes": [
        { "time": 0.0, "value": 0.0 },
        { "time": 0.5, "value": 1.0, "easing": "out_quad" },
        { "time": 2.0, "value": 0.8 }
    ],
    "autoplay": true
}
```

Tracks play automatically by default. Set `"autoplay": false` to create the track in idle state.

Both types support multi-target: a single animation entry can target multiple properties. All path formats supported by the resolver work in `"targets"` arrays.

See [Animator plugin](animator.md) for the full list of easing function names and details on how transitions and tracks work.

## Type extensions

Type extensions let plugins teach the importer how to deserialize custom value types. The built-in property dispatch handles `float`, `int32_t`, `bool`, `string`, `vec2`, `vec3`, `color`, `aabb`, etc. When a property's type UID has no built-in handler, the importer falls back to registered type extensions.

### IImporterTypeExtension

```cpp
class IImporterTypeExtension : public Interface<IImporterTypeExtension>
{
public:
    virtual array_view<Uid> supported_types() const = 0;
    virtual IAny::Ptr deserialize(Uid type_uid, const IImportData& data) const = 0;
};
```

At import time the importer scans `ITypeRegistry` for all classes implementing `IImporterTypeExtension` and builds a type UID lookup table. When a property's type has no built-in match, it queries the table and calls `deserialize()` on the matching extension.

`supported_types()` returns the type UIDs this extension handles. `deserialize()` receives the type UID and the raw `IImportData` node, and returns an `IAny::Ptr` holding the parsed value (or nullptr on failure).

### Writing a type extension

1. Define a class implementing `IImporterTypeExtension`:

```cpp
#include <velk/ext/core_object.h>
#include <velk/interface/intf_importer_extension.h>
#include <velk/api/any.h>

class MyTypeExtension
    : public velk::ext::ObjectCore<MyTypeExtension, velk::IImporterTypeExtension>
{
public:
    VELK_CLASS_UID("...", "MyTypeExtension");

    velk::array_view<velk::Uid> supported_types() const override
    {
        static const velk::Uid types[] = { velk::type_uid<MyType>() };
        return { types, 1 };
    }

    velk::IAny::Ptr deserialize(velk::Uid, const velk::IImportData& data) const override
    {
        if (data.kind() == velk::IImportData::Kind::String) {
            return velk::Any<MyType>(parse_my_type(data.as_string()));
        }
        return {};
    }
};
```

2. Register the extension **and** the `AnyValue<T>` factory in your plugin's `initialize()`:

```cpp
velk::register_type<MyTypeExtension>(velk);
velk::register_type<velk::ext::AnyValue<MyType>>(velk);
```

The `AnyValue<MyType>` registration is required. Without it, `Any<MyType>(value)` cannot create the underlying `IAny` storage and will fail at runtime. This is easy to miss because the extension itself registers fine; the crash only happens when the importer actually tries to deserialize a value of that type.

The importer discovers type extensions automatically via the type registry. No coupling between your plugin and the importer plugin is needed.

### Example: dim type (velk-ui)

The velk-ui plugin defines a `dim` value type that represents a dimension with a unit (pixels, percentage, or none). The JSON format accepts strings like `"150px"`, `"50%"`, or bare numbers:

```json
{
    "attachments": [
        {
            "targets": ["child1"],
            "class": "velk-ui.FixedSize",
            "properties": { "height": "150px", "width": "50%" }
        }
    ]
}
```

The `DimTypeExtension` handles deserialization:

```cpp
velk::array_view<velk::Uid> DimTypeExtension::supported_types() const
{
    static const velk::Uid types[] = { velk::type_uid<dim>() };
    return { types, 1 };
}

velk::IAny::Ptr DimTypeExtension::deserialize(velk::Uid, const velk::IImportData& data) const
{
    if (data.kind() == velk::IImportData::Kind::String) {
        return velk::Any<dim>(parse_dim(data.as_string()));
    }
    if (data.kind() == velk::IImportData::Kind::Number) {
        return velk::Any<dim>(dim::px(static_cast<float>(data.as_number())));
    }
    return {};
}
```

Registration in the plugin:

```cpp
velk::register_type<DimTypeExtension>(velk);
velk::register_type<velk::ext::AnyValue<dim>>(velk);
```

## Advanced

### Class aliases

Class names are normally resolved automatically from the type registry. If you need custom name mappings:

```cpp
velk::register_import_alias("myapp.Widget", MyWidget::static_class_id());
```

Aliases are registered on the importer plugin and apply to all importers.

### Direct interface access

The `Importer` wrapper is a thin handle around `IStoreImporter`. For direct interface access:

```cpp
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>

// Create via type registry
auto importer = instance().create<IStoreImporter>(ClassId::JsonImporter);
auto result = importer->import_from(json_string);

// Access the plugin directly
auto plugin = instance().plugin_registry().find_plugin(PluginId::ImporterPlugin);
auto* ip = interface_cast<IImporterPlugin>(plugin);
ip->register_class_alias("myapp.Widget", uid);
```
