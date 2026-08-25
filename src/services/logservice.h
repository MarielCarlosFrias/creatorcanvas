#pragma once

#include <QString>
#include <QtGlobal>

namespace cc {

/// Global Qt message handler writing timestamped entries to a rotating log
/// file (<dir>/creatorcanvas.log) and mirroring them to stderr. Thread-safe.
///
/// Rotation policy: checked at init() only. If the previous session's log
/// exceeds maxFileBytes it is shifted to creatorcanvas.log.1 .. .N (oldest
/// dropped). In-session growth is rotated on the next launch.
class LogService final
{
public:
    /// Installs the handler. Subsequent calls are ignored.
    static void init(const QString &directory,
                     qint64 maxFileBytes = 2 * 1024 * 1024,
                     int backupCount = 5);

    /// Flushes, closes, uninstalls the handler.
    static void shutdown();

private:
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext &context,
                               const QString &message);
};

} // namespace cc
