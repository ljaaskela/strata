# Plugin System Design

Status: draft, not yet implemented.

## Goal

Add `IPluginRegistry` to `IVelk` so users can write plugins that register types into the type registry.

## Proposed Interfaces

### IPlugin (what plugin authors implement)

```cpp
class IPlugin : public Interface<IPlugin>
{
public:
    virtual ReturnValue initialize(ITypeRegistry& registry) = 0;
    virtual ReturnValue shutdown(ITypeRegistry& registry) = 0;
    virtual string_view name() const = 0;
    virtual Uid plugin_id() const = 0;
};
```

### IPluginRegistry (on IVelk, manages plugins)

```cpp
class IPluginRegistry : public Interface<IPluginRegistry>
{
public:
    virtual ReturnValue load_plugin(IPlugin& plugin) = 0;
    virtual ReturnValue unload_plugin(Uid pluginId) = 0;
    virtual IPlugin* get_plugin(Uid pluginId) const = 0;
    virtual array_view<IPlugin*> get_plugins() const = 0;
};
```

### IVelk addition

```cpp
virtual IPluginRegistry& plugin_registry() = 0;
```

## Plugin author usage

DLL-based:

```cpp
class MyPlugin : public ext::ObjectCore<MyPlugin, IPlugin>
{
public:
    ReturnValue initialize(ITypeRegistry& registry) override {
        registry.register_type<MyWidget>();
        return ReturnValue::SUCCESS;
    }
    ReturnValue shutdown(ITypeRegistry& registry) override {
        registry.unregister_type<MyWidget>();
        return ReturnValue::SUCCESS;
    }
    string_view name() const override { return "MyPlugin"; }
    Uid plugin_id() const override { return class_id(); }
};

extern "C" PLUGIN_EXPORT velk::IPlugin* velk_create_plugin() {
    static MyPlugin instance;
    return &instance;
}
```

Inline (no DLL):

```cpp
MyPlugin plugin;
instance().plugin_registry().load_plugin(plugin);
```

## Open design questions

1. **DLL loading**: Should `IPluginRegistry` handle `LoadLibrary`/`dlopen` (e.g. `load_plugin_from_path`), or leave that to the caller? Keeping DLL loading out keeps the interface simpler.

2. **Plugin dependencies**: Should `IPlugin` expose `array_view<Uid> dependencies()` for load ordering? Adds complexity, maybe not needed initially.

3. **Plugin lifetime vs type lifetime**: What happens to live instances when a plugin is unloaded?
   - Prevent unload while instances exist (ref-count)
   - Require caller to destroy instances first
   - Crash risk if DLL is unloaded with live vtable pointers

4. **Extension points beyond types**: Give plugin `IVelk&` instead of `ITypeRegistry&` for wider access? Or keep it narrow.

5. **Where IPlugin lives**: `velk/include/velk/interface/` (plugin authors need it). Implementation in `src/`.

6. **Static vs dynamic plugins**: Design supports both. Consider a `VELK_PLUGIN` macro for boilerplate reduction.

## Suggested starting scope

Minimal: `IPlugin`, `IPluginRegistry`, `IVelk::plugin_registry()`. No DLL loading, no dependency graph, no auto-discovery. Layer the rest on top later.
