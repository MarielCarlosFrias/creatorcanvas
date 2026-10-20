#pragma once

#include <QDialog>
#include <QUuid>
#include <QImage>

class QComboBox;
class QLabel;
class QSpinBox;
class QRadioButton;
class QButtonGroup;
class QTimer;

namespace cc {

class Document;
class I18nService;

struct ExportSettings
{
    QString format;   // "png" | "jpeg" | "webp"
    int quality = 95;
    double scale = 1.0;
    bool exportSelectedLayerOnly = false;
    QUuid selectedLayerId;
};

/// Modernized export dialog: platform presets, preview with checkerboard,
/// real-time estimated file size, output resolution, and selection scope.
class ExportDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(const Document* doc,
                          I18nService* i18n,
                          const QUuid& selectedLayerId = {},
                          QWidget* parent = nullptr);

    ExportSettings settings() const;

private:
    void buildUi();
    void retranslateUi();
    void updateQualityEnabled();
    void updatePreset();
    void updatePreview();
    void estimateFileSize();

    const Document* m_doc = nullptr;
    I18nService* m_i18n = nullptr;
    QUuid m_selectedLayerId;

    // Controls
    QComboBox* m_presetCombo = nullptr;
    QComboBox* m_format = nullptr;
    QComboBox* m_scale = nullptr;
    QSpinBox* m_quality = nullptr;
    QRadioButton* m_scopeEntire = nullptr;
    QRadioButton* m_scopeLayer = nullptr;
    QButtonGroup* m_scopeGroup = nullptr;

    // Labels & Information
    QLabel* m_formatLabel = nullptr;
    QLabel* m_qualityLabel = nullptr;
    QLabel* m_scaleLabel = nullptr;
    QLabel* m_presetLabel = nullptr;
    QLabel* m_scopeLabel = nullptr;
    QLabel* m_dimensionLabel = nullptr;
    QLabel* m_fileSizeLabel = nullptr;
    QLabel* m_previewImageLabel = nullptr;

    // Cached rendered preview thumbnail
    QImage m_cachedBaseThumbnail;

    // Debounce timer for preview re-render (avoid heavy continuous renders)
    QTimer* m_previewDebounceTimer = nullptr;
};

} // namespace cc
