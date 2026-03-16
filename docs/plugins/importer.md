# Importer plugin

The importer plugin (`velk_importer`) loads JSON scene files into Velk. It creates objects, sets properties, builds hierarchies, resolves references, creates bindings, and dispatches plugin-specific data to registered extensions. See the [JSON schema](../../velk/plugins/importer/schema/velk-store.schema.json) for the formal format definition.

## Contents

- [Loading the plugin](#loading-the-plugin)
- [JSON format](#json-format)
  - [Objects](#objects)
  - [Class resolution](#class-resolution)
  - [Properties](#properties)
  - [Hierarchies](#hierarchies)
  - [Object references](#object-references)
  - [Bindings](#bindings)
  - [Extension collections](#extension-collections)
- [Import result](#import-result)
- [Importer extensions](#importer-extensions)
  - [IImportData](#iimportdata)
  - [IImportResolver](#iimportresolver)
  - [Writing an extension](#writing-an-extension)
  - [Example: animator extension](#example-animator-extension)

## Loading the plugin

```cpp
#include <velk/plugins/importer/interface/intf_importer_plugin.h>
#include <velk/plugins/importer/plugin.h>

// Load the plugin
instance().plugin_registry().load_plugin_from_path("velk_importer.dll");

// Get the plugin interface
auto plugin = instance().plugin_registry().find_plugin(PluginId::ImporterPlugin);
auto* importer = interface_cast<IImporterPlugin>(plugin);

// Import a JSON string
auto result = importer->import_from_json(json_string);
```

## JSON format

A store file is a JSON object with a required `version` field and optional top-level collections:

```json
{
    "version": 1,
    "objects": [...],
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
2. **Registered alias**: Custom names registered via `importer->register_class_alias("myapp.Widget", uid)`. Useful for application-level code that drives imports directly.
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
"target": { "ref": "other_widget" }
"target": { "ref": "other_widget", "type": "weak" }
```

Only valid on properties of type `ObjectRef`. Default is strong; add `"type": "weak"` for a non-owning reference.

**Inline binding** (any property):

```json
"width": { "bind": "source_widget.width" }
```

Creates a one-way binding from the source property to this property. Equivalent to adding a top-level binding entry but more concise for single-source single-target cases.

Supported value types: `float`, `double`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `int`, `bool`, `velk::string`.

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

### Object references

Object references point to other objects in the store. They are set on properties of type `ObjectRef`:

```json
"target": { "ref": "widget_1" }
```

References support the same path formats used elsewhere:

- **Direct id**: `"widget_1"`
- **Hierarchy path**: `"/scene/root/panel"` walks the hierarchy tree matching object names at each level.

Add `"type": "weak"` for a non-owning reference:

```json
"target": { "ref": "widget_1", "type": "weak" }
```

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

`import_from_json()` returns an `ImportResult`:

```cpp
struct ImportResult {
    IStore::Ptr store;       // The imported store, or null if parsing failed
    vector<string> errors;   // Collected errors (non-fatal problems are reported here)
};
```

The importer is lenient: it continues past individual errors (unknown class, bad property, unresolved reference) and reports them in the `errors` vector. The store is still populated with everything that could be resolved. Only a complete parse failure (invalid JSON, root not an object) results in a null store.

## Importer extensions

Extensions let plugins add their own top-level collections to the JSON format without depending on the importer plugin. The extension interface and supporting types are defined in velk core (`intf_importer_extension.h`).

### IImportData

Extensions receive data through `IImportData`, a format-neutral read-only data tree. This decouples extensions from JSON, so the transport format can change (e.g. to binary) without modifying extension code.

```cpp
class IImportData {
public:
    enum class Kind : uint8_t { Null, Bool, Number, String, Array, Object };

    Kind kind() const;
    bool as_bool() const;
    double as_number() const;
    string_view as_string() const;
    size_t count() const;
    const IImportData* at(size_t index) const;
    const IImportData* find(string_view key) const;
    string_view key_at(size_t index) const;
    bool is_null() const;
};
```

`IImportData` uses the null object pattern: `find()` on a missing key and `at()` out of bounds return a static null node that returns zero/empty for all accessors and itself for further `find`/`at` calls. This makes chaining safe without null checks:

```cpp
auto duration = entry->find("duration")->as_number();  // 0.0 if missing
auto name = entry->find("config")->find("name")->as_string();  // "" if either key missing
```

### IImportResolver

Extensions receive an `IImportResolver` that provides the same path resolution as the core importer:

```cpp
class IImportResolver {
public:
    IObject::Ptr resolve(string_view path) const;
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
            auto* entry = data.at(i);
            auto target_path = entry->find("target")->as_string();
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
