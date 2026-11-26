#pragma once

#include "services/ImageResizer.h"

#include <QDialog>
#include <QSize>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;

namespace cc {

class Document;
class I18nService;

/// Diálogo de Image Size/Resample. Ao contrário de Canvas Size, devolve um
/// pedido de reamostragem e não contém âncora nem offset de camadas.
class ImageSizeDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit ImageSizeDialog(const Document* document,
                             I18nService* i18n = nullptr,
                             QWidget* parent = nullptr);
    explicit ImageSizeDialog(const Document& document,
                             I18nService* i18n = nullptr,
                             QWidget* parent = nullptr);

    QSize imageSize() const;
    ResampleMethod resampleMethod() const;
    ImageResizeOptions options() const;

private:
    void buildUi();
    void retranslateUi();
    void widthChanged(int value);
    void heightChanged(int value);
    void updateScaleLabel();
    QString text(const QString& key, const QString& fallback) const;

    const Document* m_document = nullptr;
    I18nService* m_i18n = nullptr;
    double m_aspectRatio = 1.0;
    bool m_updatingDimensions = false;

    QLabel* m_currentSizeLabel = nullptr;
    QLabel* m_widthLabel = nullptr;
    QLabel* m_heightLabel = nullptr;
    QLabel* m_resampleLabel = nullptr;
    QLabel* m_scaleLabel = nullptr;
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
    QCheckBox* m_constrainProportions = nullptr;
    QComboBox* m_resample = nullptr;
};

} // namespace cc
