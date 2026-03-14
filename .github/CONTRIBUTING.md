# Contributing to Velk

Thank you for your interest in contributing to Velk!

## License Agreement

By submitting a pull request, you agree that your contributions are licensed under the same [MIT License](../LICENSE) that covers this project. This means your contributions will be freely available under the same terms as the rest of the codebase ("inbound = outbound").

## Getting Started

### Prerequisites

- CMake 3.14+
- A C++17 compiler (tested with MSVC, GCC and Clang)

### Building

```bash
# Configure
cmake -B build

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build --build-config Release
```

## Submitting Changes

1. Fork the repository
2. Create a feature branch from `main`
3. Make your changes
4. Ensure the project builds and tests pass
5. Open a pull request against `main`
