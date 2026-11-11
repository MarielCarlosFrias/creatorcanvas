# Third-Party Notices

CreatorCanvas is licensed under the MIT License (see `LICENSE`). It links against and vendors the following third-party components, each under its own license:

## Qt 6 — LGPLv3

CreatorCanvas uses Qt 6 (Core, Gui, Widgets, Test) under the **GNU Lesser General Public License v3 (LGPLv3)**.

- Qt is **dynamically linked**, not statically linked or embedded — this is required to satisfy LGPLv3 without triggering its stronger copyleft/relinking obligations.
- Any binary distribution of CreatorCanvas (installers, AppImage, DEB, portable ZIP) must:
  - Ship the LGPLv3 license text alongside the binaries.
  - Allow the end user to replace the bundled Qt libraries with a compatible version of their own (i.e. do not lock Qt into a form the user cannot swap out).
  - Provide, or link to, the source of the exact Qt version used, per LGPLv3 §4.
- Qt's own license and source are available at <https://www.qt.io/licensing/> and <https://code.qt.io/>.
- A commercial Qt license is a drop-in alternative if the project's distribution model changes in a way that makes LGPLv3 compliance impractical.

## miniz — Public Domain (Unlicense-style)

CreatorCanvas vendors a modular copy of **miniz** under `3rdparty/miniz/` (used for reading/writing the `.creatorcanvas` ZIP container). The base (`miniz.c`: crc32, adler32, default allocators) plus the `tinfl`, `tdef` and `miniz_zip` modules are compiled; miniz's own example program (`miniz_tester.cpp`) is excluded. No attribution or copyleft obligation applies, but the original notice is retained in the vendored source files for provenance.

## ONNX Runtime — MIT

CreatorCanvas links against **ONNX Runtime** (v1.19.0, fetched by `scripts/fetch_onnx.sh` or by the CI workflow) under the **MIT License**, loaded dynamically at runtime (`libonnxruntime.so` / `onnxruntime.dll`; the DEB and AppImage bundle it under `lib/creatorcanvas/`). It provides the local, offline `u2netp` inference used by the AI background-removal feature. Source and license: <https://github.com/microsoft/onnxruntime> and `3rdparty/onnxruntime/LICENSE`.

## Fonts / icons

The application icon (`resources/icons/creatorcanvas.svg`) is original to this project and covered by the MIT License above. If system or bundled fonts are added later, list them here with their individual licenses before distributing binaries.

---

If you add a new dependency, add an entry here describing its license and any distribution obligations (attribution, source availability, dynamic-linking requirements, etc.) before merging.
