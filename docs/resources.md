# Resources

Velk provides a URI-based resource system for loading files and other data. The resource store is a core service on `IVelk`, accessible via `instance().resource_store()`.

## Contents

- [Overview](#overview)
- [Reading a file](#reading-a-file)
- [Protocols](#protocols)
  - [Built-in protocols](#built-in-protocols)
  - [Registering scheme aliases](#registering-scheme-aliases)
  - [Custom protocols](#custom-protocols)
- [API reference](#api-reference)
  - [IResourceStore](#iresourcestore)
  - [IResource](#iresource)
  - [IFile](#ifile)
  - [IResourceProtocol](#iresourceprotocol)
  - [IResourceProtocolInternal](#iresourceprotocolinternal)

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

## API reference

### IResourceStore

Core service on `IVelk`. Header: `velk/interface/resource/intf_resource_store.h`

| Method | Description |
|--------|-------------|
| `get_resource(uri)` | Resolves a URI to an `IResource::Ptr`. Returns nullptr if the scheme is unknown. |
| `get_resource<T>(uri)` | Resolves a URI and casts to `T::Ptr`. Returns nullptr if the scheme is unknown or the cast fails. |
| `register_protocol(protocol)` | Registers a protocol handler. Replaces any existing handler for the same scheme. |
| `unregister_protocol(protocol)` | Removes a previously registered protocol handler. |
| `find_protocol(scheme)` | Returns the protocol handler for a scheme, or nullptr. |

### IResource

Base interface for all resources. Header: `velk/interface/resource/intf_resource.h`

| Method | Description |
|--------|-------------|
| `uri()` | Returns the full URI of this resource. |
| `exists()` | Returns true if the resource exists. |
| `size()` | Returns the size in bytes, or -1 on failure. |

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
