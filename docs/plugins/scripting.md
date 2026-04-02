# Scripting plugin

The scripting plugin (`velk_js`) adds JavaScript scripting via [QuickJS-ng](https://quickjs-ng.github.io/quickjs/). It enables expression bindings and event handlers declared in a [JSON file](importer.md).

## Contents

- [Loading the plugin](#loading-the-plugin)
- [JSON format](#json-format)
  - [Expression bindings](#expression-bindings)
  - [Event handlers](#event-handlers)
  - [Multi-line handlers](#multi-line-handlers)
- [How it works](#how-it-works)
  - [Object access](#object-access)
  - [Type conversion](#type-conversion)
  - [Lifecycle](#lifecycle)

## Loading the plugin

```cpp
#include <velk/plugins/script/js/plugin.h>

// Load (requires the importer plugin to be loaded first)
instance().plugin_registry().load_plugin_from_path("velk_js.dll");
```

The plugin registers a `JsImportHandler` that handles the `"scripts"` top-level collection in JSON store files. It must be loaded before loading a JSON file for the scripts to be processed.

## JSON format

Scripts are declared in the `"scripts"` top-level array. Each entry is either an expression binding or an event handler.

### Expression bindings

An expression binding evaluates a JavaScript expression and binds the result to a target property. The expression is re-evaluated whenever any property it reads changes.

```json
{
    "version": 1,
    "objects": [
        { "id": "parent", "name": "parent", "class": "my.Panel", "properties": { "width": 800 } },
        { "id": "child", "name": "child", "class": "my.Panel" }
    ],
    "scripts": [
        { "target": "child.width", "expr": "parent.width * 0.5" },
        { "target": "child.visible", "expr": "parent.width > 100" }
    ]
}
```

The expression is wrapped in a function and compiled to bytecode at import time. It runs as a Velk function binding with auto-tracked dependencies: any property read during evaluation becomes a dependency, and the binding re-evaluates when those properties change.

The result is type-converted to match the target property automatically (e.g. a JS number becomes `float`, `int32_t`, `double`, etc. depending on the target's type).

### Event handlers

Named events declared with `EVT` in `VELK_INTERFACE` are can be tied to a JavaScript statement. Use `object.event_name` to react to such events.

```json
{
    "scripts": [
        { "event": "btn.on_clicked", "handler": "status.clicked = true" }
    ]
}
```
Event handlers are invoked as deferred tasks, executing during the next `instance().update()` call.

#### Property value change

A special handler is defined for properties. The `object.property.on_changed` pattern resolves the named property and subscribes to its `on_changed()` event. 

```json
{
    "scripts": [
        { "event": "btn.text.on_changed", "handler": "status.count = 42" }
    ]
}
```
### Multi-line handlers

Handler source can be an array of strings, joined with newlines before compilation:

```json
{
    "scripts": [
        {
            "event": "btn.width.on_changed",
            "handler": [
                "let w = btn.width;",
                "status.count = w * 10;"
            ]
        }
    ]
}
```

## How it works

### Object access

Store objects are accessible in JavaScript by their store id. When the `"scripts"` collection is processed, the import handler scans script entries for object references and registers them as global variables in the JS context.

```json
{ "id": "panel", "name": "panel", "class": "my.Panel" }
```

In a script expression, `panel.width` reads the `width` property from the object with id `"panel"`. Writing `panel.width = 100` calls `set_value()` on the property, which fires `on_changed` if the value differs.

Property access goes through the Velk property system (`IMetadata::get_property` / `IProperty::get_value` / `set_value`), so bindings, extensions, and change events all work as expected.

### Type conversion

The plugin uses a custom `IAny` implementation (`JsAny`) that holds a JS value and supports on-demand conversion to any Velk numeric type, `bool`, or `string`. When the binding system reads from a `JsAny`, it requests the target type and receives a properly converted value.

| JS type | Velk types |
|---|---|
| Number | `float`, `double`, `int8_t`..`int64_t`, `uint8_t`..`uint64_t` |
| Boolean | `bool` |
| String | `velk::string` |

### Lifecycle

The plugin manages one `JSRuntime` and one `JSContext` per imported store. All JS objects created during evaluation (expression results, proxy objects) are tracked via weak references. When the plugin is shut down or the store context is destroyed, all tracked JS values are invalidated before the runtime is freed.

Event handlers and expression bindings hold references to compiled JS bytecode. These are also tracked and invalidated on teardown, so handlers that fire after teardown safely return without executing.
