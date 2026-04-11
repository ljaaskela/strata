---
name: velk
description: "Velk C++ component object model: VELK_INTERFACE macro, Object<T> CRTP, properties, variant properties, events, functions, bindings, interface_cast, type registration, create objects, IMetadata, StateReader, StateWriter, plugins, ext::Plugin<T>, VELK_PLUGIN, IPluginRegistry, load_plugin_from_path, importer plugin, IStoreImporter, IImporterExtension, IImportData, IImportResolver, JSON import, animations"
---

# Velk Usage Guide

Velk is a C++17 component object model shipped as a shared DLL. Consumers depend only on public headers in `velk/include/velk/`. Objects are composed from pure virtual interfaces, implemented via CRTP, registered with a global type registry, and created through a factory. All values are type-erased through `IAny`.

## Object Lifecycle (5 steps)

1. **Define interface** with `VELK_INTERFACE` (generates static metadata, State struct, typed accessors)
2. **Implement** with `ext::Object<MyClass, IFace1, IFace2, ...>` (collects metadata from all interfaces)
3. **Register** with `instance().type_registry().register_type<MyClass>()`
4. **Create** with `instance().create<IObject>(MyClass::static_class_id())`
5. **Use** via `interface_cast<IMyInterface>(obj)->member()` or `velk::Object` wrapper. For state access prefer the free functions `read_state<T>(obj)` / `write_state<T>(obj)` (from `velk/api/state.h`)

## VELK_INTERFACE Member Types

| Macro | Syntax | Accessor returns | Notes |
|-------|--------|-----------------|-------|
| `PROP` | `(PROP, Type, name, default)` | `Property<Type>` | Read-write property |
| `RPROP` | `(RPROP, Type, name, default)` | `ConstProperty<Type>` | Read-only (write via state only) |
| `ARR` | `(ARR, ElemType, name, ...)` | `ArrayProperty<ElemType>` | Read-write array |
| `RARR` | `(RARR, ElemType, name, ...)` | `ConstArrayProperty<ElemType>` | Read-only array |
| `PROP` | `(PROP, velk::Variant, name, {})` | `Property<Variant>` | Typeless property (any type at runtime) |
| `EVT` | `(EVT, name)` | `Event` | Observable event (zero args) |
| `EVT` | `(EVT, name, (T1, a1), ...)` | `Event` | Observable event with typed signature |
| `FN` | `(FN, RetType, name)` | `Function` | Typed function, zero args |
| `FN` | `(FN, RetType, name, (T1, a1), ...)` | `Function` | Typed function with args |
| `FN_RAW` | `(FN_RAW, name)` | `Function` | Raw function receiving `FnArgs` |

## Key Rules and Pitfalls

- **Interfaces** must inherit from `Interface<T>` (or `Interface<T, Base>` for interface inheritance). All functions declared on an interface must be pure virtual (`= 0`)
- **Implementations** must inherit from `ext::Object<T, Interfaces...>`
- **Register before create**: call `register_type<T>()` before any `create<>()`
- **Casting**: use `interface_cast<T>(obj)` for raw pointers, `interface_pointer_cast<T>(ptr)` for `shared_ptr`
- **Name qualification**: when `using namespace velk`, qualify `::velk::Object`, `::velk::invoke_function`, etc. to avoid ambiguity with `velk::velk` (the `velk` name exists inside the namespace)
- **No DLL exports**: never export functions from the DLL. Use `instance()` as the single entry point
- **FN overrides** use `fn_` prefix: `(FN, void, reset)` generates virtual `fn_reset()` to override
- **FN_RAW overrides** have signature `IAny::Ptr fn_Name(FnArgs)`
- **Variant properties**: property set is expensive (~96 ns same-type, ~154 ns type-change) due to `copy_from` cloning. For write-heavy code, prefer state struct access (`Variant::set<T>()` at ~9 ns). Property get returns an IAny pointer (~8 ns). Prefer typed `PROP` when the type is known at compile time
- **Events** conventionally use `on_` prefix: `(EVT, on_clicked)`. Add a typed signature with the same `(Type, name)` syntax as FN: `(EVT, on_resized, (int, w), (int, h))`. The signature is metadata only — `Event::invoke()` is still untyped — but it documents the event and is queryable via `MemberDesc::functionKind()->args`.
- **Deferred updates**: pass `Deferred` to `set_value()` or `write_state()`, then call `instance().update()` to flush
- **State access**: prefer `read_state<T>(obj)` / `write_state<T>(obj)` free functions over `meta->read<T>()` / `meta->write<T>()`. These accept both raw pointers and `shared_ptr`
- **StateWriter fires on_changed** when it destructs, so use RAII scoping
- **Bindings** must be kept alive; when the `Binding` wrapper destructs, the binding is NOT automatically removed. Call `binding.remove()` to uninstall.
- **ReturnValue chaining**: use `rv &= operation()` to chain operations; the first failure is preserved. All operations still execute (no short-circuiting)

## Plugin System

A plugin is a shared library (`.dll`/`.so`) that registers types with Velk and optionally hooks into the update loop.

### Defining a plugin

Derive from `ext::Plugin<T>` (CRTP) and declare metadata with macros:

| Macro | Purpose | Required |
|-------|---------|----------|
| `VELK_PLUGIN_UID("uuid")` | Stable plugin UID (alias for `VELK_CLASS_UID`) | Yes |
| `VELK_PLUGIN_NAME("name")` | Human-readable name (defaults to class name) | No |
| `VELK_PLUGIN_VERSION(maj, min, patch)` | Semantic version | No |
| `VELK_PLUGIN_DEPS(...)` | List of `PluginDependency` (uid, optional min version) | No |

Override `initialize(IVelk&, PluginConfig&)` to register types and `shutdown(IVelk&)` to clean up. Use `::velk::register_type<T>(velk)` (the free function taking `IVelk&`) inside initialize.

### Entry point

`VELK_PLUGIN(ClassName)` exports the `velk_plugin_info()` C entry point. **Must be placed outside any namespace.**

### PluginConfig

Set fields on the `PluginConfig&` passed to `initialize()`:
- `retainTypesOnUnload` (default `false`): if true, types registered by this plugin survive unload
- `enableUpdate` (default `false`): if true, `pre_update()` / `post_update()` are called during `instance().update()`

### Update hooks

Override `pre_update(const IPlugin::PreUpdateInfo&)` and/or `post_update(const IPlugin::PostUpdateInfo&)`. Only called when `config.enableUpdate = true`.

`PostUpdateInfo` includes `tasksRun` and `propertiesChanged` counts from the update cycle.

### Loading plugins

```cpp
auto& reg = instance().plugin_registry();
reg.load_plugin_from_path("my_plugin.dll");   // by path
reg.load_plugin(pluginUid);                   // by registered UID
reg.load_plugin(pluginPtr);                   // by IPlugin::Ptr

auto p = reg.find_plugin(pluginUid);         // IPlugin::Ptr, null if not loaded
auto p = reg.get_or_load_plugin(pluginUid);  // IPlugin::Ptr, lazy load

// Typed lazy load (casts to custom interface)
auto p = get_or_load_plugin<IMyPluginInterface>(PluginId::MyPlugin); // IMyPluginInterface::Ptr
```

### Public header pattern

Expose `ClassId` / `PluginId` constants so consumers can create objects and load plugins without depending on implementation headers:

```cpp
namespace MyLib {
namespace ClassId {
inline constexpr velk::Uid Widget{"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"};
}
namespace PluginId {
inline constexpr velk::Uid MyPlugin{"yyyyyyyy-yyyy-yyyy-yyyy-yyyyyyyyyyyy"};
}
}
```

### CMake setup

```cmake
add_library(my_plugin SHARED ...)
target_link_libraries(my_plugin PRIVATE velk)
target_compile_definitions(my_plugin PRIVATE VELK_EXPORTS)
```

### Key rules

- **`VELK_PLUGIN_UID`** is required so the plugin UID is stable across builds
- **`VELK_CLASS_UID`** on implementation classes so their UIDs match the public `ClassId` constants
- **`VELK_PLUGIN()` must be outside any namespace** (it defines an `extern "C"` function)
- Types registered during `initialize()` are auto-swept on unload unless `retainTypesOnUnload` is set
- Dependencies are enforced at load time; a plugin cannot load if its deps are missing or too old

## Importer Extension System

The importer plugin (`velk_importer`) loads JSON scene files. Other plugins can handle custom top-level JSON keys by implementing `IImporterExtension`.

### Core interfaces (in `velk/include/velk/interface/intf_importer_extension.h`)

| Interface | Purpose |
|-----------|---------|
| `IStoreImporter` | Base interface for importers. `import_from(source) -> ImportResult`. Create via `instance().create<IStoreImporter>(ClassId::JsonImporter)` |
| `IImporterPlugin` | Plugin interface. `register_class_alias()` for global alias config, `resolve_class()` for name lookup |
| `IImportData` | Format-neutral read-only data tree. Null object pattern: `find()`/`at()` return a null node (never null pointer). All pure virtual, implementations use `ext::InterfaceDispatch<IImportData>` |
| `IImportResolver` | Resolves paths to objects/properties. `resolve("w1.width")` returns `IObject::Ptr` (cast to `IProperty` via `interface_pointer_cast`). Supports direct ids, hierarchy paths (`/scene/root/child`), property paths (`w1.width`) |
| `IImporterExtension` | Extension point. `collection_key()` returns the JSON key, `process(data, store, resolver)` handles the data |

### Writing an extension

```cpp
class MyHandler : public ext::ObjectCore<MyHandler, IImporterExtension>
{
public:
    VELK_CLASS_UID("...", "MyHandler");
    string_view collection_key() const override { return "my_data"; }
    void process(const IImportData& data, IStore& store,
                 const IImportResolver& resolver) const override
    {
        for (size_t i = 0; i < data.count(); i++) {
            auto& entry = data.at(i);
            auto target = entry.find("target").as_string();
            auto prop = interface_pointer_cast<IProperty>(resolver.resolve(target));
            // ...
        }
    }
};
```

Register in plugin `initialize()`: `::velk::register_type<MyHandler>(velk);`

The importer discovers extensions via `for_each_class` at import time. No coupling between plugins needed.

### JSON format summary

```json
{
    "version": 1,
    "objects": [
        { "id": "w1", "class": "plugin.ClassName", "properties": { "width": 100.0 } }
    ],
    "hierarchies": {
        "scene": { "root": ["child_a", "child_b"] }
    },
    "bindings": [
        { "source": "w1.width", "targets": ["w2.width"] }
    ]
}
```

Property values: bare value (`100.0`), object form (`{ "value": 100.0 }`), object ref (`{ "ref": "w1" }`), inline binding (`{ "bind": "w1.width" }`).

`"ref"` is for ObjectRef properties only. `"bind"` creates a one-way binding (equivalent to a top-level binding with one target).

### Built-in extension: animator

Handles `"animations"` key with `"type": "transition"` and `"type": "track"`:

```json
"animations": [
    { "type": "transition", "targets": ["w1.width"], "duration": 0.3, "easing": "out_cubic" },
    { "type": "track", "targets": ["w1.opacity"],
      "keyframes": [{ "time": 0.0, "value": 0.0 }, { "time": 1.0, "value": 1.0 }] }
]
```

## Detailed Examples

See [patterns.md](patterns.md) for complete code examples covering interface definition, object implementation, property access, functions, events, bindings, `Any<T>`, state access, and testing patterns.

## Distribution

This skill directory (`.claude/skills/velk/`) can be copied alongside Velk headers into consumer projects so that Claude Code provides correct guidance automatically.
