# Contributing to Battery SDK

Thanks for your interest in contributing! This guide will help you get started.

## Quick Start

```bash
# Clone
git clone https://github.com/aliaksandr-liapin/ibattery-sdk.git
cd ibattery-sdk

# Run tests (no hardware or Zephyr needed)
cmake -B build_tests tests
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

## Development Setup

**Host tests** require only a C compiler and CMake (3.20+). They run on macOS and Linux without any embedded toolchain.

**Firmware builds** require the nRF Connect SDK. See the [README](README.md) for details.

## Code Style

- **Indent:** 4 spaces (no tabs in C files)
- **Naming:** `battery_` prefix for all public symbols, `snake_case` everywhere
- **No heap:** All allocations are static. No `malloc`, `calloc`, or `realloc`.
- **Integer math only:** No floating-point in SDK core (HAL may use float for sensor API conversion)
- **Error handling:** Return `battery_status_t` codes. Never silently ignore errors.
- **Headers:** Public API in `include/battery_sdk/`, internal headers co-located with source files

An `.editorconfig` file is included for automatic formatting in most editors.

## Architecture Rules

The SDK follows a strict layered architecture. See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for full details.

**Key rules:**
1. Modules may only call modules in the same layer or the layer directly below
2. All hardware access goes through the HAL — core modules never include Zephyr or nRF headers
3. No circular dependencies between modules
4. Each module has a clear single responsibility

## Adding a New Module

1. Create source file in the appropriate `src/` subdirectory
2. Add public header to `include/battery_sdk/` if the module has a public API
3. Wire into `battery_sdk_init()` in `src/core/battery_sdk.c` if it needs initialization
4. Write unit tests in `tests/test_<module>.c` with Unity
5. Add a mock in `tests/mocks/` if other modules depend on yours
6. Register the test in `tests/CMakeLists.txt`
7. Update documentation: `SDK_API.md`, `ARCHITECTURE.md`, `TESTING.md`

## Adding a Battery Profile (LUT)

See the step-by-step guide in [BATTERY_PROFILES.md](docs/BATTERY_PROFILES.md#adding-a-new-battery-profile).

## Porting to a New Platform

1. Implement the HAL functions in a new `hal/battery_hal_*_<platform>.c` file
2. No changes to core modules should be needed — if they are, the HAL interface is incomplete
3. See the portability section of [ARCHITECTURE.md](docs/ARCHITECTURE.md) for details

## Testing

All changes must pass the host test suite before merging:

```bash
cmake --build build_tests && ctest --test-dir build_tests --output-on-failure
```

**Test expectations:**
- New modules need unit tests
- New LUT entries need interpolation tests covering each curve region
- Bug fixes should include a regression test
- Tests must not depend on hardware, Zephyr, or network access

## Submitting Changes

1. Fork the repository and create a feature branch
2. Make your changes following the style and architecture rules above
3. Ensure all tests pass
4. Update documentation if you changed public APIs or test counts
5. Open a pull request with a clear description of what changed and why

## Reporting Issues

Use the [GitHub issue templates](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/new/choose):
- **Bug report** — for incorrect behavior, build failures, or test failures
- **Feature request** — for new capabilities or improvements

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](LICENSE).
