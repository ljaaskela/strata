---
name: velk
description: "Velk C++ component object model: VELK_INTERFACE macro, Object<T> CRTP, properties, events, functions, bindings, interface_cast, type registration, create objects, IMetadata, StateReader, StateWriter"
---

# Velk Usage Guide

Velk is a C++17 component object model shipped as a shared DLL. Consumers depend only on public headers in `velk/include/velk/`. Objects are composed from pure virtual interfaces, implemented via CRTP, registered with a global type registry, and created through a factory. All values are type-erased through `IAny`.

## Object Lifecycle (5 steps)

1. **Define interface** with `VELK_INTERFACE` (generates static metadata, State struct, typed accessors)
2. **Implement** with `ext::Object<MyClass, IFace1, IFace2, ...>` (collects metadata from all interfaces)
3. **Register** with `instance().type_registry().register_type<MyClass>()`
4. **Create** with `instance().create<IObject>(MyClass::class_id())`
5. **Use** via `interface_cast<IMyInterface>(obj)->member()` or `velk::Object` wrapper

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

- **Interfaces** must inherit from `Interface<T>` (or `Interface<T, Base>` for interface inheritance)
- **Implementations** must inherit from `ext::Object<T, Interfaces...>`
- **Register before create**: call `register_type<T>()` before any `create<>()`
- **Casting**: use `interface_cast<T>(obj)` for raw pointers, `interface_pointer_cast<T>(ptr)` for `shared_ptr`
- **Name qualification**: when `using namespace velk`, qualify `::velk::Object`, `::velk::invoke_function`, etc. to avoid ambiguity with `velk::velk` (the `velk` name exists inside the namespace)
- **No DLL exports**: never export functions from the DLL. Use `instance()` as the single entry point
- **FN overrides** use `fn_` prefix: `(FN, void, reset)` generates virtual `fn_reset()` to override
- **FN_RAW overrides** have signature `IAny::Ptr fn_Name(FnArgs)`
- **Events** conventionally use `on_` prefix: `(EVT, on_clicked)`
- **Deferred updates**: pass `Deferred` to `set_value()` or `write_state()`, then call `instance().update()` to flush
- **StateWriter fires on_changed** when it destructs, so use RAII scoping
- **Bindings** must be kept alive; when the `Binding` wrapper destructs, the binding is NOT automatically removed. Call `binding.remove()` to uninstall.

## Detailed Examples

See [patterns.md](patterns.md) for complete code examples covering interface definition, object implementation, property access, functions, events, bindings, `Any<T>`, state access, and testing patterns.

## Distribution

This skill directory (`.claude/skills/velk/`) can be copied alongside Velk headers into consumer projects so that Claude Code provides correct guidance automatically.
