# Tracy Plugin

The Tracy plugin (`velk_tracy.dll`) integrates [Tracy Profiler](https://github.com/wolfpld/tracy) into velk's performance logging system. When loaded, all `VELK_PERF_SCOPE` instrumentation automatically appears as Tracy zones with source locations and color-coded labels.

## Usage

Load the plugin at startup:

```cpp
velk::instance().plugin_registry().load_plugin_from_path("velk_tracy.dll");
```

Then connect with the [Tracy Profiler](https://github.com/wolfpld/tracy/releases) GUI. Zones, frame marks, and source locations appear automatically.

No code changes are needed beyond loading the plugin. If you don't want profiling, simply don't load it.

## How it works

The plugin registers an `IPerfSink` that translates velk's perf API into Tracy's C API:

| Velk | Tracy |
|------|-------|
| `IPerfSink::start_perf` | `___tracy_alloc_srcloc_name` + `___tracy_emit_zone_begin_alloc` |
| `IPerfSink::end_perf` | `___tracy_emit_zone_end` |
| `IPerfSink::event(Present)` | `___tracy_emit_frame_mark(nullptr)` (main frame) |
| `IPerfSink::event(Update/Render)` | `___tracy_emit_frame_mark("Update"/"Render")` (named frames) |

## Zone colors

Zones are automatically colored by label prefix:

| Prefix | Color |
|--------|-------|
| `renderer.` | Orange |
| `vk.` | Red |
| `scene.` | Blue |
| `layout.` | Green |
| `text.` | Purple |
| `image.` | Yellow |
| `app.` | Teal |

## Build

The plugin is built by default when `VELK_ENABLE_TRACY` is `ON` (the default). Tracy v0.13.1 client sources are vendored as a tarball in `third_party/`.

## MSVC 2019 notes

Two workarounds are applied for MSVC 2019 compatibility:

- `TRACY_NO_SYSTEM_TRACING` disables ETW tracing (requires a newer Windows SDK)
- `RelationProcessorDie` is defined as a cast to `LOGICAL_PROCESSOR_RELATIONSHIP(5)` (missing from older SDK headers)
