# ADR 0002: Build System & Dependency Management

## Status
Accepted

## Context
Building native Windows C++ applications with low latency and hardware acceleration requires reproducible compilation across developer machines and automated CI runners.

## Decision
1. **Build System:** CMake 3.25+ with `CMakePresets.json` (schema version 4).
   - Configure presets provide unified settings for Debug and Release across MSVC and Clang.
   - Build presets decouple builds from IDE-specific environment variables.
   - Test presets bind directly to CTest.
2. **Dependency Management:** `vcpkg` manifest mode (`vcpkg.json`).
   - Keeps dependencies versioned and pinned at the repository root.
   - Declares `gtest` for unit testing.
3. **Resilience Fallback:** CMake build files include a fallback via `FetchContent` for GoogleTest when vcpkg is not present, ensuring that tests build seamlessly in bare environments.
4. **Minimal External Dependencies:** Basic logging, configuration parsing, and error tracking are implemented directly in `engine/common` rather than introducing massive third-party frameworks.

## Consequences
- Fast configuration and build times.
- Reproducible builds in both automated CI and local developer setups.
- Zero runtime bloat from heavy third-party logging or configuration dependencies.
