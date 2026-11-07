# ADR 0004: Windows Platform Isolation Layer

## Status
Accepted

## Context
Directly including `<windows.h>` across engine headers pollutes the global namespace with thousands of macros (e.g. `min`, `max`, `ERROR`, `DELETE`), causes naming collisions, and slows down compile times.

## Decision
1. **Platform Isolation Library:** Encapsulate all native Windows calls within `platform/windows` (`mist_platform_windows`).
2. **Lean Header Inclusion:** In `.cpp` files requiring Windows API calls, always define `WIN32_LEAN_AND_MEAN` and `NOMINMAX` before including `<windows.h>`.
3. **Dynamic API Resolution:** For APIs that may vary across Windows SDK versions (such as `SetProcessDpiAwarenessContext`), resolve function pointers dynamically via `GetProcAddress` on `user32.dll` with safe fallbacks.
4. **Typed Error Translation:** Never expose raw `DWORD` error codes or unstructured HRESULTs across engine interfaces. Provide explicit translation helpers:
   - `win32ErrorToMistError`
   - `hresultToMistError`
5. **High-Resolution Timing:** Abstract Windows `QueryPerformanceCounter` and `QueryPerformanceFrequency` behind a clean, RAII-based `HighResolutionTimer` class.

## Consequences
- Engine headers remain free of Windows macro pollution.
- Fast, clean compilation.
- Seamless debugging with meaningful typed error messages.
