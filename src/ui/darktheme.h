#pragma once

class QApplication;

namespace cc {

/// Applies the application's identity theme: Fusion style + dark palette.
/// Called once from main() before any widget exists.
void applyDarkTheme(QApplication &app);

} // namespace cc
