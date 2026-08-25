# CreatorCanvas

A fast, layer-based image editor for content creators (YouTube thumbnails,
social posts, banners). Cross-platform: Windows 10/11 and Linux today,
macOS-ready by design.

**Status:** Phase 1 in progress — see milestone table below.

## Milestones (Phase 1 - Core Editor)

| #  | Milestone                | Status |
|----|--------------------------|--------|
| M0 | Application skeleton     | Code complete |
| M1 | Core document/layers     | Code complete |
| M2 | Undo/redo history        | Pending |
| M3 | Project save/open (v1)   | Pending |
| M4 | Canvas view (pan/zoom)   | Pending |
| M5 | New Document dialog      | Pending |
| M6 | Image import + transform | Pending |
| M7 | Layers panel             | Pending |
| M8 | Text tool                | Pending |
| M9 | Shape tool               | Pending |
| M10| Menus, shortcuts, settings dialog | Pending |
| M11| Export (PNG/JPEG/WebP)   | Pending |
| M12| Autosave + recovery      | Pending |
| M13| Start screen             | Pending |
| M14| Polish + packaging       | Pending |

## Prerequisites

- CMake >= 3.24, Ninja (recommended), a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022)
- Qt 6.x (dev builds tolerate 6.2+; reference/deployment toolchain: Qt 6.8 LTS)

## Build and test - Linux

    cmake --preset linux-debug
    cmake --build --preset linux-debug
    ctest --test-dir build/linux-debug --output-on-failure
    ./build/linux-debug/bin/CreatorCanvas

## Build and test - Windows (VS 2022, Qt via online installer)

From a developer prompt:

    cmake --preset windows-debug
    cmake --build --preset windows-debug --config Debug
    ctest --test-dir build/windows-debug -C Debug --output-on-failure

## Sanitizer build (Linux)

    cmake --preset linux-asan
    cmake --build --preset linux-asan
    ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/linux-asan --output-on-failure

## Conventions

- No hardcoded user-facing strings: always resolve via
  I18nService::t(namespace, key); catalogs live in
  resources/locales/<lang>/<ns>.json. Missing keys fall back to English,
  then to the dotted key (logged warning).
- Logging via qInfo/qWarning/...; categories named cc.<area>. Logs rotate
  under the OS app-data directory.
- From M2 onward, ALL document mutation goes through Commands.

## License

Source is proprietary to the project for now. Third-party runtime
components include Qt 6 (LGPLv3 - dynamic linking; notices ship with
installers in M14) and miniz (public domain, vendored at packaging time).
