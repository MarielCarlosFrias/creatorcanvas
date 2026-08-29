# Contributing to CreatorCanvas

Thanks for taking a look at CreatorCanvas. This is a young, milestone-driven project — read `DOCUMENTATION.md` first for the architecture and current state before sending a change.

## Ground rules

1. **Every document mutation goes through a `Command`** (`src/core/history/`). If your change touches `Document` state from UI code, add or extend a command instead of calling a setter directly — this is what keeps undo/redo correct.
2. **No hardcoded user-facing strings.** Route everything through `I18nService::t(namespace, key)` and add the key to *both* `resources/locales/en/` and `resources/locales/pt-BR/` in the same PR.
3. **`core/` must not depend on QtWidgets.** Only QtCore/QtGui. This is what makes the core logic unit-testable without a display.
4. **Only `platform/` may contain OS-conditional (`#ifdef _WIN32` etc.) code.**
5. **`std::unique_ptr<T>` members of a `Q_OBJECT` class need a full `#include` of `T`**, not a forward declaration — the compiler-generated destructor needs the complete type. This has broken the build more than once (M12, M13); double-check when adding a new `unique_ptr` member to a header.
6. **Qualify `cc::` free functions explicitly** when calling them from inside a class that defines a member of the same name (e.g. `cc::saveDocument(...)` vs `MainWindow::saveDocument()`), or the member will silently shadow the free function.
7. **Avoid broad `sed`/regex rewrites of source files.** Prefer small, exact, reviewed string replacements so a patch either applies exactly where intended or fails loudly — not a near-miss that silently touches the wrong occurrence.

## Workflow

1. Build and run the existing test suite before you start (`ctest --test-dir build/linux-debug --output-on-failure`) — 16/16 should pass on a clean checkout.
2. Make your change against the smallest reasonable scope. If it's a new feature, check `DOCUMENTATION.md` → Roadmap first — some things are deferred *on purpose*.
3. Add or update a unit test under `tests/unit/` for any behavior change in `core/`, `rendering/`, or `services/`.
4. Run the sanitizer build if you touched ownership/lifetime-sensitive code:
   ```bash
   cmake --preset linux-asan && cmake --build --preset linux-asan
   ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/linux-asan --output-on-failure
   ```
5. Keep commit messages milestone/scope-tagged where relevant (the existing history uses `M<n>: <what shipped>` — follow that pattern for consistency, or use conventional commits for anything outside the milestone track).

## Reporting bugs

Include: your OS/compiler/Qt version, the `cmake --preset` you used, the exact compiler error or runtime behavior, and — if it's a crash — a log excerpt (logs live under the OS app-data directory, see `DOCUMENTATION.md` → Conventions).
