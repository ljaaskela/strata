---
name: velk
description: "Velk C++ component object model: VELK_INTERFACE macro, Object<T> CRTP, properties, events, functions, bindings, interface_cast, type registration, create objects, IMetadata, StateReader, StateWriter, plugins, ext::Plugin<T>, VELK_PLUGIN, IPluginRegistry, load_plugin_from_path"
---

# Velk Usage Guide

Velk is a C++17 component object model shipped as a shared DLL. Consumers depend only on public headers in `velk/include/velk/`. Objects are composed from pure virtual interfaces, implemented via CRTP, registered with a global type registry, and created through a factory. All values are type-erased through `IAny`.

## Object Lifecycle (5 steps)

1. **Define interface** with `VELK_INTERFACE` (generates static metadata, State struct, typed accessors)
2. **Implement** with `ext::Object<MyClass, IFace1, IFace2, ...>` (collects metadata from all interfaces)
3. **Register** with `instance().type_registry().register_type<MyClass>()`
4. **Create** with `instance().create<IObject>(MyClass::class_id())`
5. **Use** via `interface_cast<IMyInterface>(obj)->member()` or `velk::Object` wrapper. For state access prefer the free functions `read_state<T>(obj)` / `write_state<T>(obj)` (from `velk/api/state.h`)

## VELK_INTERFACE Member Types

| Macro | Syntax | Accessor returns | Notes |
|-------|--------|-----------------|-------|
| `PROP` | `(PROP, Type, name, default)` | `Property<Type>` | Read-write property |
| `RPROP` | `(RPROP, Type, name, default)` | `ConstProperty<Type>` | Read-only (write via state only) |
| `ARR` | `(ARR, ElemType, name, ...)` | `ArrayProperty<ElemType>` | Read-write array |
| `RARR` | `(RARR, ElemType, name, ...)` | `ConstArrayProperty<ElemType>` | Read-only array |
| `EVT` | `(EVT, name)` | `Event` | Observable event |
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
- **Events** conventionally use `on_` prefix: `(EVT, on_clicked)`
- **Deferred updates**: pass `Deferred` to `set_value()` or `write_state()`, then call `instance().update()` to flush
- **State access**: prefer `read_state<T>(obj)` / `write_state<T>(obj)` free functions over `meta->read<T>()` / `meta->write<T>()`. These accept both raw pointers and `shared_ptr`
- **StateWriter fires on_changed** when it destructs, so use RAII scoping
- **Bindings** must be kept alive; when the `Binding` wrapper destructs, the binding is NOT automatically removed. Call `binding.remove()` to uninstall.

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

auto* p = reg.find_plugin(pluginUid);         // nullptr if not loaded
auto* p = reg.get_or_load_plugin(pluginUid);  // lazy load

// Typed lazy load (casts to custom interface)
auto* p = get_or_load_plugin<IMyPluginInterface>(PluginId::MyPlugin);
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

## Detailed Examples

See [patterns.md](patterns.md) for complete code examples covering interface definition, object implementation, property access, functions, events, bindings, `Any<T>`, state access, and testing patterns.

## Distribution

This skill directory (`.claude/skills/velk/`) can be copied alongside Velk headers into consumer projects so that Claude Code provides correct guidance automatically.
