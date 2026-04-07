# Resources

Velk provides a URI-based resource system for loading files and other data. The resource store is a core service on `IVelk`, accessible via `instance().resource_store()`.

## Contents

- [Overview](#overview)
- [Reading a file](#reading-a-file)
- [Protocols](#protocols)
  - [Built-in protocols](#built-in-protocols)
  - [Registering scheme aliases](#registering-scheme-aliases)
  - [Custom protocols](#custom-protocols)
- [Decoders](#decoders)
  - [Decoder URIs](#decoder-uris)
  - [Writing a decoder](#writing-a-decoder)
  - [Dedup cache](#dedup-cache)
- [Persistence](#persistence)
- [API reference](#api-reference)
  - [IResourceStore](#iresourcestore)
  - [IResource](#iresource)
  - [IFile](#ifile)
  - [IResourceProtocol](#iresourceprotocol)
  - [IResourceProtocolInternal](#iresourceprotocolinternal)
  - [IResourceDecoder](#iresourcedecoder)

## Overview

Resources are accessed through URIs with an explicit scheme:

```
file://C:/data/config.json
app://assets/logo.png
```

The resource store parses the scheme, finds the matching protocol handler, and returns a typed resource handle. File resources implement `IFile`, which provides `read()` and `read_text()` methods.

```
URI  -->  IResourceStore  -->  IResourceProtocol  -->  IResource (IFile)
```

## Reading a file

```cpp
#include <velk/api/velk.h>
#include <velk/interface/resource/intf_resource.h>
#include <velk/interface/resource/intf_resource_store.h>

auto& store = velk::instance().resource_store();
auto file = store.get_resource<velk::IFile>("file://C:/data/config.json");

if (file) {
    velk::string content;
    if (velk::succeeded(file->read_text(content))) {
        // use content
    }
}
```

Binary reads work the same way:

```cpp
velk::vector<uint8_t> data;
if (velk::succeeded(file->read(data))) {
    // use data
}
```

You can also check existence and size without reading:

```cpp
auto res = store.get_resource("file://path/to/file.bin");
if (res && res->exists()) {
    int64_t bytes = res->size();
}
```

## Protocols

### Built-in protocols

Two protocols are registered automatically at startup:

| Scheme | Base path | Description |
|--------|-----------|-------------|
| `file://` | (none) | Absolute local filesystem paths |
| `app://` | Working directory | Paths relative to the process working directory |

```cpp
// Absolute path
store.get_resource<velk::IFile>("file://C:/data/test.json");

// Relative to working directory
store.get_resource<velk::IFile>("app://assets/logo.png");
```

The `FileProtocol` class (`ClassId::FileProtocol`) can be created via the type registry to serve as the handler for additional schemes.

### Registering scheme aliases

Create additional filesystem-backed schemes by instantiating `FileProtocol` with a different scheme and base path:

```cpp
auto fp = velk::instance().create<velk::IResourceProtocolInternal>(
    velk::ClassId::FileProtocol);
fp->set_scheme("assets");
fp->set_base_path("C:/my/project/assets/");
velk::instance().resource_store().register_protocol(
    velk::interface_pointer_cast<velk::IResourceProtocol>(fp));

// Now assets://logo.png resolves to C:/my/project/assets/logo.png
auto file = store.get_resource<velk::IFile>("assets://logo.png");
```

Multiple aliases can coexist:

```cpp
// app:// -> working directory
// assets:// -> assets subdirectory
// data:// -> user data directory
```

To remove a scheme (e.g. when a plugin unloads):

```cpp
auto proto = store.find_protocol("assets");
if (proto) {
    store.unregister_protocol(proto);
}
```

### Custom protocols

For non-filesystem resources (e.g. HTTP, in-memory buffers), implement `IResourceProtocol`:

```cpp
class HttpProtocol : public velk::ext::ObjectCore<HttpProtocol, velk::IResourceProtocol>
{
public:
    VELK_CLASS_UID("...", "HttpProtocol");

    velk::string_view scheme() const override { return "http"; }

    velk::IResource::Ptr resolve(velk::string_view path) const override
    {
        // Create and return a resource handle for the given path.
        // The resource type can be anything that implements IResource.
    }
};
```

Register the type and create an instance via the type registry:

```cpp
velk::instance().type_registry().register_type<HttpProtocol>();

auto proto = velk::instance().create<velk::IResourceProtocol>(HttpProtocol::class_uid);
velk::instance().resource_store().register_protocol(proto);
```

## Decoders

A decoder turns one resource into another. Where a protocol turns a URI into bytes (e.g. `file://`, `app://`), a decoder turns those bytes into a typed result (e.g. an `IImage` with a GPU texture).

Decoders are addressed in URIs with the form `name:inner_uri`, where `name` is the decoder's `name()` and `inner_uri` is a normal protocol URI:

```
image:app://logo.png
mesh:file://C:/models/cube.obj
```

The resource store recognises this form, resolves the inner URI through the normal protocol path, and runs the decoder on the result. Decoders are composable: a hypothetical `compressed:image:app://x.png.gz` would decode the inner expression first, then apply the outer decoder.

The disambiguation between decoder URIs and protocol URIs is unambiguous: if the part after the first `:` starts with `//`, it is a protocol URI (`file://...`); otherwise the leading token is a decoder name (`image:...`). They never collide.

### Decoder URIs

```cpp
auto img = store.get_resource<IImage>("image:app://logo.png");
```

A second call with the same URI returns the same `IImage::Ptr` until all consumers drop their references (see [Dedup cache](#dedup-cache)).

### Writing a decoder

Implement `IResourceDecoder` and register it with the store, typically in a plugin's `initialize()`:

```cpp
class ImageDecoder : public velk::ext::ObjectCore<ImageDecoder, velk::IResourceDecoder>
{
public:
    VELK_CLASS_UID("...", "ImageDecoder");

    velk::string_view name() const override { return "image"; }

    velk::IResource::Ptr decode(const velk::IResource::Ptr& inner) const override
    {
        auto* file = velk::interface_cast<velk::IFile>(inner);
        if (!file) return nullptr;
        velk::vector<uint8_t> bytes;
        if (!velk::succeeded(file->read(bytes))) return nullptr;
        // ... decode bytes into a typed Image, return as IResource::Ptr
    }
};

velk::instance().resource_store().register_decoder(
    velk::interface_pointer_cast<velk::IResourceDecoder>(decoder));
```

Return `nullptr` only when the input is fundamentally not of the expected type (wrong magic bytes, etc.). For partial decode failures, prefer returning a non-null resource that exposes a failure status of its own, so consumers can degrade gracefully and the failure stays cached (preventing N retries per frame from N failed consumers).

### Dedup cache

Decoded resources are deduplicated by full URI. The store maintains a weak-ref cache: a second call to `get_resource("image:app://logo.png")` returns the same `IResource::Ptr` while at least one consumer holds a reference. When the last reference is dropped, the cache slot becomes a dead `weak_ptr`. The next call reloads.

Only decoded results are cached. Raw protocol results (e.g. `IFile`) are not cached, so the resource store does not keep large source files in memory.

## Persistence

By default a decoded resource lives only as long as a consumer holds it. To keep a resource alive even when no external references exist, set the persistence flag:

```cpp
auto img = store.get_resource<IImage>("image:app://logo.png");
img->set_persistent(true);
```

The store then upgrades its cache slot to a strong reference and the resource survives across reference drops. To unpin:

```cpp
img->set_persistent(false);
```

The change takes effect on the next `get_resource` call: the store reconciles every cache entry against its current `is_persistent()` flag, so flipping the bit and then performing any store access drops the strong reference.

Persistence is meaningful only for decoded resources (the only ones that get cached). Setting it on a protocol-direct resource has no effect.

## Importer integration

The [importer plugin](plugins/importer.md) supports resource protocols and resource objects in the JSON format:

```json
{
    "resource-protocols": [
        { "scheme": "assets", "base_path": "./assets/" }
    ],
    "resources": [
        { "id": "main_font", "class": "velk-ui.Font", "properties": { "uri": "assets://default.ttf" } }
    ],
    "objects": [
        { "id": "label", "class": "velk-ui.Label", "properties": {
            "font": { "ref": "resources.main_font" }
        }}
    ]
}
```

See the [importer documentation](plugins/importer.md#resource-protocols) for details.

## API reference

### IResourceStore

Core service on `IVelk`. Header: `velk/interface/resource/intf_resource_store.h`

| Method | Description |
|--------|-------------|
| `get_resource(uri)` | Resolves a URI to an `IResource::Ptr`. Handles both protocol URIs (`scheme://path`) and decoder URIs (`name:inner_uri`). Returns nullptr if the scheme/decoder is unknown. |
| `get_resource<T>(uri)` | Resolves a URI and casts to `T::Ptr`. Returns nullptr if the scheme/decoder is unknown or the cast fails. |
| `register_protocol(protocol)` | Registers a protocol handler. Replaces any existing handler for the same scheme. |
| `unregister_protocol(protocol)` | Removes a previously registered protocol handler. |
| `find_protocol(scheme)` | Returns the protocol handler for a scheme, or nullptr. |
| `register_decoder(decoder)` | Registers a resource decoder. Replaces any existing decoder for the same name. |
| `unregister_decoder(decoder)` | Removes a previously registered decoder. |
| `find_decoder(name)` | Returns the decoder for a name, or nullptr. |

### IResource

Base interface for all resources. Header: `velk/interface/resource/intf_resource.h`

| Method | Description |
|--------|-------------|
| `uri()` | Returns the full URI of this resource. |
| `exists()` | Returns true if the resource exists. |
| `size()` | Returns the size in bytes, or -1 on failure. |
| `is_persistent()` | Returns whether this resource is pinned in the decoded-resource cache. |
| `set_persistent(value)` | Pins or unpins the resource in the cache. Takes effect on next `get_resource` call. |

### IFile

File resource with read access. Inherits `IResource`. Header: `velk/interface/resource/intf_resource.h`

| Method | Description |
|--------|-------------|
| `read(out)` | Reads the entire file as binary bytes into a `vector<uint8_t>`. |
| `read_text(out)` | Reads the entire file as UTF-8 text into a `string`. |

### IResourceProtocol

Protocol handler interface. Header: `velk/interface/resource/intf_resource_protocol.h`

| Method | Description |
|--------|-------------|
| `scheme()` | Returns the URI scheme this protocol handles (e.g. "file"). |
| `resolve(path)` | Resolves a path (scheme stripped) to an `IResource::Ptr`. |

### IResourceProtocolInternal

Configuration interface for protocols that support scheme and base path. Inherits `IResourceProtocol`. Header: `velk/interface/resource/intf_resource_protocol.h`

| Method | Description |
|--------|-------------|
| `set_scheme(scheme)` | Sets the URI scheme. Returns `ReturnValue`. |
| `set_base_path(base_path)` | Sets a base path prepended to all resolved paths. Returns `ReturnValue`. |

### IResourceDecoder

Decoder interface. Header: `velk/interface/resource/intf_resource_decoder.h`

| Method | Description |
|--------|-------------|
| `name()` | Returns the decoder name (e.g. `"image"`), used as the leading token in `name:inner_uri` URIs. |
| `decode(inner)` | Decodes the inner resource into a typed result. Return nullptr only when the input is fundamentally not of the expected type; for partial failures, return a non-null resource with its own status field. |
