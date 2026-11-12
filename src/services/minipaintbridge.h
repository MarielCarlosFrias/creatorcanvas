#pragma once

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QString>
#include <QImage>

#include <memory>

#include "core/layers/Layer.h"

namespace cc {

class Document;

/// Bridge between CreatorCanvas and the MiniPaint standalone editor.
///
/// MiniPaint is a separate SFML 3 process (incompatible GUI/event/context
/// lifecycle with Qt). Communication happens via PNG file handoff:
///   1. Export the selected image layer's bitmap to a temp PNG
///   2. Launch `minipaint-standalone --input <in.png> --output <out.png>`
///      with optional tool/color/brush overrides
///   3. Wait for the process to exit
///   4. On success, import the output PNG as a new asset and replace the
///      layer's asset through one undoable command
///
/// On cancel (exit code 2) or failure, the document is left unchanged.
class MiniPaintBridge final : public QObject
{
    Q_OBJECT

public:
    explicit MiniPaintBridge(QObject* parent = nullptr);
    ~MiniPaintBridge() override = default;

    MiniPaintBridge(const MiniPaintBridge&) = delete;
    MiniPaintBridge& operator=(const MiniPaintBridge&) = delete;

    /// Returns true if the MiniPaint standalone executable was found.
    [[nodiscard]] bool isAvailable() const;

    /// Returns the resolved path to the mini paint executable (or empty).
    [[nodiscard]] QString executablePath() const;

    /// Launches MiniPaint to edit the bitmap of the given image layer.
    /// The caller (MainWindow) is responsible for pushing an undo command
    /// with the returned image on success.
    ///
    /// @param layer         Image layer to edit (must be non-null, unlocked)
    /// @param document      Owning document (for asset store access)
    /// @param initialTool   Initial tool to select in MiniPaint
    /// @param brushSize     Initial brush diameter in pixels
    /// @return QImage result on success, null QImage on cancel/failure
    QImage editLayer(ImageLayer* layer,
                     Document* document,
                     QString initialTool = QStringLiteral("pencil"),
                     float brushSize = 3.0F);

signals:
    /// Emitted when MiniPaint starts/stops being available.
    void availabilityChanged(bool available);

private:
    /// Resolves the mini paint executable path beside the app bundle or in
    /// PATH. On Windows looks for minipaint-standalone.exe.
    QString resolveExecutable() const;

    QString m_executablePath;
    bool m_available{false};
};

} // namespace cc
