#pragma once

#include <QDialog>
#include <QImage>

namespace cc {

class I18nService;

/// Interactive, pin-based raster deformation for the selected image layer.
/// The dialog keeps the source image untouched until the caller accepts it.
class PuppetWarpDialog final : public QDialog
{
public:
    explicit PuppetWarpDialog(const QImage& source, I18nService* i18n,
                              QWidget* parent = nullptr);

    QImage resultImage() const;

private:
    class WarpCanvas;
    WarpCanvas* m_canvas = nullptr;
};

} // namespace cc
