# Performance

This document covers runtime performance and memory usage related topics.

## Contents

- [Design philosophy](#design-philosophy)
  - [Lazy metadata creation](#lazy-metadata-creation)
  - [Lazy change events](#lazy-change-events)
  - [Extension chain](#extension-chain)
  - [Direct state access](#direct-state-access-1)
  - [Static metadata](#static-metadata)
  - [Control block pooling](#control-block-pooling)
  - [Compile-time interface_cast](#compile-time-interface_cast)
  - [No RTTI, no exceptions](#no-rtti-no-exceptions)
  - [Hive slot reuse](#hive-slot-reuse)
- [Operation costs](#operation-costs)
  - [Property get/set](#property-getset)
  - [Variant property get/set](#variant-property-getset)
  - [Direct state access](#direct-state-access)
  - [Function invoke](#function-invoke)
  - [Event dispatch](#event-dispatch)
  - [interface_cast](#interface_cast)
  - [Metadata lookup](#metadata-lookup)
  - [Object creation](#object-creation)
- [Hierarchy](#hierarchy)
  - [Queries](#queries)
  - [Mutation](#mutation)
  - [Tree construction](#tree-construction)
  - [Event overhead](#event-overhead)
- [Memory layout](#memory-layout)
  - [Example: Minimal object with 1 member](#example-minimal-object-with-1-member)
  - [Example: MyWidget with 6 members](#example-mywidget-with-6-members)
  - [Layout notes](#layout-notes)
  - [Common base layers](#common-base-layers)
  - [Base types](#base-types)

## Design philosophy

Velk follows a "pay for what you use" principle. Every abstraction layer is designed so that unused features impose zero runtime or memory cost. The techniques below work together to keep objects lightweight in the common case while still offering rich functionality when needed.

### Lazy metadata creation

`ObjectStorage` is not allocated until the first runtime metadata access (e.g. `get_property()`, `get_event()`, `get_function()`). Until then, the object carries only a null pointer. Once the container exists, individual member instances (`ClassId::Property`, `ClassId::Function`, `ClassId::Event`) are created on demand by `find_or_create()`, which checks a cache of already-created instances before scanning the static metadata array. An object that never touches its metadata at runtime pays nothing beyond the object itself.

### Lazy change events

Property change events are owned by `ObjectStorage`, not by each `ClassId::Property` instance. `ObjectStorage` lazily creates a `ClassId::Event` per property on first access (`get_property_event` with `Resolve::Create`). Properties that are never observed pay no event overhead. Firing a change (`invoke_property_changed`) uses `Resolve::Existing` so no event is created if nobody has subscribed.

### Extension chain

`IAnyExtension` is a chainable wrapper that sits in a property's `data_` slot, intercepting `get_data` / `set_data` calls. Each extension wraps an inner `IAny` (which may itself be another extension), forming a chain. Extensions are installed via `install_extension()`, which splices the new extension in front of the existing backing store. When no extensions are installed, the property reads and writes its `AnyRef` directly with no indirection.

### Direct state access

`get_property_state<T>(object)` returns a typed pointer to the interface's `State` struct, which is stored inline in the object. Reading and writing fields through this pointer is a plain dereference with no virtual dispatch, no `IAny` layer, and no change-event firing. This is the fastest path (~1 ns read, <1 ns write) and is appropriate when the caller owns the object and does not need property-system features like change notification or extension interception.

### Static metadata

Interface metadata (`MemberDesc` arrays, `InterfaceInfo` lists) is generated at compile time by the `VELK_INTERFACE` macro and stored as `static constexpr` data. `ext::Object<T, Interfaces...>` concatenates metadata from all interfaces into a single `constexpr` array via `CollectedMetadata<Interfaces...>`. These arrays are shared across all instances of a type at zero per-object cost.

### Control block pooling

Every `shared_ptr`-managed object needs a control block for its reference counts. Velk maintains a thread-local free-list of recycled `control_block` and `external_control_block` instances, avoiding the global allocator on the create/destroy hot path. Pooled allocation is ~2.5x faster than `new`/`delete` in benchmarks. The pool uses Fiber Local Storage (FLS) on Windows and `pthread_key` on POSIX, with automatic cleanup on thread exit. Pooling can be disabled at build time with `-DVELK_ENABLE_BLOCK_POOL=OFF`.

### Compile-time interface_cast

`interface_cast<T>(obj)` uses `std::is_base_of_v<T, U>` at compile time. When the target type `T` is a known base of the source type `U`, the cast compiles down to a plain pointer return with no virtual dispatch. The runtime path (linear scan of the interface pack + parent chains) is only taken when the relationship cannot be determined statically.

### No RTTI, no exceptions

The library itself is compiled with RTTI and C++ exceptions disabled (`/GR- /EHs-c-` on MSVC, `-fno-rtti -fno-exceptions` on other compilers). Type identity uses compile-time FNV-1a hashing of type names instead of `typeid`. Error conditions are reported through return values (`nullptr`, `false`) rather than exceptions. This reduces binary size and avoids the overhead of exception-handling tables.

### Hive slot reuse

`Hive<T>` (the pool allocator used for `ObjectStorage` and other internal types) pre-allocates pages of fixed-size slots. Each page maintains an intrusive LIFO free-list threaded through the slot memory itself. Allocating a slot pops from the head; deallocating pushes back. After the initial page allocation, steady-state add/remove cycles never touch the heap. Active slots are tracked with a bitset for iteration.

## Operation costs

| Operation | Cost | Measured | Notes |
|---|---|---|---|
| **Property get** | 1 virtual call + `memcpy` | ~37 ns | Via `Property<T>` wrapper; queries `IPropertyInternal`, then `IAny::get_data` |
| **Property set** | 1 virtual call + `memcpy` | ~22 ns | Reverse path through `IAny::set_data`; fires `on_changed` if value differs |
| **Variant get** | Property lookup + delegate | ~20 ns | Same-type returns IAny pointer directly; conversion path scans type table (~44 ns) |
| **Variant set** | `copy_from` + delegate | ~43 ns | Same-type delegates to inner IAny; type-change allocates new inner (~103 ns) |
| **Variant state read/write** | Direct `get_data`/`set_data` | ~5/11 ns | `Variant::get<T>()`/`set<T>()` bypass `ClassId::Property`, call `ClassId::Variant` directly |
| **Direct state read** | Pointer dereference | ~1 ns | `IPropertyState::get_property_state<T>()` returns `T::State*`; read fields directly |
| **Direct state write** | Pointer dereference | <1 ns | Write fields via state pointer; no virtual dispatch |
| **Function invoke** | 1 indirect call | ~13 ns | `target_fn_(target_context_, args)`, context/function-pointer pair, no virtual dispatch |
| **Typed-arg trampoline** | Arg extraction + indirect call | ~42 ns | `FnBind` reads each arg via `IAny::get_data()`, then calls the virtual `fn_Name(...)` |
| **Raw function invoke** | 1 indirect call | ~14 ns | `FnRawBind` passes `FnArgs` through unchanged, no extraction overhead |
| **Event dispatch (immediate)** | Snapshot + loop | ~36 ns | Snapshots handler list, then iterates; safe against handler mutation during dispatch |
| **Event dispatch (deferred)** | Clone + queue | ~144 ns | Clones args once into `shared_ptr`, queues `DeferredTask`; mutex lock on insertion |
| **interface_cast** | Linear scan | ~5 ns | Walks the interface pack + parent chains; typically 2-4 interfaces, fully inlinable. When `T` is a base of the source type, resolves at compile time via `is_base_of` with no virtual dispatch |
| **Metadata lookup (cold)** | Linear scan + alloc | ~563 ns | First `get_property()` call; allocates `ClassId::Property` and caches result |
| **Metadata lookup (cached)** | Cache-first scan | ~32 ns | Subsequent call; scans cached instances first, no allocation |
| **Object creation** | placement-new into hive page | ~55 ns | Factory lookup (`O(log N)`), then hive allocation (default for types < 1 KB via `CreationPolicy::Auto`); no per-object heap alloc after initial page |

*Measured on AMD Ryzen 7 5800X (3.8 GHz), MSVC 19.29, Release build. Run `build/bin/Release/benchmarks.exe` to reproduce.*

### Property get/set

`Property<T>::get_value()` queries `IPropertyInternal` on the property, gets the backing `IAny`, and calls `get_data(&value, sizeof(T), typeUid)` which copies data to a stack-local variable. `set_value()` follows the reverse path and fires the `on_changed` event if the value changed.

The backing `IAny` is typically an `AnyRef<T>`, a non-owning pointer into the object's inline `State` struct. For trivially-copyable types, `AnyRef<T>::set_value()` uses `memcmp` + `memcpy`. For non-trivial types, it uses direct assignment.

### Variant property get/set

A variant property uses the same `ClassId::Property` path as a typed property, but the backing `IAny` is a `ClassId::Variant` that holds an inner typed `IAny` (e.g. `AnyValue<float>`) and delegates to it. 
* Same-type gets (~20 ns) return the `IAny` pointer directly without calling `get_data`, while conversion gets (~44 ns) scan a static 42-entry conversion table with no heap allocation.
* Same-type sets (~43 ns) delegate to the inner `IAny`'s `copy_from`, type-change sets (~103 ns) allocate a new inner `IAny`, which dominates the cost. For hot paths, `Variant::get<T>()`/`set<T>()` bypass `ClassId::Property` entirely and call `ClassId::Variant` directly (~5/11 ns).

Per-instance memory overhead of a Variant is one `IAny::Ptr` (16 bytes) inside `ClassId::Variant` plus one `IVariant::Ptr` (16 bytes) in the State struct; compatible-type information is a single static array shared across all instances.

### Direct state access

Bypasses the property system entirely. `IPropertyState::get_property_state<T>()` returns a pointer to the interface's `State` struct stored inline in the object. Reading and writing fields is a plain pointer dereference with zero abstraction overhead.

For trivially-copyable state structs, the entire state can be snapshotted or restored with `memcpy`.

### Function invoke

`ClassId::Function` stores a `target_fn_` / `target_context_` pair. Invocation is a single indirect call: `target_fn_(target_context_, args)`. For `VELK_INTERFACE` functions, the context is a pointer to the owning object and `target_fn_` is a static trampoline generated by `FnBind` or `FnRawBind`.

- **Zero-arg / typed-arg (`FN`)**: The `FnBind` trampoline extracts typed values from `FnArgs` via `IAny::get_data()` (one per argument), then calls the virtual `fn_Name(...)`.
- **Raw (`FN_RAW`)**: The `FnRawBind` trampoline passes `FnArgs` through unchanged, no extraction overhead.
- **Explicit callback**: `set_invoke_callback()` stores a `CallbackFn*` with a static trampoline that does one `reinterpret_cast` + call.

### Event dispatch

Handlers are stored in a single `std::vector` partitioned by invoke type: `[0, deferred_begin_)` for immediate, `[deferred_begin_, size())` for deferred.

- **Immediate handlers**: The handler list is snapshotted before iteration so that handlers can safely add/remove handlers on the same event during dispatch. The snapshot copies the `shared_ptr` vector, adding a ref-count bump per handler.
- **Deferred handlers**: Args are cloned once into a `shared_ptr` (shared across all deferred handlers for that invocation), then queued as `DeferredTask` entries. Queue insertion takes a mutex lock. `instance().update()` swaps the queue under the lock and executes outside it, no nested locking.
- **No handlers**: The handlers vector is empty, zero heap allocation.
- **add_handler()**: Linear dedup scan before insertion, `O(H)` where H is handler count.

### interface_cast

`InterfaceDispatch::get_interface(uid)` walks the compile-time interface pack. For each interface, it compares the UID, then walks the parent interface chain (`ParentInterface` typedef) until reaching `IInterface`. Returns the first match or `nullptr`.

Complexity is `O(N + P)` where N is the number of interfaces in the pack (typically 2-4) and P is the maximum inheritance depth (typically 1-2). The template recursion is fully inlined by the compiler.

### Metadata lookup

`ObjectStorage::find_or_create(name, kind)` checks the `instances_` cache first, a linear scan of `O(M)` already-created members comparing by name and kind. On a cache hit this is the only work done, avoiding the full `members_` scan. On a cache miss, it scans the static `members_` array to find the member, allocates a new `ClassId::Property` or `ClassId::Function`, wires up the virtual dispatch trampoline, and caches the result.

Subsequent accesses for the same member skip creation and only pay the cache lookup cost. Since applications typically access a subset of declared members, the cache-first scan is shorter than the full members array. Static metadata arrays (`MemberDesc`, `InterfaceInfo`) are `constexpr`, shared across all instances at zero per-object cost.

### Object creation

`instance().create()` defaults to hive-backed allocation for most types. The ~55 ns benchmark number measures this hive path.

**Steps (hive path, default):**

1. **Factory lookup**: `O(log N)` binary search on sorted registered types vector
2. **Policy check**: `CreationPolicy::Auto` is resolved at registration time to `Hive` for types < 1 KB, `Alloc` for larger types
3. **Hive allocation**: `ensure_hive()` lazily creates one `ObjectHive` per type on first use (cached in the registry entry). `allocate()` does placement-new into a pre-allocated page slot. The returned object is a "born zombie": it lives in a hive slot but is not active (not iterable via `for_each()`), and the returned `shared_ptr` is the sole owner. No per-object heap allocation after the initial page is allocated.
4. **Wire self-pointer**: Stores `IObject*` in `control_block::ptr` (for `shared_from_object()`; reconstructs `shared_ptr` on demand)
5. **State initialization**: `State` structs are default-constructed inline (part of the object allocation, not separate)

**Heap fallback:**

When `resolved_policy` is `Alloc` (types >= 1 KB, or explicitly requested), the factory's `create_instance()` does `new FinalClass` with a pool-recycled control block.

**CreationPolicy override:** The policy can be set per-type via `TypeOptions` at registration time:

```cpp
instance().type_registry().register_type<MyType>({CreationPolicy::Alloc});
instance().type_registry().register_type<MyType>({CreationPolicy::Hive});
instance().type_registry().register_type<MyType>({CreationPolicy::Auto}); // default
```

**allocate() vs add():** TypeRegistry's `create()` uses `ObjectHive::allocate()`, which produces a born zombie (external ownership only, not iterable). `ObjectHive::add()` creates an active slot where the hive holds a strong reference and the object is iterable via `for_each()`. Use `add()` when the hive should own and enumerate its objects; `allocate()` when the caller manages lifetime independently.

No member instances (`ClassId::Property`, `ClassId::Function`) are created until first access.

## Hierarchy

Hierarchy operations are measured on balanced binary trees of `BenchWidget` objects. Queries and mutations go through the `IHierarchy` interface; the `Hierarchy` and `Node` API wrappers add minimal overhead on top.

| Operation | Cost | Measured | Notes |
|---|---|---|---|
| **parent_of** | Hash lookup | ~78 ns | Returns parent of a leaf in a 1024-node tree |
| **contains** | Hash lookup | ~14 ns | Checks membership of a known leaf |
| **children_of** (512 children) | Copy vector | ~30 µs | Returns `vector<IObject::Ptr>`; cost dominated by ref-count bumps |
| **for_each_child** (512 children) | Iterate + cast | ~5.4 µs | Visits each child via `interface_cast<IObject>` callback |
| **add + remove** (leaf) | 2 mutations | ~408 ns | Steady-state add then remove on a 256-node tree |
| **replace** | In-place swap | ~507 ns | Replaces a node, reparents children |
| **set_root** | 1 mutation | ~381 ns | Sets root on an empty hierarchy (includes object creation) |
| **Build tree/64** | 64 add ops | ~17 µs | Balanced binary tree construction |
| **Build tree/512** | 512 add ops | ~135 µs | |
| **Build tree/1024** | 1024 add ops | ~275 µs | ~268 ns per node amortized |

### Queries

`parent_of()` and `contains()` are hash-map lookups on the internal node table. `children_of()` returns a copy of the children vector, so its cost scales linearly with child count due to `shared_ptr` reference counting. `for_each_child()` avoids the vector copy by iterating in-place with a visitor callback, making it ~5x faster than `children_of()` for the same 512 children.

### Mutation

`add()` and `remove()` insert/erase from the internal node map and update parent/children links. `replace()` does the same plus reparents the old node's children to the new node. All mutations fire `on_changing` / `on_changed` events when handlers are registered.

### Tree construction

Building a balanced binary tree scales linearly. At 1024 nodes the amortized cost is ~268 ns per node, which includes object creation (~55 ns) plus the `add()` operation.

### Event overhead

| Scenario | Measured | Notes |
|---|---|---|
| add + remove, no handler | ~407 ns | Baseline: no event listeners |
| add + remove, with handler | ~646 ns | One `on_changed` handler subscribed |

Subscribing an `on_changed` handler adds ~59% overhead to mutation operations. With no handlers registered, the event system imposes no cost beyond checking for an empty handler list.

## Memory layout

An `ext::Object<T, Interfaces...>` instance carries minimal per-object data. The ObjectStorage is heap-allocated once per object and lazily creates member instances on first access.

### Example: Minimal object with 1 member

A minimal object implements a single interface with one property. `ext::Object` adds `IObjectStorage`, giving 2 interfaces in the dispatch pack (IObjectStorage, IToggle). IObject is not prepended because it is reachable via IObjectStorage's parent chain (IObjectStorage → IMetadata → IPropertyState → IObject). The ObjectStorage is allocated lazily on first runtime metadata or attachment access.

```
Toggle (48 bytes)                           ObjectStorage (104 bytes, heap, lazy)
┌──────────────────────────────────┐      ┌──────────────────────────────────┐
│ MI base layout               16  │      │ base (InterfaceDispatch)      8  │
│   (2 vptrs)                      │      │ members_ (array_view)        16  │
│ flags + padding               8  │      │ owner_ (pointer)              8  │
│ block*                        8  │      │ attachments_ (vector)        24  │
│ storage_ (pointer)            8  │      │ metadata_ (vector)           24  │
│ IToggle::State                8  │      │ observers_ (vector)          24  │
│   (enabled: bool + padding)      │      └──────────────────────────────────┘
└──────────────────────────────────┘
```

With no members accessed, the ObjectStorage is not allocated. The total footprint is **48 bytes** (object only). On first runtime metadata access the container is lazily allocated (104 bytes). Accessing the one property then adds a 48-byte `MemberSlot` to `metadata_`, bringing the total to **208 bytes**.

### Example: MyWidget with 6 members

MyWidget implements IMyWidget (2 PROP + 1 EVT + 1 FN) and ISerializable (1 PROP + 1 FN). `ext::Object` adds IObjectStorage, totaling 3 interfaces in the dispatch pack (IObjectStorage, IMyWidget, ISerializable). IObject is not prepended because it is reachable via IObjectStorage's parent chain. The ObjectStorage is allocated lazily on first runtime metadata or attachment access.

```
MyWidget (80 bytes)                         ObjectStorage (104 bytes, heap, lazy)
┌──────────────────────────────────┐      ┌──────────────────────────────────┐
│ MI base layout               24  │      │ base (InterfaceDispatch)      8  │
│   (3 vptrs)                      │      │ members_ (array_view)        16  │
│ flags + padding               8  │      │ owner_ (pointer)              8  │
│ block*                        8  │      │ attachments_ (vector)        24  │
│ storage_ (pointer)            8  │      │ metadata_ (vector)           24  │
│ IMyWidget::State              8  │      │ observers_ (vector)          24  │
│   (width, height: 2× float)      │      └──────────────────────────────────┘
│ ISerializable::State         24  │
│   (name: velk::String)           │
└──────────────────────────────────┘
```

### Layout notes

The MI base layout contains one vtable pointer per interface chain, plus MSVC multiple-inheritance adjustment padding. The exact layout is compiler-specific; sizes are derived from `sizeof(ObjectCore<...>)` minus non-MI fields (ObjectData + meta_). The self-pointer (`IObject*`) is stored in `control_block::ptr` rather than inline, so it costs no per-object space beyond the already-allocated block.

Member instances are created lazily, only when first accessed via `get_property()`, `get_event()`, or `get_function()`. Each accessed member adds a **48-byte** `MemberSlot` to the `metadata_` vector: a 16-byte `shared_ptr<IInterface>` (the instance) and a 32-byte `MemberData` union (property data+event or function callback). The member index is stored on the instance itself (via `IStorageOwned::get_member_index()`), keeping the slot compact. The `MemberData` union avoids storing per-member data on the instances themselves, keeping Property/Function/Event objects thin.

| Scenario | Object | ObjectStorage | MemberSlots | Total |
|---|---|---|---|---|
| Toggle, no members accessed | 48 | 0 (lazy) | 0 | **48 bytes** |
| Toggle, 1 member accessed | 48 | 104 | 1 × 48 = 48 | **200 bytes** |
| MyWidget, no members accessed | 80 | 0 (lazy) | 0 | **80 bytes** |
| MyWidget, 3 members accessed | 80 | 104 | 3 × 48 = 144 | **328 bytes** |
| MyWidget, all 6 members accessed | 80 | 104 | 6 × 48 = 288 | **472 bytes** |

These totals exclude the heap-allocated impl instances themselves (Property 48 bytes, Function 48 bytes, Event 80 bytes each) and their control blocks (16 bytes each). The MemberSlot holds a shared_ptr to the instance but does not include its allocation. Per-member data (property value/event, callback pointers) lives in the MemberSlot's 32-byte `MemberData` union rather than on the impl instance.

The `states_` tuple contains one `State` struct per interface that declares properties via `VELK_INTERFACE`. Each `State` struct holds one field per `PROP` member, initialized with its declared default value. Properties backed by state storage use `ext::AnyRef<T>` to read/write directly into these fields.

Static metadata arrays (`MemberDesc`, `InterfaceInfo`) are `constexpr` data shared across all instances at zero per-object cost.

### Common base layers

Every object starts with the same infrastructure. Multiple inheritance adds one vtable pointer per interface chain (MSVC x64). Each `RefCountedDispatch` stores an `ObjectData` struct containing object flags and a `control_block*` for `shared_ptr`/`weak_ptr` support. The control block's `strong` atomic is the authoritative reference count.

The control block comes in two variants: `control_block` (16 bytes: strong + weak + ptr) for IInterface types, and `external_control_block` (24 bytes) which adds a type-erased `destroy` function pointer for non-IInterface types managed by `shared_ptr`.

- **RefCountedDispatch base** (24 bytes): vptr (8) + flags (4) + padding (4) + block* (8)
- **ObjectCore** adds IObject = **32 bytes** base (with 1 extra interface) for Property, Function, and user objects. `get_self()` reconstructs a `shared_ptr` from `control_block::ptr`.
- **AnyBase** skips `self_` and uses a single inheritance chain = **24 bytes** base for Any types

Measured ObjectCore sizes (MSVC x64):

| Configuration | Interfaces | Size |
|---|---|---|
| `ext::ObjectCore<X, I>` (1 interface, IObject prepended) | 2 (IObject + I) | **32 bytes** |
| `ext::ObjectCore<X, IMetadata, IMyWidget, ISerializable>` (IObject in chain) | 3 | **40 bytes** |

### Base types

```
AnyValue<float> (32 bytes)          ArrayAnyValue<float> (48 bytes)
┌────────────────────────────┐      ┌────────────────────────────┐
│ vptr                    8  │      │ vptr                    8  │
│ flags + padding         8  │      │ flags + padding         8  │
│ block*                  8  │      │ block*                  8  │
│ data_ (float) + pad     8  │      │ data_ (vector<float>)  24  │
└────────────────────────────┘      └────────────────────────────┘

ClassId::Property (48 bytes)        ClassId::Function (48 bytes)        ClassId::Event (80 bytes)
┌────────────────────────────┐      ┌────────────────────────────┐      ┌────────────────────────────┐
│ MI base layout          16 │      │ MI base layout          16 │      │ MI base layout          16 │
│   (2 vptrs)                │      │   (2 vptrs)                │      │   (2 vptrs)                │
│ flags + padding          8 │      │ flags + padding          8 │      │ flags + padding          8 │
│ block*                   8 │      │ block*                   8 │      │ block*                   8 │
│ owner_ / standalone_    8* │      │ owner_ / standalone_    8* │      │ owner_ / standalone_    8* │
│ member_index_ + pad      8 │      │ member_index_ + pad      8 │      │ member_index_ + pad      8 │
│   storage_id_, external_   │      │   storage_id_              │      │   storage_id_              │
│ (total verified: 48)       │      │ (total verified: 48)       │      │ handlers_ (vector)      24 │
└────────────────────────────┘      └────────────────────────────┘      │ deferred_begin_ + pad    8 │
                                                                        │ (total verified: 80)       │
* owner_ and standalone_slot_                                           └────────────────────────────┘
  share a union (8 bytes).
  Per-member data (property value,
  callback pointers) lives in
  MemberSlot::MemberData (32-byte
  union in ObjectStorage).

MemberSlot (48 bytes)
┌────────────────────────────┐
│ instance (shared_ptr)   16 │
│ MemberData (union)      32 │
│   property: data + event   │
│   callback: ctx/fn/own/del │
└────────────────────────────┘
```

Internal interface types use inheritance to reduce MI chains: `IPropertyInternal` inherits `IProperty`, `IFunctionInternal` inherits `IEvent` (which inherits `IFunction`), and `IFutureInternal` inherits `IFuture`. This means each impl class only needs one entry in its interface pack (the Internal variant), halving the MI vptr overhead compared to listing both the public and internal interfaces separately.

- **AnyValue** uses a single inheritance chain (`IInterface` → `IObject` → `IAny`), so only one vptr. **ArrayAnyValue** extends the same single chain (`IInterface` → `IObject` → `IAny` → `IArrayAny`), still one vptr. The `control_block*` in `ObjectData` supports `shared_ptr`/`weak_ptr` interop, it is always heap-allocated at construction.
- **`ClassId::Function`** is the lightweight invoke-only implementation. The primary invoke target uses a unified context/function-pointer pair; plain callbacks go through a static trampoline. Owned callbacks (`set_owned_callback`) store heap-allocated context with a type-erased deleter. IEvent methods (`add_handler`, `remove_handler`) are stubs.
- **`ClassId::Event`** extends the same invoke machinery with a partitioned handler list: `[0, deferred_begin_)` for immediate handlers, `[deferred_begin_, size())` for deferred. When no handlers are registered the vector is empty (zero heap allocation).
- **`ClassId::Property`** holds a shared pointer to its backing `IAny` storage, a back-pointer to its owning `IObjectStorage`, and a storage index. Change events are owned by `ObjectStorage` rather than the property itself, so the property carries no per-instance event overhead. The destructor walks the extension chain calling `take_inner()` to break ref cycles when the property is destroyed.
