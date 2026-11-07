# ADR 0001: Language Standard & Compiler Selection

## Status
Accepted

## Context
Mist requires predictable, deterministic execution, low CPU overhead, zero garbage collection pauses, and access to low-level Windows graphics and hardware encoding APIs (Direct3D 11/12, DXGI, NVENC, WASAPI). 

## Decision
1. **Language Standard:** C++20 (`cxx_std_20`). C++20 provides modern features such as `std::span`, concepts, `<chrono>` enhancements, structured binding improvements, and `<bit>` utilities, while maintaining mature compiler support across MSVC and Clang.
2. **Standard Compatibility Guard:** Avoid features exclusive to C++23 (such as `std::expected` or `std::print`) to ensure immediate compatibility with current stable toolchains and enterprise build environments.
3. **Primary Compiler:** Microsoft Visual C++ (MSVC 2022 v143+) for official Windows production and CI builds.
4. **Secondary Compiler:** LLVM Clang++ (17+) with UCRT for high-performance static analysis, fast developer turnaround, and cross-toolchain sanity checks.
5. **Warning Policy:** Treat warnings as errors (`/WX` on MSVC, `-Werror` on Clang) for all project-owned code, while isolating third-party dependencies from breaking warning rules.

## Consequences
- Guaranteed zero-overhead modern language constructs.
- Deterministic memory ownership via RAII.
- High code cleanliness enforced by strict compiler diagnostics.
